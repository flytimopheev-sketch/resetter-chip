#include "DumpManager.h"

#include "core/Checksum.h"
#include "utils/FileUtils.h"

#include <QDataStream>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace {
// Магия формата: позволяет надёжно отличить наш контейнер от сырого дампа
const QByteArray kMagic = QByteArrayLiteral("RSCHIPD1");
constexpr quint16 kFormatVersion = 1;

QByteArray buildContainer(const QByteArray &data, const QString &chipId,
                          const QString &cartridgeModel, const QDateTime &createdAt)
{
    QByteArray payload;
    QDataStream stream(&payload, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_0);
    stream << kFormatVersion
           << chipId
           << cartridgeModel
           << createdAt
           << static_cast<quint32>(data.size())
           << Checksum::crc32(data);
    stream.writeRawData(data.constData(), data.size());

    return kMagic + payload;
}

bool parseContainer(const QByteArray &raw, DumpInfo &info, QByteArray &outData)
{
    if (!raw.startsWith(kMagic)) {
        return false;
    }

    const QByteArray body = raw.mid(kMagic.size());
    QDataStream stream(body);
    stream.setVersion(QDataStream::Qt_6_0);

    quint16 version = 0;
    quint32 dataSize = 0;
    quint32 storedCrc = 0;

    stream >> version;
    if (version != kFormatVersion) {
        return false;
    }

    stream >> info.chipId >> info.cartridgeModel >> info.createdAt >> dataSize >> storedCrc;
    if (stream.status() != QDataStream::Ok) {
        return false;
    }

    if (dataSize > static_cast<quint32>(body.size())) {
        return false;
    }

    outData.resize(static_cast<int>(dataSize));
    if (stream.readRawData(outData.data(), static_cast<int>(dataSize)) != static_cast<int>(dataSize)) {
        return false;
    }

    info.sizeBytes = dataSize;
    info.crc32 = Checksum::crc32(outData);
    info.hasMetadata = true;
    return true;
}
}

bool DumpManager::saveDump(const QString &filePath, const QByteArray &data,
                           const QString &chipId, const QString &cartridgeModel)
{
    const QFileInfo fileInfo(filePath);
    QDir dir = fileInfo.dir();
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        return false;
    }

    const QByteArray container = buildContainer(data, chipId, cartridgeModel,
                                                QDateTime::currentDateTime());
    return FileUtils::writeBinaryFile(filePath, container);
}

std::optional<QByteArray> DumpManager::loadDump(const QString &filePath)
{
    const auto raw = FileUtils::readBinaryFile(filePath);
    if (!raw.has_value()) {
        return std::nullopt;
    }

    DumpInfo info;
    QByteArray payload;
    if (parseContainer(*raw, info, payload)) {
        return payload;
    }

    // Файл без нашего заголовка считается сырым дампом
    return *raw;
}

std::optional<DumpInfo> DumpManager::dumpInfo(const QString &filePath)
{
    const QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        return std::nullopt;
    }

    const auto raw = FileUtils::readBinaryFile(filePath);
    if (!raw.has_value()) {
        return std::nullopt;
    }

    DumpInfo info;
    info.filePath = filePath;
    info.createdAt = fileInfo.lastModified();

    QByteArray payload;
    if (!parseContainer(*raw, info, payload)) {
        info.sizeBytes = raw->size();
        info.crc32 = Checksum::crc32(*raw);
        info.hasMetadata = false;
    }

    return info;
}

QVector<quint32> DumpManager::compareDumps(const QByteArray &a, const QByteArray &b)
{
    QVector<quint32> differences;

    const int minSize = qMin(a.size(), b.size());
    for (int i = 0; i < minSize; ++i) {
        if (a.at(i) != b.at(i)) {
            differences.append(static_cast<quint32>(i));
        }
    }

    const int maxSize = qMax(a.size(), b.size());
    for (int i = minSize; i < maxSize; ++i) {
        differences.append(static_cast<quint32>(i));
    }

    return differences;
}

int DumpManager::countDifferences(const QByteArray &a, const QByteArray &b)
{
    return static_cast<int>(compareDumps(a, b).size());
}

QString DumpManager::backupDirectory()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
           + QStringLiteral("/backups");
}

QString DumpManager::backupFileName(const QString &prefix)
{
    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_hhmmss"));
    return QStringLiteral("%1/%2_%3.bin").arg(backupDirectory(), prefix, timestamp);
}
