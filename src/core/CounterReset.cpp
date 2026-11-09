#include "CounterReset.h"
#include <cstring>
#include <QDebug>

std::optional<quint32> CounterReset::readCounter(const QByteArray &data, const ChipProfile &profile)
{
    if (profile.counterOffset == 0xFFFFFFFF) {
        return std::nullopt;
    }
    if (profile.counterOffset + profile.counterSize > data.size()) {
        return std::nullopt;
    }

    quint32 value = 0;
    switch (profile.counterSize) {
    case 1:
        value = static_cast<quint8>(data[profile.counterOffset]);
        break;
    case 2: {
        quint16 val;
        memcpy(&val, data.constData() + profile.counterOffset, 2);
        value = val;
        break;
    }
    case 4:
        memcpy(&value, data.constData() + profile.counterOffset, 4);
        break;
    default:
        return std::nullopt;
    }
    return value;
}

bool CounterReset::setCounter(QByteArray &data, const ChipProfile &profile, quint32 newValue)
{
    if (profile.counterOffset == 0xFFFFFFFF) {
        return false;
    }
    if (profile.counterOffset + profile.counterSize > data.size()) {
        return false;
    }

    switch (profile.counterSize) {
    case 1:
        data[profile.counterOffset] = static_cast<char>(newValue & 0xFF);
        break;
    case 2: {
        quint16 val = static_cast<quint16>(newValue & 0xFFFF);
        memcpy(data.data() + profile.counterOffset, &val, 2);
        break;
    }
    case 4:
        memcpy(data.data() + profile.counterOffset, &newValue, 4);
        break;
    default:
        return false;
    }
    return true;
}

std::optional<QByteArray> CounterReset::resetCounter(QByteArray data, const ChipProfile &profile)
{
    if (profile.counterOffset == 0xFFFFFFFF) {
        return std::nullopt;
    }

    bool ok = setCounter(data, profile, 0);
    if (!ok) {
        return std::nullopt;
    }
    return data;
}