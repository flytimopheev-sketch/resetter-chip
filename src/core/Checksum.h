#ifndef CHECKSUM_H
#define CHECKSUM_H

#include <QByteArray>
#include <QString>

// Контрольные суммы и проверка целостности дампов
class Checksum
{
public:
    // Простой CRC32 (аналогичный FileUtils, но для ядра)
    static quint32 crc32(const QByteArray &data);

    // Контрольная сумма на основе суммы байт (простая, для быстрой проверки)
    static quint16 simpleSum(const QByteArray &data);

    // Добавить CRC32 в конец дампа (последние 4 байта)
    static QByteArray appendCRC32(const QByteArray &data);

    // Проверить CRC32 в конце дампа
    static bool verifyCRC32(const QByteArray &data);

    // Вычислить контрольную сумму по заданному алгоритму
    enum class Algorithm {
        CRC32,
        SUM16,
        NONE
    };

    static quint32 compute(const QByteArray &data, Algorithm algo);
};

#endif // CHECKSUM_H