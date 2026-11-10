#ifndef CH341_PROTOCOL_H
#define CH341_PROTOCOL_H

#include <cstdint>
#include <vector>

// Параметры адресации, которые влияют на байт устройства и адрес памяти.
struct Ch341Addressing {
    uint8_t deviceAddress = 0x50;  // 7-битный базовый адрес
    bool twoByteAddress = false;   // 24C32 и крупнее
    uint8_t blockBits = 0;         // 24C04/08/16 -> 1/2/3, 24C1024 -> 1
};

// Чистые функции сборки пакетов CH341A в режиме I2C stream.
// Вынесены из адаптера, чтобы протокол можно было проверить тестами без USB.
namespace Ch341Protocol {

constexpr uint8_t kStreamCommand = 0xAA;
constexpr uint8_t kStart = 0x74;
constexpr uint8_t kStop = 0x75;
constexpr uint8_t kOut = 0x80;
constexpr uint8_t kIn = 0xC0;
constexpr uint8_t kSet = 0x60;
constexpr uint8_t kEnd = 0x00;
constexpr uint8_t kSpeed100kHz = 1;
constexpr uint8_t kStatusNak = 0x80;

// Байт адреса устройства с учётом старших бит адреса памяти.
uint8_t deviceAddressByte(const Ch341Addressing &addressing, uint32_t address, bool readOp);

// 1 или 2 байта адреса памяти (старший байт первым).
std::vector<uint8_t> memoryAddressBytes(const Ch341Addressing &addressing, uint32_t address);

// Настроить скорость шины.
std::vector<uint8_t> buildConfigure(uint8_t speed = kSpeed100kHz);

// START + адрес устройства + STOP — проверка ACK.
std::vector<uint8_t> buildProbe(const Ch341Addressing &addressing, uint32_t address);

// Чтение: dummy write адреса памяти, repeated START, чтение length байт.
std::vector<uint8_t> buildRead(const Ch341Addressing &addressing, uint32_t address,
                               uint16_t length);

// Запись: START + адрес устройства + адрес памяти + data + STOP.
std::vector<uint8_t> buildWrite(const Ch341Addressing &addressing, uint32_t address,
                                const uint8_t *data, uint16_t length);

// STOP без данных — восстановление шины после ошибки.
std::vector<uint8_t> buildBusRecovery();

// Ответ программатора: первый байт — статус, бит 0x80 = NAK.
bool responseIsAck(const std::vector<uint8_t> &response);

} // namespace Ch341Protocol

#endif // CH341_PROTOCOL_H
