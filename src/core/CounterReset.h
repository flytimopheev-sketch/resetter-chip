#ifndef COUNTER_RESET_H
#define COUNTER_RESET_H

#include "ChipProfile.h"

#include <QByteArray>

#include <optional>

// Работа со счётчиком страниц в дампе EEPROM.
// Порядок байтов задаётся профилем (counter_endian): по умолчанию
// little-endian, для картриджей с big-endian счётчиком — "big".
// Сброс устанавливает значение counter_initial_value из профиля (обычно 0).
class CounterReset
{
public:
    static std::optional<quint32> readCounter(const QByteArray &data, const ChipProfile &profile);
    static bool setCounter(QByteArray &data, const ChipProfile &profile, quint32 newValue);
    static std::optional<QByteArray> resetCounter(QByteArray data, const ChipProfile &profile);
};

#endif // COUNTER_RESET_H
