#include "FileUtils.h"

#include <QDataStream>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

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
    // QSaveFile пишет во временный файл и атомарно переименовывает его:
    // обрыв питания или переполнение диска не оставят نصف дампа.
    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    if (file.write(data) != data.size()) {
        file.cancelWriting();
        return false;
    }
    return file.commit();
}

quint32 FileUtils::calculateCRC32(const QByteArray &data)
{
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
    if (bytesPerLine <= 0) {
        bytesPerLine = 16;
    }

    QString result;
    for (int i = 0; i < data.size(); i += bytesPerLine) {
        const int chunkSize = qMin(bytesPerLine, data.size() - i);
        QString hexPart;
        QString asciiPart;
        for (int j = 0; j < chunkSize; ++j) {
            const uchar b = static_cast<uchar>(data.at(i + j));
            hexPart += QStringLiteral("%1 ").arg(b, 2, 16, QLatin1Char('0')).toUpper();
            asciiPart += (b >= 32 && b < 127) ? QChar(b) : QChar(QLatin1Char('.'));
        }
        result += QStringLiteral("%1:  %2 %3\n")
                      .arg(i, 8, 16, QLatin1Char('0'))
                      .arg(hexPart, asciiPart);
    }
    return result;
}

bool FileUtils::fileExistsAndNotEmpty(const QString &filePath)
{
    QFileInfo info(filePath);
    return info.exists() && info.isFile() && info.size() > 0;
}
