#ifndef CARTRIDGE_PROFILE_H
#define CARTRIDGE_PROFILE_H

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include <cstdint>

// Диапазон памяти картриджа с необязательным описанием.
// Конец диапазона не включается: [start, end).
struct CartridgeRange {
    quint32 start = 0;
    quint32 end = 0;
    QString description;
};

// Профиль картриджа: связывает модель картриджа с профилем EEPROM
// и уточняет смещение счётчика, защищённые и разрешённые зоны.
struct CartridgeProfile {
    QString id;
    QString name;
    QString chipProfileId;                 // id профиля чипа, например "24C16"

    QStringList printerModels;
    QStringList cartridgeModels;

    quint32 counterOffset = 0xFFFFFFFF;    // 0xFFFFFFFF = взять из профиля чипа
    quint32 counterSize = 0;               // 0 = взять из профиля чипа
    quint32 counterInitialValue = 0;       // значение нового/обнулённого счётчика
    quint32 pageResource = 0;               // заявленный ресурс, страниц

    QVector<CartridgeRange> protectedRanges;
    QVector<CartridgeRange> allowedModifyRanges;

    static CartridgeProfile fromJson(const QJsonObject &obj);
    QJsonObject toJson() const;

    bool isValid() const;
};

#endif // CARTRIDGE_PROFILE_H
