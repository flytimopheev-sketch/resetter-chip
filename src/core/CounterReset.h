#ifndef COUNTER_RESET_H
#define COUNTER_RESET_H

#include <QByteArray>
#include <cstdint>
#include <optional>
#include "core/ChipProfile.h"

// Сброс счётчика страниц картриджа.
// Смещения и размер берутся из профиля чипа.
class CounterReset
{
public:
    // Обнулить счётчик в дампе (в памяти, не в чипе)
    // data - дамп, profile - профиль чипа с информацией о смещении счётчика
    static std::optional<QByteArray> resetCounter(QByteArray data, const ChipProfile &profile);

    // Прочитать текущее значение счётчика из дампа
    static std::optional<quint32> readCounter(const QByteArray &data, const ChipProfile &profile);

    // Установить значение счётчика в дампе
    static bool setCounter(QByteArray &data, const ChipProfile &profile, quint32 newValue);
};

#endif // COUNTER_RESET_H