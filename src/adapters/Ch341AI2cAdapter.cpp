#include "Ch341AI2cAdapter.h"

#include <QElapsedTimer>
#include <QString>
#include <QStringList>
#include <QThread>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {
constexpr uint16_t kCh341VendorId = 0x1A86;
constexpr std::array<uint16_t, 4> kKnownProductIds = {0x5512, 0x5523, 0x5535, 0x5572};

constexpr uint8_t kEpOut = 0x02;
constexpr uint8_t kEpIn = 0x82;
constexpr int kUsbTimeoutMs = 1000;

// CH341A использует 32-байтовые USB-пакеты. Оставляем запас на служебные
// байты пакета I2C stream: AA, START, OUT, STOP, END.
constexpr size_t kMaxPacketSize = 32;
constexpr size_t kPacketOverhead = 5;

constexpr size_t kMaxChunkRead = 29;
constexpr int kReadAttempts = 3;
constexpr int kWriteAttempts = 2;
} // namespace

Ch341AI2cAdapter::Ch341AI2cAdapter(uint16_t vendorId, uint16_t productId)
    : m_vendorId(vendorId)
    , m_productId(productId)
{
}

Ch341AI2cAdapter::~Ch341AI2cAdapter()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    closeUnlocked();
}

void Ch341AI2cAdapter::setError(const std::string &message)
{
    m_lastError = message;
}

Ch341Addressing Ch341AI2cAdapter::addressing() const
{
    Ch341Addressing a;
    a.deviceAddress = m_params.deviceAddress;
    a.twoByteAddress = m_params.twoByteAddress;
    a.blockBits = m_params.blockBits;
    return a;
}

