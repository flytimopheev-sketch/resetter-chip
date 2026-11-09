#include "Ch341AI2cAdapter.h"

#include <QThread>

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace {
constexpr uint16_t kCh341VendorId = 0x1A86;
constexpr uint16_t kCh341ProductId = 0x5512;
constexpr uint8_t kEpOut = 0x02;
constexpr uint8_t kEpIn = 0x82;

// Ведущая команда режима I2C
constexpr uint8_t kCmdI2cStream = 0xAA;
// Подкоманды потока
constexpr uint8_t kStmStart = 0x74;
constexpr uint8_t kStmStop = 0x75;
constexpr uint8_t kStmOut = 0x80;   // 0x80 | n  — записать n байт
constexpr uint8_t kStmIn = 0xC0;    // 0xC0 | (n-1) — прочитать n байт
constexpr uint8_t kStmSet = 0x60;   // 0x60 | speed
constexpr uint8_t kStmEnd = 0x00;   // конец пакета

// Скорость шины: 0 = 20 кГц, 1 = 100 кГц, 2 = 400 кГц, 3 = 750 кГц
constexpr uint8_t kI2cSpeed100k = 1;

// Бит статуса NAK/ошибки в ответе устройства
constexpr uint8_t kStatusNak = 0x80;

constexpr int kUsbTimeoutMs = 1000;
constexpr size_t kMaxPacket = 64;
constexpr size_t kMaxChunkWrite = 28;  // ограничение одного пакета CH341A
constexpr size_t kMaxChunkRead = 29;
}

Ch341AI2cAdapter::Ch341AI2cAdapter() = default;

Ch341AI2cAdapter::~Ch341AI2cAdapter()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    closeUnlocked();
}

void Ch341AI2cAdapter::setError(const std::string &message)
{
    m_lastError = message;
}

bool Ch341AI2cAdapter::open()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_opened && m_handle) {
        return true;
    }

    if (!m_usbCtx) {
        int rc = libusb_init(&m_usbCtx);
        if (rc < 0) {
            setError(std::string("Ошибка инициализации libusb: ") + libusb_error_name(rc));
            m_usbCtx = nullptr;
            return false;
        }
        libusb_set_option(m_usbCtx, LIBUSB_OPTION_LOG_LEVEL, LIBUSB_LOG_LEVEL_WARNING);
    }

    m_handle = libusb_open_device_with_vid_pid(m_usbCtx, kCh341VendorId, kCh341ProductId);
    if (!m_handle) {
        setError("Программатор CH341A не найден (VID=1A86, PID=5512). Проверьте подключение и права доступа.");
        return false;
    }

    if (libusb_kernel_driver_active(m_handle, 0) == 1) {
        libusb_detach_kernel_driver(m_handle, 0);
    }

    int rc = libusb_claim_interface(m_handle, 0);
    if (rc < 0) {
        setError(std::string("Не удалось захватить интерфейс USB: ") + libusb_error_name(rc));
        libusb_close(m_handle);
        m_handle = nullptr;
        return false;
    }

    if (!configureI2cUnlocked()) {
        libusb_release_interface(m_handle, 0);
        libusb_close(m_handle);
        m_handle = nullptr;
        return false;
    }

    m_opened = true;
    m_lastError.clear();
    return true;
}

void Ch341AI2cAdapter::closeUnlocked()
{
    if (m_handle) {
        libusb_release_interface(m_handle, 0);
        libusb_close(m_handle);
        m_handle = nullptr;
    }
    if (m_usbCtx) {
        libusb_exit(m_usbCtx);
        m_usbCtx = nullptr;
    }
    m_opened = false;
}

void Ch341AI2cAdapter::close()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    closeUnlocked();
}

bool Ch341AI2cAdapter::isOpen() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_opened && m_handle != nullptr;
}

void Ch341AI2cAdapter::setChipParams(const ChipParams &params)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_params = params;
}

bool Ch341AI2cAdapter::configureI2cUnlocked()
{
    // Установить скорость шины 100 кГц
    std::vector<uint8_t> packet = {kCmdI2cStream,
                                   static_cast<uint8_t>(kStmSet | kI2cSpeed100k),
                                   kStmEnd};

    int transferred = 0;
    int rc = libusb_bulk_transfer(m_handle, kEpOut, packet.data(),
                                  static_cast<int>(packet.size()), &transferred, kUsbTimeoutMs);
    if (rc < 0) {
        setError(std::string("Не удалось настроить I2C-режим CH341A: ") + libusb_error_name(rc));
        return false;
    }
    return true;
}

