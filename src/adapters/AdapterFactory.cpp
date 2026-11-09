#include "AdapterFactory.h"
#include "Ch341AI2cAdapter.h"

std::unique_ptr<IChipAdapter> AdapterFactory::createAdapter(AdapterType type)
{
    switch (type) {
    case AdapterType::Ch341AI2c:
        return std::make_unique<Ch341AI2cAdapter>();
    default:
        return nullptr;
    }
}

AdapterFactory::AdapterType AdapterFactory::typeFromString(const QString &name)
{
    if (name.contains("CH341A", Qt::CaseInsensitive)) {
        return AdapterType::Ch341AI2c;
    }
    return AdapterType::Unknown;
}