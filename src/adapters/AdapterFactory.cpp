#include "AdapterFactory.h"

#include "Ch341AI2cAdapter.h"
#include "SerialI2cAdapter.h"

#include <QStringList>

namespace {
bool parseHex16(const QString &text, quint16 *value)
{
    bool ok = false;
    const uint parsed = text.trimmed().toUInt(&ok, 16);
    if (!ok || parsed > 0xFFFFu) {
        return false;
    }
    *value = static_cast<quint16>(parsed);
    return true;
}
} // namespace

AdapterFactory::Spec AdapterFactory::parseSpec(const QString &spec)
{
    Spec result;
    const QString trimmed = spec.trimmed();
    const QString lower = trimmed.toLower();

    if (lower == QStringLiteral("ch341a") || lower == QStringLiteral("ch341")) {
        result.type = AdapterType::Ch341AI2c;
        return result;
    }

    if (lower.startsWith(QStringLiteral("ch341a:"))) {
        const QStringList parts = trimmed.split(QLatin1Char(':'));
        if (parts.size() != 3
            || !parseHex16(parts.at(1), &result.vendorId)
            || !parseHex16(parts.at(2), &result.productId)) {
            result.error = QStringLiteral("Неверный формат VID:PID. Пример: ch341a:1A86:5512");
            return result;
        }
        result.type = AdapterType::Ch341AI2c;
        return result;
    }

    if (lower.startsWith(QStringLiteral("serial:"))) {
        // Пустой порт допустим: адаптер сам найдёт /dev/ttyUSB0, /dev/ttyACM0
        // или /dev/serial0.
        const QString port = trimmed.mid(QStringLiteral("serial:").size()).trimmed();
        result.type = AdapterType::SerialI2c;
        result.portName = port;
        return result;
    }

    result.error = QStringLiteral("Неизвестный тип программатора: %1").arg(spec);
    return result;
}

std::unique_ptr<IChipAdapter> AdapterFactory::createAdapter(const QString &spec,
                                                            QString *errorMessage)
{
    const Spec parsed = parseSpec(spec);
    if (parsed.type == AdapterType::Unknown) {
        if (errorMessage) {
            *errorMessage = parsed.error;
        }
        return nullptr;
    }

    switch (parsed.type) {
    case AdapterType::Ch341AI2c:
        return std::make_unique<Ch341AI2cAdapter>(parsed.vendorId, parsed.productId);
    case AdapterType::SerialI2c:
        return std::make_unique<SerialI2cAdapter>(parsed.portName);
    case AdapterType::Unknown:
        break;
    }

    if (errorMessage) {
        *errorMessage = parsed.error;
    }
    return nullptr;
}

QString AdapterFactory::typeName(AdapterType type)
{
    switch (type) {
    case AdapterType::Ch341AI2c:
        return QStringLiteral("CH341A (I2C)");
    case AdapterType::SerialI2c:
        return QStringLiteral("Самодельный последовательный I2C");
    case AdapterType::Unknown:
        break;
    }
    return QStringLiteral("Неизвестный программатор");
}
