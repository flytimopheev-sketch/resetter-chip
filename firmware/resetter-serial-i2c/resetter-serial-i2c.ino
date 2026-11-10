/*
 * Resetter Chip — прошивка самодельного I2C-программатора.
 *
 * Плата: Arduino Uno/Nano/Leonardo, ESP8266 или ESP32.
 * Подключение EEPROM 24Cxx:
 *   SDA  -> SDA (A4 на Uno/Nano, GPIO4 на ESP8266, GPIO21 на ESP32)
 *   SCL  -> SCL (A5 на Uno/Nano, GPIO5 на ESP8266, GPIO22 на ESP32)
 *   GND  -> GND, VCC -> 3.3 В или 5 В (по datasheet чипа)
 *   Подтяжка SDA/SCL к питанию 4.7 кОм, если её нет на модуле.
 *
 * Протокол описан в docs/PROTOCOL.md. Кадр запроса:
 *   A5 5A | cmd | i2c_addr | mem_addr(4, LE) | length(2, LE) | payload | CRC16
 * Кадр ответа:
 *   A5 5A | status | cmd | length(2, LE) | payload | CRC16
 *
 * CRC16 — сумма байт от поля cmd/status до конца payload (16 бит, LE).
 * Скорость порта: 115200 8N1.
 */

#include <Arduino.h>
#include <Wire.h>

#include <algorithm>

