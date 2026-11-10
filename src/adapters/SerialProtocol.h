#ifndef SERIAL_PROTOCOL_H
#define SERIAL_PROTOCOL_H

#include <QByteArray>
#include <QString>

#include <cstdint>

// Бинарный протокол самодельного I2C-программатора на Arduino/ESP.
// Формат кадра запроса (little-endian):
//   A5 5A | cmd | i2c_addr | mem_addr(4) | length(2) | payload | crc16
// Формат кадра ответа:
//   A5 5A | status | cmd | length(2) | payload | crc16
// CRC — сумма всех байт от cmd/status до конца payload, 16 бит.
// Для READ поле length задаёт количество читаемых байт, payload пуст.
// Для WRITE length равно длине payload.
namespace SerialProtocol {

constexpr quint8 kMagic1 = 0xA5;
constexpr quint8 kMagic2 = 0x5A;
constexpr int kRequestHeaderSize = 10;
constexpr int kResponseHeaderSize = 6;
constexpr int kMaxPayload = 128;       // как в прошивке (буфер Arduino)
constexpr int kMaxChunk = 32;          // одна I2C-транзакция
constexpr int kBaudRate = 115200;

enum class Command : quint8 {
    Read = 1,
    Write = 2,
    Scan = 3,
    Ping = 4,
    Configure = 5
};

enum class Status : quint8 {
    Ok = 0,
    Nak = 1,
    BadRequest = 2,
    BusError = 3,
    Unsupported = 4
};

struct ChipConfig {
    quint8 deviceAddress = 0x50;
    bool twoByteAddress = false;
    quint8 blockBits = 0;
    quint16 pageSize = 16;
    quint16 writeCycleMs = 5;
};

QByteArray buildRequest(Command command, quint8 i2cAddress, quint32 memoryAddress,
                        quint16 length, const QByteArray &payload = QByteArray());

QByteArray buildConfigureRequest(const ChipConfig &config);

// Возвращает false, если кадр повреждён. error заполняется для диагностики.
bool parseResponse(const QByteArray &frame, Status *status, Command *command,
                   QByteArray *payload, QString *error = nullptr);

quint16 checksum16(const QByteArray &data, int start, int end);

QByteArray configurePayload(const ChipConfig &config);

} // namespace SerialProtocol

#endif // SERIAL_PROTOCOL_H
