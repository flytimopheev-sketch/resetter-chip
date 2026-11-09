#include "Checksum.h"

#include <cstring>

quint32 Checksum::crc32(const QByteArray &data)
{
    quint32 crc = 0xFFFFFFFFu;
    for (uchar byte : data) {
        crc ^= byte;
        for (int i = 0; i < 8; ++i) {
            if (crc & 1u) {
                crc = (crc >> 1) ^ 0xEDB88320u;
            } else {
                crc >>= 1;
            }
        }
    }
    return ~crc;
}

quint16 Checksum::simpleSum(const QByteArray &data)
{
    quint32 sum = 0;
    for (uchar byte : data) {
        sum += byte;
    }
    return static_cast<quint16>(sum & 0xFFFFu);
}

QByteArray Checksum::appendCRC32(const QByteArray &data)
{
    QByteArray result = data;
    const quint32 crc = crc32(data);

    // Явный порядок little-endian: файлы переносимы между архитектурами
    result.append(static_cast<char>(crc & 0xFFu));
    result.append(static_cast<char>((crc >> 8) & 0xFFu));
    result.append(static_cast<char>((crc >> 16) & 0xFFu));
    result.append(static_cast<char>((crc >> 24) & 0xFFu));
    return result;
}

bool Checksum::verifyCRC32(const QByteArray &data)
{
    if (data.size() < 4) {
        return false;
    }

    const QByteArray payload = data.left(data.size() - 4);
    const auto *tail = reinterpret_cast<const uchar *>(data.constData() + data.size() - 4);
    const quint32 storedCrc = static_cast<quint32>(tail[0])
                              | (static_cast<quint32>(tail[1]) << 8)
                              | (static_cast<quint32>(tail[2]) << 16)
                              | (static_cast<quint32>(tail[3]) << 24);
    return crc32(payload) == storedCrc;
}

quint32 Checksum::compute(const QByteArray &data, Algorithm algo)
{
    switch (algo) {
    case Algorithm::CRC32:
        return crc32(data);
    case Algorithm::SUM16:
        return simpleSum(data);
    case Algorithm::NONE:
        return 0;
    }
    return 0;
}