bool Ch341AI2cAdapter::streamUnlocked(const std::vector<uint8_t> &packet,
                                      size_t readLength,
                                      std::vector<uint8_t> &out)
{
    out.clear();

    int transferred = 0;
    int rc = libusb_bulk_transfer(m_handle, kEpOut,
                                  const_cast<uint8_t *>(packet.data()),
                                  static_cast<int>(packet.size()), &transferred, kUsbTimeoutMs);
    if (rc < 0) {
        setError(std::string("Ошибка USB-передачи (OUT): ") + libusb_error_name(rc));
        return false;
    }

    // Ответ устройства: байт статуса (бит 0x80 = NAK), затем данные (для чтения)
    std::vector<uint8_t> response(readLength + 1, 0);
    rc = libusb_bulk_transfer(m_handle, kEpIn, response.data(),
                              static_cast<int>(response.size()), &transferred, kUsbTimeoutMs);

    if (rc == LIBUSB_ERROR_TIMEOUT) {
        setError("Чип не отвечает по шине I2C (таймаут)");
        return false;
    }
    if (rc < 0) {
        setError(std::string("Ошибка USB-передачи (IN): ") + libusb_error_name(rc));
        return false;
    }
    if (transferred < 1) {
        setError("Пустой ответ программатора");
        return false;
    }
    if ((response[0] & kStatusNak) != 0) {
        setError("Чип не подтвердил транзакцию (NAK). Проверьте адрес и подключение.");
        return false;
    }

    if (readLength > 0) {
        size_t available = static_cast<size_t>(transferred) - 1;
        size_t toCopy = std::min(readLength, available);
        if (toCopy == 0) {
            setError("Программатор не вернул данные чтения");
            return false;
        }
        out.assign(response.begin() + 1, response.begin() + 1 + toCopy);
        if (toCopy < readLength) {
            setError("Прочитано меньше байт, чем запрошено");
            return false;
        }
    }

    return true;
}

uint8_t Ch341AI2cAdapter::deviceAddressByte(uint32_t address, bool readOp) const
{
    uint8_t addr = static_cast<uint8_t>((m_params.deviceAddress << 1) & 0xFE);

    // Для 24C04/08/16 старшие биты адреса передаются в байте устройства
    if (m_params.blockBits > 0) {
        uint8_t block = static_cast<uint8_t>((address >> 8) & ((1u << m_params.blockBits) - 1u));
        addr = static_cast<uint8_t>(addr | (block << 1));
    }

    if (readOp) {
        addr = static_cast<uint8_t>(addr | 0x01);
    }
    return addr;
}

std::vector<uint8_t> Ch341AI2cAdapter::memoryAddressBytes(uint32_t address) const
{
    if (m_params.twoByteAddress) {
        return {static_cast<uint8_t>((address >> 8) & 0xFF),
                static_cast<uint8_t>(address & 0xFF)};
    }
    return {static_cast<uint8_t>(address & 0xFF)};
}

bool Ch341AI2cAdapter::writeChunkUnlocked(uint32_t address, const uint8_t *data, size_t length)
{
    if (length == 0) {
        return true;
    }

    std::vector<uint8_t> memAddr = memoryAddressBytes(address);
    const size_t payloadBytes = memAddr.size() + length;
    if (payloadBytes > kMaxChunkWrite) {
        setError("Слишком длинная транзакция записи для CH341A");
        return false;
    }

    std::vector<uint8_t> packet;
    packet.reserve(payloadBytes + 5);
    packet.push_back(kCmdI2cStream);
    packet.push_back(kStmStart);
    packet.push_back(static_cast<uint8_t>(kStmOut | payloadBytes));
    packet.push_back(deviceAddressByte(address, false));
    packet.insert(packet.end(), memAddr.begin(), memAddr.end());
    packet.insert(packet.end(), data, data + length);
    packet.push_back(kStmStop);
    packet.push_back(kStmEnd);

    std::vector<uint8_t> ignored;
    if (!streamUnlocked(packet, 0, ignored)) {
        return false;
    }

    // Ожидание цикла записи EEPROM (внутренняя запись страницы)
    const int cycleMs = m_params.writeCycleMs > 0 ? static_cast<int>(m_params.writeCycleMs) : 5;
    QThread::msleep(static_cast<unsigned long>(std::max(2, cycleMs)));
    return true;
}

