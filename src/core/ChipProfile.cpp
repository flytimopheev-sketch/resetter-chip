#include "ChipProfile.h"

#include <QJsonArray>

ChipProfile ChipProfile::fromJson(const QJsonObject &obj)
{
    ChipProfile p;
    p.id = obj.value(QStringLiteral("id")).toString();
    p.name = obj.value(QStringLiteral("name")).toString();
    p.sizeBytes = static_cast<quint32>(obj.value(QStringLiteral("size_bytes")).toDouble());
    p.i2cAddress = static_cast<quint8>(obj.value(QStringLiteral("i2c_address")).toInt(0x50));
    p.twoByteAddress = obj.value(QStringLiteral("two_byte_address")).toBool(false);
    p.blockBits = static_cast<quint8>(obj.value(QStringLiteral("block_bits")).toInt(0));
    p.pageSize = static_cast<quint16>(obj.value(QStringLiteral("page_size")).toInt(16));
    p.writeCycleMs = static_cast<quint32>(obj.value(QStringLiteral("write_cycle_ms")).toInt(5));
    p.family = obj.value(QStringLiteral("family")).toString();

    if (obj.contains(QStringLiteral("counter_offset"))) {
        p.counterOffset = static_cast<quint32>(obj.value(QStringLiteral("counter_offset")).toDouble());
    }
    if (obj.contains(QStringLiteral("counter_size"))) {
        p.counterSize = static_cast<quint32>(obj.value(QStringLiteral("counter_size")).toInt(2));
    }

    const QJsonArray ranges = obj.value(QStringLiteral("protected_ranges")).toArray();
    for (const QJsonValue &value : ranges) {
        const QJsonObject range = value.toObject();
        const quint32 start = static_cast<quint32>(range.value(QStringLiteral("start")).toDouble());
        const quint32 end = static_cast<quint32>(range.value(QStringLiteral("end")).toDouble());
        if (end > start) {
            p.protectedRanges.append(qMakePair(start, end));
        }
    }

    return p;
}

QJsonObject ChipProfile::toJson() const
{
    QJsonObject obj;
    obj[QStringLiteral("id")] = id;
    obj[QStringLiteral("name")] = name;
    obj[QStringLiteral("size_bytes")] = static_cast<double>(sizeBytes);
    obj[QStringLiteral("i2c_address")] = static_cast<int>(i2cAddress);
    obj[QStringLiteral("two_byte_address")] = twoByteAddress;
    obj[QStringLiteral("block_bits")] = static_cast<int>(blockBits);
    obj[QStringLiteral("page_size")] = static_cast<int>(pageSize);
    obj[QStringLiteral("write_cycle_ms")] = static_cast<double>(writeCycleMs);
    obj[QStringLiteral("family")] = family;
    obj[QStringLiteral("counter_offset")] = static_cast<double>(counterOffset);
    obj[QStringLiteral("counter_size")] = static_cast<double>(counterSize);

    QJsonArray ranges;
    for (const auto &range : protectedRanges) {
        QJsonObject item;
        item[QStringLiteral("start")] = static_cast<double>(range.first);
        item[QStringLiteral("end")] = static_cast<double>(range.second);
        ranges.append(item);
    }
    obj[QStringLiteral("protected_ranges")] = ranges;
    return obj;
}

bool ChipProfile::isAddressProtected(quint32 address, quint32 length) const
{
    if (protectedRanges.isEmpty() || length == 0) {
        return false;
    }

    const quint32 endAddr = address + length;  // конец не включается
    for (const auto &range : protectedRanges) {
        if (address < range.second && endAddr > range.first) {
            return true;
        }
    }
    return false;
}