namespace {

constexpr uint8_t kMagic1 = 0xA5;
constexpr uint8_t kMagic2 = 0x5A;
constexpr uint8_t kRequestHeader = 10;
constexpr uint16_t kMaxPayload = 128;
constexpr size_t kMaxFrame = kRequestHeader + kMaxPayload + 2;

enum Command : uint8_t {
    CmdRead = 1,
    CmdWrite = 2,
    CmdScan = 3,
    CmdPing = 4,
    CmdConfigure = 5
};

enum Status : uint8_t {
    StatusOk = 0,
    StatusNak = 1,
    StatusBadRequest = 2,
    StatusBusError = 3,
    StatusUnsupported = 4
};

struct ChipConfig {
    uint8_t deviceAddress = 0x50;
    bool twoByteAddress = false;
    uint8_t blockBits = 0;
    uint16_t pageSize = 16;
    uint16_t writeCycleMs = 5;
};

ChipConfig config;
uint8_t rxBuffer[kMaxFrame];
size_t rxLength = 0;

uint16_t checksum16(const uint8_t *data, size_t start, size_t end)
{
    uint32_t sum = 0;
    for (size_t i = start; i < end; ++i) {
        sum += data[i];
    }
    return static_cast<uint16_t>(sum & 0xFFFFu);
}

uint32_t readLe32(const uint8_t *data)
{
    return static_cast<uint32_t>(data[0])
           | (static_cast<uint32_t>(data[1]) << 8)
           | (static_cast<uint32_t>(data[2]) << 16)
           | (static_cast<uint32_t>(data[3]) << 24);
}

uint16_t readLe16(const uint8_t *data)
{
    return static_cast<uint16_t>(data[0])
           | (static_cast<uint16_t>(data[1]) << 8);
}

void writeLe16(uint8_t *data, uint16_t value)
{
    data[0] = static_cast<uint8_t>(value & 0xFF);
    data[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
}

// Arduino Wire использует 7-битный адрес. Старшие биты адреса памяти
// (24C04/08/16 и A16 у 24C1024) занимают младшие биты 7-битного адреса.
uint8_t deviceAddress7(uint8_t baseAddress, uint32_t memoryAddress)
{
    uint8_t address = static_cast<uint8_t>(baseAddress & 0x7F);
    if (address == 0) {
        address = static_cast<uint8_t>(config.deviceAddress & 0x7F);
    }

    if (config.blockBits > 0) {
        const uint8_t mask = static_cast<uint8_t>((1u << config.blockBits) - 1u);
        const uint8_t shift = config.twoByteAddress ? 16 : 8;
        const uint8_t block = static_cast<uint8_t>((memoryAddress >> shift) & mask);
        address = static_cast<uint8_t>(address | block);
    }
    return address;
}

void sendResponse(uint8_t status, uint8_t command, const uint8_t *payload, uint16_t length)
{
    if (length > kMaxPayload) {
        status = StatusBadRequest;
        length = 0;
    }

    uint8_t frame[6 + kMaxPayload + 2];
    frame[0] = kMagic1;
    frame[1] = kMagic2;
    frame[2] = status;
    frame[3] = command;
    writeLe16(frame + 4, length);
    for (uint16_t i = 0; i < length; ++i) {
        frame[6 + i] = payload[i];
    }
    const uint16_t crc = checksum16(frame, 2, 6 + length);
    writeLe16(frame + 6 + length, crc);
    Serial.write(frame, 6 + length + 2);
}

bool waitReady(uint8_t address)
{
    const uint32_t start = millis();
    const uint32_t timeout = std::max<uint32_t>(50, config.writeCycleMs * 20UL);
    while (millis() - start < timeout) {
        Wire.beginTransmission(address);
        if (Wire.endTransmission() == 0) {
            return true;
        }
        delay(1);
    }
    return false;
}

uint8_t readMemory(uint8_t baseAddress, uint32_t memoryAddress, uint8_t *out, uint16_t length)
{
    const uint8_t address = deviceAddress7(baseAddress, memoryAddress);

    Wire.beginTransmission(address);
    if (config.twoByteAddress) {
        Wire.write(static_cast<uint8_t>((memoryAddress >> 8) & 0xFF));
    }
    Wire.write(static_cast<uint8_t>(memoryAddress & 0xFF));
    if (Wire.endTransmission(false) != 0) {
        return StatusNak;
    }

    const uint16_t received = Wire.requestFrom(address, static_cast<uint8_t>(length));
    if (received != length) {
        return StatusBusError;
    }
    for (uint16_t i = 0; i < length; ++i) {
        out[i] = static_cast<uint8_t>(Wire.read());
    }
    return StatusOk;
}

uint8_t writeMemory(uint8_t baseAddress, uint32_t memoryAddress, const uint8_t *data,
                    uint16_t length)
{
    uint16_t offset = 0;
    const uint16_t pageSize = config.pageSize > 0 ? config.pageSize : 16;

    while (offset < length) {
        const uint32_t currentAddress = memoryAddress + offset;
        const uint16_t pageRemaining =
            static_cast<uint16_t>(pageSize - (currentAddress % pageSize));
        uint16_t chunk = std::min<uint16_t>(length - offset, pageRemaining);

        // Ограничение буфера Wire на AVR — 32 байта, включая адрес памяти.
        const uint16_t overhead = config.twoByteAddress ? 3 : 2;
        if (chunk + overhead > 32) {
            chunk = static_cast<uint16_t>(32 - overhead);
        }

        const uint8_t address = deviceAddress7(baseAddress, currentAddress);
        Wire.beginTransmission(address);
        if (config.twoByteAddress) {
            Wire.write(static_cast<uint8_t>((currentAddress >> 8) & 0xFF));
        }
        Wire.write(static_cast<uint8_t>(currentAddress & 0xFF));
        Wire.write(data + offset, chunk);

        if (Wire.endTransmission() != 0) {
            return StatusNak;
        }

        delay(std::max<uint16_t>(2, config.writeCycleMs));
        if (!waitReady(address)) {
            return StatusBusError;
        }
        offset = static_cast<uint16_t>(offset + chunk);
    }
    return StatusOk;
}

void handleRequest(const uint8_t *frame, size_t frameLength)
{
    if (frameLength < kRequestHeader + 2) {
        return;
    }

    const uint16_t payloadLength = readLe16(frame + 8);
    if (payloadLength > kMaxPayload || frameLength != kRequestHeader + payloadLength + 2) {
        return;
    }

    const uint16_t storedCrc = readLe16(frame + kRequestHeader + payloadLength);
    const uint16_t actualCrc = checksum16(frame, 2, kRequestHeader + payloadLength);
    if (storedCrc != actualCrc) {
        return;
    }

    const uint8_t command = frame[2];
    const uint8_t i2cAddress = frame[3];
    const uint32_t memoryAddress = readLe32(frame + 4);
    const uint16_t length = readLe16(frame + 8);
    const uint8_t *payload = frame + kRequestHeader;

    switch (command) {
    case CmdPing: {
        static const char id[] = "RESETTER1";
        sendResponse(StatusOk, command, reinterpret_cast<const uint8_t *>(id),
                     static_cast<uint16_t>(sizeof(id) - 1));
        break;
    }

    case CmdConfigure: {
        if (payloadLength != 7) {
            sendResponse(StatusBadRequest, command, nullptr, 0);
            break;
        }
        config.deviceAddress = payload[0];
        config.twoByteAddress = payload[1] != 0;
        config.blockBits = payload[2];
        config.pageSize = readLe16(payload + 3);
        config.writeCycleMs = readLe16(payload + 5);
        sendResponse(StatusOk, command, nullptr, 0);
        break;
    }

    case CmdRead: {
        if (length == 0 || length > kMaxPayload || payloadLength != 0) {
            sendResponse(StatusBadRequest, command, nullptr, 0);
            break;
        }
        uint8_t data[kMaxPayload];
        const uint8_t status = readMemory(i2cAddress, memoryAddress, data, length);
        sendResponse(status, command, data, status == StatusOk ? length : 0);
        break;
    }

    case CmdWrite: {
        if (length == 0 || length != payloadLength || length > kMaxPayload) {
            sendResponse(StatusBadRequest, command, nullptr, 0);
            break;
        }
        const uint8_t status = writeMemory(i2cAddress, memoryAddress, payload, length);
        sendResponse(status, command, nullptr, 0);
        break;
    }

    case CmdScan: {
        uint8_t found[112];
        uint16_t count = 0;
        for (uint8_t address = 0x08; address <= 0x77; ++address) {
            Wire.beginTransmission(address);
            if (Wire.endTransmission() == 0) {
                found[count++] = address;
            }
        }
        sendResponse(StatusOk, command, found, count);
        break;
    }

    default:
        sendResponse(StatusUnsupported, command, nullptr, 0);
        break;
    }
}

} // namespace

void setup()
{
    Serial.begin(115200);
    Wire.begin();
    Wire.setClock(100000);
}

void loop()
{
    while (Serial.available() > 0) {
        const uint8_t byte = static_cast<uint8_t>(Serial.read());

        if (rxLength == 0 && byte != kMagic1) {
            continue;
        }
        if (rxLength == 1 && byte != kMagic2) {
            rxLength = 0;
            continue;
        }

        if (rxLength < kMaxFrame) {
            rxBuffer[rxLength++] = byte;
        } else {
            rxLength = 0;
            continue;
        }

        if (rxLength >= kRequestHeader) {
            const uint16_t payloadLength = readLe16(rxBuffer + 8);
            if (payloadLength > kMaxPayload) {
                rxLength = 0;
                continue;
            }
            const size_t expected = kRequestHeader + payloadLength + 2;
            if (rxLength == expected) {
                handleRequest(rxBuffer, rxLength);
                rxLength = 0;
            }
        }
    }
}
