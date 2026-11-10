#include "CounterReset.h"

namespace {
constexpr quint32 kCounterUnset = 0xFFFFFFFFu;

bool counterFits(const ChipProfile &profile, int dataSize)
{
    if (profile.counterOffset == kCounterUnset) {
        return false;
    }
    if (profile.counterSize != 1 && profile.counterSize != 2 && profile.counterSize != 4) {
        return false;
    }

    const quint64 end = static_cast<quint64>(profile.counterOffset) + profile.counterSize;
    return end <= static_cast<quint64>(dataSize);
}
} // namespace

std::optional<quint32> CounterReset::readCounter(const QByteArray &data, const ChipProfile &profile)
{
    if (!counterFits(profile, data.size())) {
        return std::nullopt;
    }

    quint32 value = 0;
    for (quint32 i = 0; i < profile.counterSize; ++i) {
        const quint8 byte = static_cast<quint8>(data.at(static_cast<int>(profile.counterOffset + i)));
        value |= static_cast<quint32>(byte) << (8u * i);
    }
    return value;
}

bool CounterReset::setCounter(QByteArray &data, const ChipProfile &profile, quint32 newValue)
{
    if (!counterFits(profile, data.size())) {
        return false;
    }

    const quint32 value = newValue & profile.counterMaxValue();
    for (quint32 i = 0; i < profile.counterSize; ++i) {
        const quint8 byte = static_cast<quint8>((value >> (8u * i)) & 0xFFu);
        data[static_cast<int>(profile.counterOffset + i)] = static_cast<char>(byte);
    }
    return true;
}

std::optional<QByteArray> CounterReset::resetCounter(QByteArray data, const ChipProfile &profile)
{
    if (!setCounter(data, profile, 0)) {
        return std::nullopt;
    }
    return data;
}
