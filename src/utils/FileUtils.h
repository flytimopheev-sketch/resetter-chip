#ifndef FILE_UTILS_H
#define FILE_UTILS_H

#include <QByteArray>
#include <QString>
#include <optional>

// Утилиты для работы с файлами и дампами
class FileUtils
{
public:
    // Чтение бинарного файла в QByteArray
    static std::optional<QByteArray> readBinaryFile(const QString &filePath);

    // Запись QByteArray в бинарный файл
    static bool writeBinaryFile(const QString &filePath, const QByteArray &data);

    // Вычисление простого CRC32 (вспомогательный)
    static quint32 calculateCRC32(const QByteArray &data);

    // Форматирование байтов в строку hex
    static QString formatHex(const QByteArray &data, int bytesPerLine = 16);

    // Проверка, что файл существует и не пуст
    static bool fileExistsAndNotEmpty(const QString &filePath);
};

#endif // FILE_UTILS_H