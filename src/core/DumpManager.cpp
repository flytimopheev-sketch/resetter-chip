#include "DumpManager.h"

#include "core/Checksum.h"
#include "utils/FileUtils.h"
#include "utils/Logger.h"

#include <QBuffer>
#include <QDataStream>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>

namespace {
// Магия формата: позволяет надёжно отличить наш контейнер от сырого дампа
const QByteArray kMagic = QByteArrayLiteral("RSCHIPD1");
constexpr quint16 kFormatVersion = 1;
constexpr qint64 kMaxDumpSize = 16 * 1024 * 1024;
constexpr qint64 kMaxContainerSize = kMaxDumpSize + 1024 * 1024;

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
    if (raw.size() > kMaxContainerSize) {
        info.error = QStringLiteral("Файл слишком велик для контейнера дампа");
        return false;
    }

    const QByteArray body = raw.mid(kMagic.size());
    QBuffer buffer;
    buffer.setData(body);
    if (!buffer.open(QIODevice::ReadOnly)) {
        info.error = QStringLiteral("Не удалось открыть буфер контейнера");
        return false;
    }

    QDataStream stream(&buffer);
    stream.setVersion(QDataStream::Qt_6_0);

    quint16 version = 0;
    quint32 dataSize = 0;
    quint32 storedCrc = 0;
    QString chipId;
    QString cartridgeModel;
    QDateTime createdAt;

    stream >> version;
    if (version != kFormatVersion) {
        info.error = QStringLiteral("Неподдерживаемая версия контейнера: %1").arg(version);
        return false;
    }

    stream >> chipId >> cartridgeModel >> createdAt >> dataSize >> storedCrc;
    if (stream.status() != QDataStream::Ok) {
        info.error = QStringLiteral("Повреждён заголовок контейнера");
        return false;
    }

    if (dataSize > static_cast<quint32>(kMaxDumpSize)) {
        info.error = QStringLiteral("Некорректный размер данных в контейнере");
        return false;
    }
    if (static_cast<qint64>(dataSize) > buffer.bytesAvailable()) {
        info.error = QStringLiteral("Данные контейнера обрезаны");
        return false;
    }

    outData.resize(static_cast<int>(dataSize));
    if (stream.readRawData(outData.data(), static_cast<int>(dataSize)) != static_cast<int>(dataSize)) {
        info.error = QStringLiteral("Не удалось прочитать данные контейнера");
        outData.clear();
        return false;
    }

    const quint32 actualCrc = Checksum::crc32(outData);
    if (actualCrc != storedCrc) {
        info.error = QStringLiteral("CRC32 не совпадает: ожидалось 0x%1, получено 0x%2")
                         .arg(storedCrc, 8, 16, QLatin1Char('0'))
                         .arg(actualCrc, 8, 16, QLatin1Char('0'));
        outData.clear();
        info.crc32 = actualCrc;
        info.crcValid = false;
        return false;
    }

    info.chipId = chipId;
    info.cartridgeModel = cartridgeModel;
    info.createdAt = createdAt;
    info.sizeBytes = dataSize;
    info.crc32 = actualCrc;
    info.hasMetadata = true;
    info.crcValid = true;
    info.error.clear();
    return true;
}

bool readDumpFile(const QString &filePath, QByteArray &raw)
{
    const QFileInfo fileInfo(filePath);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        return false;
    }
    if (fileInfo.size() > kMaxContainerSize) {
        Logger::instance().error(QStringLiteral("Файл дампа слишком велик: %1").arg(filePath));
        return false;
    }

    const auto data = FileUtils::readBinaryFile(filePath);
    if (!data.has_value()) {
        return false;
    }
    raw = *data;
    return true;
}
} // namespace

bool DumpManager::saveDump(const QString &filePath, const QByteArray &data,
                           const QString &chipId, const QString &cartridgeModel)
{
    if (data.size() > kMaxDumpSize) {
        Logger::instance().error(QStringLiteral("Дамп слишком велик для сохранения: %1 байт").arg(data.size()));
        return false;
    }

    const QFileInfo fileInfo(filePath);
    QDir dir = fileInfo.dir();
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        return false;
    }

    const QByteArray container = buildContainer(data, chipId, cartridgeModel,
                                                QDateTime::currentDateTime());
    return FileUtils::writeBinaryFile(filePath, container);
}

bool DumpManager::saveRawDump(const QString &filePath, const QByteArray &data)
{
    if (data.size() > kMaxDumpSize) {
        Logger::instance().error(QStringLiteral("Дамп слишком велик для сохранения: %1 байт").arg(data.size()));
        return false;
    }

    const QFileInfo fileInfo(filePath);
    QDir dir = fileInfo.dir();
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        return false;
    }

    return FileUtils::writeBinaryFile(filePath, data);
}

std::optional<QByteArray> DumpManager::loadDump(const QString &filePath)
{
    QByteArray raw;
    if (!readDumpFile(filePath, raw)) {
        Logger::instance().error(QStringLiteral("Не удалось прочитать файл дампа: %1").arg(filePath));
        return std::nullopt;
    }

    DumpInfo info;
    QByteArray payload;
    if (parseContainer(raw, info, payload)) {
        return payload;
    }

    if (raw.startsWith(kMagic)) {
        // Файл заявлен как контейнер, но повреждён. Возвращать его как «сырой»
        // дамп опасно: заголовок попадёт в данные чипа.
        Logger::instance().error(QStringLiteral("Контейнер дампа повреждён (%1): %2")
                                     .arg(filePath, info.error));
        return std::nullopt;
    }

    // Файл без нашего заголовка считается сырым дампом
    if (raw.size() > kMaxDumpSize) {
        Logger::instance().error(QStringLiteral("Сырой дамп слишком велик: %1").arg(filePath));
        return std::nullopt;
    }
    return raw;
}

std::optional<DumpInfo> DumpManager::dumpInfo(const QString &filePath)
{
    QByteArray raw;
    if (!readDumpFile(filePath, raw)) {
        return std::nullopt;
    }

    DumpInfo info;
    info.filePath = filePath;
    info.createdAt = QFileInfo(filePath).lastModified();

    QByteArray payload;
    if (!parseContainer(raw, info, payload)) {
        if (raw.startsWith(kMagic)) {
            info.sizeBytes = raw.size();
            info.crc32 = Checksum::crc32(raw);
            info.hasMetadata = true;
            info.crcValid = false;
            return info;
        }

        info.sizeBytes = raw.size();
        info.crc32 = Checksum::crc32(raw);
        info.hasMetadata = false;
        info.crcValid = false;
        info.error.clear();
    }

    return info;
}

QVector<quint32> DumpManager::compareDumps(const QByteArray &a, const QByteArray &b)
{
    QVector<quint32> differences;

    const int minSize = qMin(a.size(), b.size());
    const char *aData = a.constData();
    const char *bData = b.constData();
    for (int i = 0; i < minSize; ++i) {
        if (aData[i] != bData[i]) {
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
    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_hhmmss_zzz"));
    QString safePrefix = prefix;
    safePrefix.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_.-]+")), QStringLiteral("_"));
    if (safePrefix.isEmpty()) {
        safePrefix = QStringLiteral("backup");
    }
    return QStringLiteral("%1/%2_%3.rchip").arg(backupDirectory(), safePrefix, timestamp);
}
