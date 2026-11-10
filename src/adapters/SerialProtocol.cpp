#include "SerialProtocol.h"

namespace {

void appendLe16(QByteArray &out, quint16 value)
{
    out.append(static_cast<char>(value & 0xFF));
    out.append(static_cast<char>((value >> 8) & 0xFF));
}

void appendLe32(QByteArray &out, quint32 value)
{
    out.append(static_cast<char>(value & 0xFF));
    out.append(static_cast<char>((value >> 8) & 0xFF));
    out.append(static_cast<char>((value >> 16) & 0xFF));
    out.append(static_cast<char>((value >> 24) & 0xFF));
}

quint16 readLe16(const QByteArray &data, int offset)
{
    return static_cast<quint16>(static_cast<quint8>(data.at(offset)))
           | static_cast<quint16>(static_cast<quint8>(data.at(offset + 1)) << 8);
}

} // namespace

namespace SerialProtocol {

quint16 checksum16(const QByteArray &data, int start, int end)
{
    const int from = qMax(0, start);
    const int to = qMin(end, data.size());
    quint32 sum = 0;
    for (int i = from; i < to; ++i) {
        sum += static_cast<quint8>(data.at(i));
    }
    return static_cast<quint16>(sum & 0xFFFFu);
}

QByteArray buildRequest(Command command, quint8 i2cAddress, quint32 memoryAddress,
                        quint16 length, const QByteArray &payload)
{
    if (length > kMaxPayload || payload.size() > kMaxPayload) {
        return {};
    }

    QByteArray frame;
    frame.reserve(kRequestHeaderSize + payload.size() + 2);
    frame.append(static_cast<char>(kMagic1));
    frame.append(static_cast<char>(kMagic2));
    frame.append(static_cast<char>(command));
    frame.append(static_cast<char>(i2cAddress));
    appendLe32(frame, memoryAddress);
    appendLe16(frame, length);
    frame.append(payload);
    appendLe16(frame, checksum16(frame, 2, frame.size()));
    return frame;
}

QByteArray configurePayload(const ChipConfig &config)
{
    QByteArray payload;
    payload.append(static_cast<char>(config.deviceAddress));
    payload.append(static_cast<char>(config.twoByteAddress ? 1 : 0));
    payload.append(static_cast<char>(config.blockBits));
    appendLe16(payload, config.pageSize);
    appendLe16(payload, config.writeCycleMs);
    return payload;
}

QByteArray buildConfigureRequest(const ChipConfig &config)
{
    const QByteArray payload = configurePayload(config);
    return buildRequest(Command::Configure, 0, 0, static_cast<quint16>(payload.size()), payload);
}

bool parseResponse(const QByteArray &frame, Status *status, Command *command,
                   QByteArray *payload, QString *error)
{
    const auto fail = [error](const QString &text) {
        if (error) {
            *error = text;
        }
        return false;
    };

    if (frame.size() < kResponseHeaderSize + 2) {
        return fail(QStringLiteral("Короткий ответ программатора"));
    }
    if (static_cast<quint8>(frame.at(0)) != kMagic1
        || static_cast<quint8>(frame.at(1)) != kMagic2) {
        return fail(QStringLiteral("Неверная сигнатура ответа"));
    }

    const quint16 length = readLe16(frame, 4);
    if (length > kMaxPayload) {
        return fail(QStringLiteral("Слишком длинный ответ программатора"));
    }
    if (frame.size() != kResponseHeaderSize + static_cast<int>(length) + 2) {
        return fail(QStringLiteral("Неверная длина кадра ответа"));
    }

    const int crcOffset = kResponseHeaderSize + static_cast<int>(length);
    const quint16 stored = readLe16(frame, crcOffset);
    const quint16 actual = checksum16(frame, 2, crcOffset);
    if (stored != actual) {
        return fail(QStringLiteral("CRC ответа не совпадает"));
    }

    if (status) {
        *status = static_cast<Status>(static_cast<quint8>(frame.at(2)));
    }
    if (command) {
        *command = static_cast<Command>(static_cast<quint8>(frame.at(3)));
    }
    if (payload) {
        *payload = frame.mid(kResponseHeaderSize, static_cast<int>(length));
    }
    return true;
}

} // namespace SerialProtocol
