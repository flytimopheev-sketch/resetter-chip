#include "ChipProfile.h"

#include <QJsonArray>

#include <algorithm>

namespace {
constexpr qint64 kMaxChipSize = 16 * 1024 * 1024;  // защита от ошибочных профилей
constexpr quint32 kCounterUnset = 0xFFFFFFFFu;

quint32 clampedUInt32(const QJsonValue &value, quint32 defaultValue, quint32 maximum)
{
    if (value.isUndefined() || value.isNull()) {
        return defaultValue;
    }
    const qint64 raw = value.toInteger(static_cast<qint64>(defaultValue));
    if (raw < 0) {
        return 0;
    }
    return static_cast<quint32>(std::min<qint64>(raw, maximum));
}

QVector<QPair<quint32, quint32>> parseRanges(const QJsonValue &value, quint32 sizeBytes)
{
    QVector<QPair<quint32, quint32>> ranges;
    if (!value.isArray()) {
        return ranges;
    }

    for (const QJsonValue &item : value.toArray()) {
        const QJsonObject range = item.toObject();
        const qint64 startRaw = range.value(QStringLiteral("start")).toInteger(-1);
        const qint64 endRaw = range.value(QStringLiteral("end")).toInteger(-1);
        if (startRaw < 0 || endRaw <= startRaw) {
            continue;
        }

        const quint32 start = static_cast<quint32>(startRaw);
        const quint32 end = static_cast<quint32>(std::min<qint64>(endRaw, sizeBytes));
        if (end > start) {
            ranges.append(qMakePair(start, end));
        }
    }

    std::sort(ranges.begin(), ranges.end(),
              [](const QPair<quint32, quint32> &a, const QPair<quint32, quint32> &b) {
                  return a.first < b.first;
              });
    return ranges;
}
} // namespace

ChipProfile ChipProfile::fromJson(const QJsonObject &obj)
{
    ChipProfile p;
    p.id = obj.value(QStringLiteral("id")).toString().trimmed();
    p.name = obj.value(QStringLiteral("name")).toString().trimmed();
    p.sizeBytes = clampedUInt32(obj.value(QStringLiteral("size_bytes")), 0,
                                static_cast<quint32>(kMaxChipSize));
    p.i2cAddress = static_cast<quint8>(obj.value(QStringLiteral("i2c_address"))
                                           .toInteger(0x50));
    if (p.i2cAddress < 0x08 || p.i2cAddress > 0x77) {
        p.i2cAddress = 0x50;
    }
    p.twoByteAddress = obj.value(QStringLiteral("two_byte_address")).toBool(false);
    p.blockBits = static_cast<quint8>(std::min<qint64>(
        std::max<qint64>(0, obj.value(QStringLiteral("block_bits")).toInteger(0)), 3));
    p.pageSize = static_cast<quint16>(clampedUInt32(obj.value(QStringLiteral("page_size")), 16, 4096));
    p.writeCycleMs = clampedUInt32(obj.value(QStringLiteral("write_cycle_ms")), 5, 60000);
    p.family = obj.value(QStringLiteral("family")).toString().trimmed();

    p.counterOffset = clampedUInt32(obj.value(QStringLiteral("counter_offset")),
                                    kCounterUnset, kCounterUnset);
    p.counterSize = clampedUInt32(obj.value(QStringLiteral("counter_size")), 2, 4);

    p.protectedRanges = parseRanges(obj.value(QStringLiteral("protected_ranges")), p.sizeBytes);
    p.allowedRanges = parseRanges(obj.value(QStringLiteral("allowed_modify_ranges")), p.sizeBytes);

    return p;
}

QJsonObject ChipProfile::toJson() const
{
    QJsonObject obj;
    obj[QStringLiteral("id")] = id;
    obj[QStringLiteral("name")] = name;
    obj[QStringLiteral("size_bytes")] = static_cast<qint64>(sizeBytes);
    obj[QStringLiteral("i2c_address")] = static_cast<int>(i2cAddress);
    obj[QStringLiteral("two_byte_address")] = twoByteAddress;
    obj[QStringLiteral("block_bits")] = static_cast<int>(blockBits);
    obj[QStringLiteral("page_size")] = static_cast<int>(pageSize);
    obj[QStringLiteral("write_cycle_ms")] = static_cast<qint64>(writeCycleMs);
    obj[QStringLiteral("family")] = family;
    obj[QStringLiteral("counter_offset")] = static_cast<qint64>(counterOffset);
    obj[QStringLiteral("counter_size")] = static_cast<qint64>(counterSize);

    QJsonArray ranges;
    for (const auto &range : protectedRanges) {
        QJsonObject item;
        item[QStringLiteral("start")] = static_cast<qint64>(range.first);
        item[QStringLiteral("end")] = static_cast<qint64>(range.second);
        ranges.append(item);
    }
    obj[QStringLiteral("protected_ranges")] = ranges;

    QJsonArray allowed;
    for (const auto &range : allowedRanges) {
        QJsonObject item;
        item[QStringLiteral("start")] = static_cast<qint64>(range.first);
        item[QStringLiteral("end")] = static_cast<qint64>(range.second);
        allowed.append(item);
    }
    obj[QStringLiteral("allowed_modify_ranges")] = allowed;
    return obj;
}

bool ChipProfile::isValid() const
{
    if (id.isEmpty() || sizeBytes == 0 || sizeBytes > kMaxChipSize) {
        return false;
    }
    if (i2cAddress < 0x08 || i2cAddress > 0x77) {
        return false;
    }
    if (pageSize == 0 || (pageSize & (pageSize - 1)) != 0) {
        return false;  // размер страницы должен быть степенью двойки
    }
    if (blockBits > 3) {
        return false;
    }
    if (counterSize != 1 && counterSize != 2 && counterSize != 4) {
        return false;
    }
    if (counterOffset != kCounterUnset) {
        const quint64 end = static_cast<quint64>(counterOffset) + counterSize;
        if (end > sizeBytes) {
            return false;
        }
    }
    for (const auto &range : protectedRanges) {
        if (range.first >= range.second || range.second > sizeBytes) {
            return false;
        }
    }
    for (const auto &range : allowedRanges) {
        if (range.first >= range.second || range.second > sizeBytes) {
            return false;
        }
    }
    return true;
}

bool ChipProfile::isAddressProtected(quint32 address, quint32 length) const
{
    if (protectedRanges.isEmpty() || length == 0) {
        return false;
    }

    const quint64 start = address;
    const quint64 end = start + length;
    for (const auto &range : protectedRanges) {
        if (start < range.second && end > range.first) {
            return true;
        }
    }
    return false;
}

quint32 ChipProfile::counterMaxValue() const
{
    switch (counterSize) {
    case 1:
        return 0xFFu;
    case 2:
        return 0xFFFFu;
    case 4:
        return 0xFFFFFFFFu;
    default:
        return 0;
    }
}