bool Ch341AI2cAdapter::readChunkUnlocked(uint32_t address, uint8_t *dst, size_t length)
{
    if (length == 0) {
        return true;
    }

    std::vector<uint8_t> memAddr = memoryAddressBytes(address);
    const size_t addrPayload = memAddr.size();

    std::vector<uint8_t> packet;
    packet.reserve(addrPayload + 8);
    packet.push_back(kCmdI2cStream);
    packet.push_back(kStmStart);
    packet.push_back(static_cast<uint8_t>(kStmOut | addrPayload));
    packet.insert(packet.end(), memAddr.begin(), memAddr.end());
    packet.push_back(kStmStart);
    packet.push_back(static_cast<uint8_t>(kStmOut | 1));
    packet.push_back(deviceAddressByte(address, true));
    packet.push_back(static_cast<uint8_t>(kStmIn | static_cast<uint8_t>(length - 1)));
    packet.push_back(kStmStop);
    packet.push_back(kStmEnd);

    std::vector<uint8_t> data;
    if (!streamUnlocked(packet, length, data)) {
        return false;
    }
    if (data.size() != length) {
        setError("Некорректная длина данных при чтении");
        return false;
    }

    std::memcpy(dst, data.data(), length);
    return true;
}

std::vector<uint8_t> Ch341AI2cAdapter::read(uint32_t address, uint32_t length)
{
    std::vector<uint8_t> result;

    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_opened || !m_handle) {
        setError("Программатор не открыт");
        return result;
    }
    if (length == 0) {
        return result;
    }

    result.resize(length);

    uint32_t offset = 0;
    while (offset < length) {
        const uint32_t remaining = length - offset;
        const size_t chunk = std::min(static_cast<size_t>(remaining), kMaxChunkRead);

        if (!readChunkUnlocked(address + offset, result.data() + offset, chunk)) {
            result.clear();
            return result;
        }
        offset += static_cast<uint32_t>(chunk);
    }

    m_lastError.clear();
    return result;
}

bool Ch341AI2cAdapter::write(uint32_t address, const std::vector<uint8_t> &data)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_opened || !m_handle) {
        setError("Программатор не открыт");
        return false;
    }
    if (data.empty()) {
        return true;
    }

    const size_t pageSize = m_params.pageSize > 0 ? m_params.pageSize : 16;

    size_t offset = 0;
    while (offset < data.size()) {
        const uint32_t currentAddress = address + static_cast<uint32_t>(offset);

        // Не пересекать границу страницы и предел пакета
        const size_t pageRemaining = pageSize - (currentAddress % pageSize);
        const size_t memAddrSize = memoryAddressBytes(currentAddress).size();
        const size_t packetRoom = (memAddrSize < kMaxChunkWrite) ? (kMaxChunkWrite - memAddrSize) : 0;

        if (packetRoom == 0) {
            setError("Некорректный размер страницы в профиле чипа");
            return false;
        }

        size_t chunk = std::min<size_t>(data.size() - offset, pageRemaining);
        chunk = std::min(chunk, packetRoom);

        if (!writeChunkUnlocked(currentAddress, data.data() + offset, chunk)) {
            return false;
        }
        offset += chunk;
    }

    m_lastError.clear();
    return true;
}

std::vector<uint8_t> Ch341AI2cAdapter::scanBus()
{
    std::vector<uint8_t> found;

    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_opened || !m_handle) {
        setError("Программатор не открыт");
        return found;
    }

    const ChipParams saved = m_params;

    for (uint8_t addr = 0x08; addr <= 0x77; ++addr) {
        m_params.deviceAddress = addr;

        // Probe: START + адрес + STOP. NAK означает отсутствие устройства.
        std::vector<uint8_t> packet = {kCmdI2cStream, kStmStart,
                                       static_cast<uint8_t>(kStmOut | 1),
                                       static_cast<uint8_t>((addr << 1) & 0xFE),
                                       kStmStop, kStmEnd};
        std::vector<uint8_t> ignored;
        if (streamUnlocked(packet, 0, ignored)) {
            found.push_back(addr);
        }
    }

    m_params = saved;
    m_lastError.clear();
    return found;
}

std::string Ch341AI2cAdapter::lastError() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lastError;
}

QString Ch341AI2cAdapter::adapterName() const
{
    return QStringLiteral("CH341A (I2C)");
}