bool Ch341AI2cAdapter::open()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_opened && m_handle) {
        return true;
    }

    if (!m_usbCtx) {
        const int rc = libusb_init(&m_usbCtx);
        if (rc < 0) {
            setError(std::string("Ошибка инициализации libusb: ") + libusb_error_name(rc));
            m_usbCtx = nullptr;
            return false;
        }
        libusb_set_option(m_usbCtx, LIBUSB_OPTION_LOG_LEVEL, LIBUSB_LOG_LEVEL_WARNING);
    }

    struct Candidate {
        uint16_t vid;
        uint16_t pid;
    };
    std::vector<Candidate> candidates;

    if (m_vendorId != 0) {
        if (m_productId != 0) {
            candidates.push_back({m_vendorId, m_productId});
        } else {
            for (uint16_t pid : kKnownProductIds) {
                candidates.push_back({m_vendorId, pid});
            }
        }
    } else {
        for (uint16_t pid : kKnownProductIds) {
            candidates.push_back({kCh341VendorId, pid});
        }
    }

    QStringList tried;
    for (const Candidate &candidate : candidates) {
        m_handle = libusb_open_device_with_vid_pid(m_usbCtx, candidate.vid, candidate.pid);
        if (m_handle) {
            m_openedVendorId = candidate.vid;
            m_openedProductId = candidate.pid;
            break;
        }
        tried << QStringLiteral("%1:%2")
                     .arg(candidate.vid, 4, 16, QLatin1Char('0'))
                     .arg(candidate.pid, 4, 16, QLatin1Char('0'));
    }

    if (!m_handle) {
        setError(QStringLiteral("Программатор не найден. Проверены VID:PID: %1. "
                                "Проверьте подключение и права доступа.")
                     .arg(tried.join(QStringLiteral(", ")))
                     .toStdString());
        return false;
    }

    if (libusb_kernel_driver_active(m_handle, 0) == 1) {
        if (libusb_detach_kernel_driver(m_handle, 0) == 0) {
            m_kernelDriverDetached = true;
        }
    }

    const int rc = libusb_claim_interface(m_handle, 0);
    if (rc < 0) {
        setError(std::string("Не удалось захватить интерфейс USB: ") + libusb_error_name(rc));
        if (m_kernelDriverDetached) {
            libusb_attach_kernel_driver(m_handle, 0);
        }
        libusb_close(m_handle);
        m_handle = nullptr;
        m_kernelDriverDetached = false;
        return false;
    }

    if (!configureI2cUnlocked()) {
        libusb_release_interface(m_handle, 0);
        if (m_kernelDriverDetached) {
            libusb_attach_kernel_driver(m_handle, 0);
        }
        libusb_close(m_handle);
        m_handle = nullptr;
        m_kernelDriverDetached = false;
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
        if (m_kernelDriverDetached) {
            libusb_attach_kernel_driver(m_handle, 0);
        }
        libusb_close(m_handle);
        m_handle = nullptr;
    }
    if (m_usbCtx) {
        libusb_exit(m_usbCtx);
        m_usbCtx = nullptr;
    }
    m_kernelDriverDetached = false;
    m_opened = false;
    m_openedVendorId = 0;
    m_openedProductId = 0;
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
    const std::vector<uint8_t> packet = Ch341Protocol::buildConfigure();

    int transferred = 0;
    const int rc = libusb_bulk_transfer(m_handle, kEpOut, const_cast<uint8_t *>(packet.data()),
                                        static_cast<int>(packet.size()), &transferred,
                                        kUsbTimeoutMs);
    if (rc < 0) {
        setError(std::string("Не удалось настроить I2C-режим CH341A: ") + libusb_error_name(rc));
        return false;
    }
    if (transferred != static_cast<int>(packet.size())) {
        setError("CH341A принял неполный пакет настройки I2C");
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
    int rc = libusb_bulk_transfer(m_handle, kEpOut, const_cast<uint8_t *>(packet.data()),
                                  static_cast<int>(packet.size()), &transferred, kUsbTimeoutMs);
    if (rc < 0) {
        setError(std::string("Ошибка USB-передачи (OUT): ") + libusb_error_name(rc));
        return false;
    }
    if (transferred != static_cast<int>(packet.size())) {
        setError("CH341A принял неполный пакет");
        return false;
    }

    // Ответ устройства: байт статуса (бит 0x80 = NAK), затем данные чтения.
    std::vector<uint8_t> response(readLength + 1, 0);
    rc = libusb_bulk_transfer(m_handle, kEpIn, response.data(),
                              static_cast<int>(response.size()), &transferred, kUsbTimeoutMs);

    if (rc == LIBUSB_ERROR_TIMEOUT && readLength == 0) {
        // Некоторые совместимые платы не возвращают статус на write-only
        // пакеты. Для записи это не ошибка: NAK будет виден по верификации.
        m_lastError.clear();
        return true;
    }
    if (rc == LIBUSB_ERROR_TIMEOUT) {
        setError("Чип не отвечает по шине I2C (таймаут)");
        return false;
    }
    if (rc < 0) {
        setError(std::string("Ошибка USB-передачи (IN): ") + libusb_error_name(rc));
        return false;
    }
    if (transferred < 1) {
        if (readLength == 0) {
            m_lastError.clear();
            return true;
        }
        setError("Пустой ответ программатора");
        return false;
    }
    if ((response[0] & Ch341Protocol::kStatusNak) != 0) {
        setError("Чип не подтвердил транзакцию (NAK). Проверьте адрес и подключение.");
        return false;
    }

    if (readLength > 0) {
        const size_t available = static_cast<size_t>(transferred) - 1;
        const size_t toCopy = std::min(readLength, available);
        if (toCopy < readLength) {
            setError("Прочитано меньше байт, чем запрошено");
            return false;
        }
        out.assign(response.begin() + 1, response.begin() + 1 + toCopy);
    }

    return true;
}

bool Ch341AI2cAdapter::probeUnlocked(uint32_t address)
{
    std::vector<uint8_t> ignored;
    return streamUnlocked(Ch341Protocol::buildProbe(addressing(), address), 0, ignored);
}

void Ch341AI2cAdapter::recoverBusUnlocked()
{
    std::vector<uint8_t> ignored;
    const std::string savedError = m_lastError;
    streamUnlocked(Ch341Protocol::buildBusRecovery(), 0, ignored);
    m_lastError = savedError;
}

