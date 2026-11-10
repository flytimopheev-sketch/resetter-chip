#ifndef CHIP_PROFILE_H
#define CHIP_PROFILE_H

#include <QJsonObject>
#include <QString>
#include <QVector>

#include <cstdint>

// Профиль чипа EEPROM: геометрия, адресация и защищённые зоны
struct ChipProfile {
    QString id;                                  // ID профиля, например "24C16"
    QString name;                                // Человекочитаемое имя
    quint32 sizeBytes = 0;                       // Размер чипа в байтах
    quint8 i2cAddress = 0x50;                    // I2C-адрес по умолчанию (7 бит)
    bool twoByteAddress = false;                 // 2 байта адреса памяти (24C32 и крупнее)
    quint8 blockBits = 0;                        // Биты адреса в байте устройства (24C04/08/16, 24C1024)
    quint16 pageSize = 16;                       // Размер страницы записи
    quint32 writeCycleMs = 5;                    // Время цикла записи
    QString family;                              // Семейство (24Cxx и т.п.)

    // Защищённые зоны: пары (начало, конец-не-включая)
    QVector<QPair<quint32, quint32>> protectedRanges;

    // Разрешённые для изменения зоны. Пустой список = ограничений нет,
    // кроме protectedRanges.
    QVector<QPair<quint32, quint32>> allowedRanges;

    quint32 counterOffset = 0xFFFFFFFF;          // 0xFFFFFFFF = не задано
    quint32 counterSize = 2;                     // 1, 2 или 4 байта

    static ChipProfile fromJson(const QJsonObject &obj);
    QJsonObject toJson() const;

    // Проверка геометрии и диапазонов перед использованием профиля
    bool isValid() const;

    bool isAddressProtected(quint32 address, quint32 length) const;

    // Максимальное значение счётчика для выбранного counterSize
    quint32 counterMaxValue() const;
};

#endif // CHIP_PROFILE_H
