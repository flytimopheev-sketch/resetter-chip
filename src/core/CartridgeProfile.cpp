#include "CartridgeProfile.h"

#include <QJsonArray>
#include <QJsonValue>

#include <algorithm>

namespace {

QStringList jsonStringList(const QJsonValue &value)
{
    QStringList result;
    if (value.isArray()) {
        const QJsonArray array = value.toArray();
        for (const QJsonValue &item : array) {
            const QString text = item.toString().trimmed();
            if (!text.isEmpty()) {
                result.append(text);
            }
        }
    } else if (value.isString()) {
        const QString text = value.toString().trimmed();
        if (!text.isEmpty()) {
            result.append(text);
        }
    }
    return result;
}

QVector<CartridgeRange> jsonRanges(const QJsonValue &value)
{
    QVector<CartridgeRange> ranges;
    if (!value.isArray()) {
        return ranges;
    }

    const QJsonArray array = value.toArray();
    for (const QJsonValue &item : array) {
        const QJsonObject obj = item.toObject();
        const qint64 start = obj.value(QStringLiteral("start")).toInteger(-1);
        const qint64 end = obj.value(QStringLiteral("end")).toInteger(-1);
        if (start < 0 || end <= start) {
            continue;
        }

        CartridgeRange range;
        range.start = static_cast<quint32>(start);
        range.end = static_cast<quint32>(end);
        range.description = obj.value(QStringLiteral("description")).toString();
        ranges.append(range);
    }

    std::sort(ranges.begin(), ranges.end(),
              [](const CartridgeRange &a, const CartridgeRange &b) {
                  return a.start < b.start;
              });
    return ranges;
}

QJsonArray rangesToJson(const QVector<CartridgeRange> &ranges)
{
    QJsonArray array;
    for (const CartridgeRange &range : ranges) {
        QJsonObject obj;
        obj[QStringLiteral("start")] = static_cast<qint64>(range.start);
        obj[QStringLiteral("end")] = static_cast<qint64>(range.end);
        if (!range.description.isEmpty()) {
            obj[QStringLiteral("description")] = range.description;
        }
        array.append(obj);
    }
    return array;
}

} // namespace

CartridgeProfile CartridgeProfile::fromJson(const QJsonObject &obj)
{
    CartridgeProfile p;
    p.id = obj.value(QStringLiteral("id")).toString().trimmed();
    p.name = obj.value(QStringLiteral("name")).toString().trimmed();
    p.chipProfileId = obj.value(QStringLiteral("chip_profile")).toString().trimmed();

    p.printerModels = jsonStringList(obj.value(QStringLiteral("printer_models")));
    p.cartridgeModels = jsonStringList(obj.value(QStringLiteral("cartridge_models")));

    if (obj.contains(QStringLiteral("counter_offset"))) {
        p.counterOffset = static_cast<quint32>(
            qMax<qint64>(0, obj.value(QStringLiteral("counter_offset")).toInteger(0)));
    }
    if (obj.contains(QStringLiteral("counter_size"))) {
        p.counterSize = static_cast<quint32>(
            qMax<qint64>(0, obj.value(QStringLiteral("counter_size")).toInteger(0)));
    }
    if (obj.contains(QStringLiteral("counter_initial_value"))) {
        p.counterInitialValue = static_cast<quint32>(
            qMax<qint64>(0, obj.value(QStringLiteral("counter_initial_value")).toInteger(0)));
    }
    if (obj.contains(QStringLiteral("page_resource"))) {
        p.pageResource = static_cast<quint32>(
            qMax<qint64>(0, obj.value(QStringLiteral("page_resource")).toInteger(0)));
    }

    p.protectedRanges = jsonRanges(obj.value(QStringLiteral("protected_ranges")));
    p.allowedModifyRanges = jsonRanges(obj.value(QStringLiteral("allowed_modify_ranges")));

    return p;
}

QJsonObject CartridgeProfile::toJson() const
{
    QJsonObject obj;
    obj[QStringLiteral("id")] = id;
    obj[QStringLiteral("name")] = name;
    obj[QStringLiteral("chip_profile")] = chipProfileId;

    QJsonArray printers;
    for (const QString &model : printerModels) {
        printers.append(model);
    }
    obj[QStringLiteral("printer_models")] = printers;

    QJsonArray cartridges;
    for (const QString &model : cartridgeModels) {
        cartridges.append(model);
    }
    obj[QStringLiteral("cartridge_models")] = cartridges;

    obj[QStringLiteral("counter_offset")] = static_cast<qint64>(counterOffset);
    obj[QStringLiteral("counter_size")] = static_cast<qint64>(counterSize);
    obj[QStringLiteral("counter_initial_value")] = static_cast<qint64>(counterInitialValue);
    obj[QStringLiteral("page_resource")] = static_cast<qint64>(pageResource);
    obj[QStringLiteral("protected_ranges")] = rangesToJson(protectedRanges);
    obj[QStringLiteral("allowed_modify_ranges")] = rangesToJson(allowedModifyRanges);
    return obj;
}

bool CartridgeProfile::isValid() const
{
    if (id.isEmpty() || name.isEmpty() || chipProfileId.isEmpty()) {
        return false;
    }
    if (counterSize != 0 && counterSize != 1 && counterSize != 2 && counterSize != 4) {
        return false;
    }
    return true;
}
