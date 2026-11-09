#include "FileUtils.h"
#include <QFile>
#include <QDataStream>
#include <QFileInfo>

std::optional<QByteArray> FileUtils::readBinaryFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }
    QByteArray data = file.readAll();
    file.close();
    return data;
}

bool FileUtils::writeBinaryFile(const QString &filePath, const QByteArray &data)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    qint64 written = file.write(data);
    file.close();
    return written == data.size();
}

quint32 FileUtils::calculateCRC32(const QByteArray &data)
{
    // Простая реализация CRC32 для контрольных сумм дампов
    quint32 crc = 0xFFFFFFFF;
    for (uchar byte : data) {
        crc ^= byte;
        for (int i = 0; i < 8; ++i) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
    }
    return ~crc;
}

QString FileUtils::formatHex(const QByteArray &data, int bytesPerLine)
{
    QString result;
    for (int i = 0; i < data.size(); i += bytesPerLine) {
        int chunkSize = qMin(bytesPerLine, data.size() - i);
        QString hexPart;
        QString asciiPart;
        for (int j = 0; j < chunkSize; ++j) {
            uchar b = static_cast<uchar>(data[i + j]);
            hexPart += QString("%1 ").arg(b, 2, 16, QLatin1Char('0')).toUpper();
            asciiPart += (b >= 32 && b < 127) ? QChar(b) : QChar('.');
        }
        result += QString("%1:  %2  %3\n")
                      .arg(i, 8, 16, QLatin1Char('0'))
                      .arg(hexPart)
                      .arg(asciiPart);
    }
    return result;
}

bool FileUtils::fileExistsAndNotEmpty(const QString &filePath)
{
    QFileInfo info(filePath);
    return info.exists() && info.size() > 0;
}