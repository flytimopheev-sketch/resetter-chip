#ifndef COUNTER_RESET_H
#define COUNTER_RESET_H

#include "ChipProfile.h"

#include <QByteArray>

#include <optional>

// Работа со счётчиком страниц в дампе EEPROM.
// Все многобайтные значения хранятся в little-endian — так же, как их
// читает большинство прошивок картриджей.
class CounterReset
{
public:
    static std::optional<quint32> readCounter(const QByteArray &data, const ChipProfile &profile);
    static bool setCounter(QByteArray &data, const ChipProfile &profile, quint32 newValue);
    static std::optional<QByteArray> resetCounter(QByteArray data, const ChipProfile &profile);
};

#endif // COUNTER_RESET_H