void Ch341AI2cAdapter::waitForWriteCycleUnlocked()
{
    const int minimum = m_params.writeCycleMs > 0
                            ? static_cast<int>(m_params.writeCycleMs)
                            : 5;
    QThread::msleep(static_cast<unsigned long>(std::max(2, minimum)));

    // ACK-polling: большинство EEPROM заканчивают внутренний цикл записи
    // раньше максимального времени. Ждём не дольше 100 мс, но не считаем
    // отсутствие ответа ошибкой — верификация всё равно проверит данные.
    const int timeoutMs = std::max(100, minimum * 10);
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs) {
        if (probeUnlocked(0)) {
            break;
        }
        QThread::msleep(1);
    }
    m_lastError.clear();
}

bool Ch341AI2cAdapter::writeChunkUnlocked(uint32_t address, const uint8_t *data, size_t length)
{
    if (length == 0) {
        return true;
    }

    const std::vector<uint8_t> packet =
        Ch341Protocol::buildWrite(addressing(), address, data, static_cast<uint16_t>(length));

    std::vector<uint8_t> ignored;
    if (!streamUnlocked(packet, 0, ignored)) {
        return false;
    }

    waitForWriteCycleUnlocked();
    return true;
}

bool Ch341AI2cAdapter::readChunkUnlocked(uint32_t address, uint8_t *dst, size_t length)
{
    if (length == 0) {
        return true;
    }

    std::vector<uint8_t> data;
    if (!streamUnlocked(Ch341Protocol::buildRead(addressing(), address,
                                                 static_cast<uint16_t>(length)),
                        length, data)) {
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

        bool ok = false;
        for (int attempt = 0; attempt < kReadAttempts && !ok; ++attempt) {
            ok = readChunkUnlocked(address + offset, result.data() + offset, chunk);
            if (!ok) {
                recoverBusUnlocked();
                QThread::msleep(2);
            }
        }

        if (!ok) {
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

        // Не пересекать границу страницы.
        const size_t pageRemaining = pageSize - (currentAddress % pageSize);
        const size_t memAddrSize = Ch341Protocol::memoryAddressBytes(addressing(), currentAddress).size();
        const size_t packetPayloadRoom = kMaxPacketSize - kPacketOverhead - 1 - memAddrSize;

        if (packetPayloadRoom == 0) {
            setError("Некорректный размер страницы в профиле чипа");
            return false;
        }

        size_t chunk = std::min<size_t>(data.size() - offset, pageRemaining);
        chunk = std::min(chunk, packetPayloadRoom);

        bool ok = false;
        for (int attempt = 0; attempt < kWriteAttempts && !ok; ++attempt) {
            ok = writeChunkUnlocked(currentAddress, data.data() + offset, chunk);
            if (!ok) {
                recoverBusUnlocked();
                QThread::msleep(2);
            }
        }

        if (!ok) {
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

    Ch341Addressing scanAddressing;
    scanAddressing.blockBits = 0;
    scanAddressing.twoByteAddress = false;

    for (uint8_t addr = 0x08; addr <= 0x77; ++addr) {
        scanAddressing.deviceAddress = addr;
        std::vector<uint8_t> ignored;
        if (streamUnlocked(Ch341Protocol::buildProbe(scanAddressing, 0), 0, ignored)) {
            found.push_back(addr);
        } else if (m_lastError.find("USB") != std::string::npos) {
            // Ошибка USB, а не NAK: дальше сканировать бессмысленно.
            break;
        }
    }

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
    if (m_vendorId != 0 || m_productId != 0) {
        const QString vid = QStringLiteral("%1")
                                .arg(m_openedVendorId != 0 ? m_openedVendorId : m_vendorId,
                                     4, 16, QLatin1Char('0'))
                                .toUpper();
        const QString pid = QStringLiteral("%1")
                                .arg(m_openedProductId != 0 ? m_openedProductId : m_productId,
                                     4, 16, QLatin1Char('0'))
                                .toUpper();
        return QStringLiteral("CH341A-совместимый (%1:%2)").arg(vid, pid);
    }
    return QStringLiteral("CH341A (I2C)");
}
