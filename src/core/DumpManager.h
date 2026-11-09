#ifndef DUMP_MANAGER_H
#define DUMP_MANAGER_H

#include <QByteArray>
#include <QDateTime>
#include <QString>
#include <QVector>

#include <optional>

// Информация о файле дампа
struct DumpInfo {
    QString filePath;
    QDateTime createdAt;
    qint64 sizeBytes = 0;      // размер полезных данных (без заголовка)
    quint32 crc32 = 0;         // CRC32 полезных данных
    QString chipId;
    QString cartridgeModel;
    bool hasMetadata = false;  // был ли заголовок с метаданными
};

// Менеджер дампов: сохранение, загрузка, сравнение, резервные копии.
// Формат файла: 8 байт магии "RSCHIPD1", затем метаданные и данные.
// Файлы без магии читаются как «сырой» бинарный дамп.
class DumpManager
{
public:
    static bool saveDump(const QString &filePath, const QByteArray &data,
                         const QString &chipId = QString(),
                         const QString &cartridgeModel = QString());

    static std::optional<QByteArray> loadDump(const QString &filePath);
    static std::optional<DumpInfo> dumpInfo(const QString &filePath);

    static QVector<quint32> compareDumps(const QByteArray &a, const QByteArray &b);
    static int countDifferences(const QByteArray &a, const QByteArray &b);

    static QString backupDirectory();
    static QString backupFileName(const QString &prefix = QStringLiteral("cartridge_backup"));
};

#endif // DUMP_MANAGER_H
