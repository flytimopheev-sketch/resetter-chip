#ifndef ADAPTER_FACTORY_H
#define ADAPTER_FACTORY_H

#include "IChipAdapter.h"
#include <memory>
#include <QString>

// Фабрика создания адаптеров программаторов
class AdapterFactory
{
public:
    enum class AdapterType {
        Ch341AI2c,
        Unknown
    };

    static std::unique_ptr<IChipAdapter> createAdapter(AdapterType type);
    static AdapterType typeFromString(const QString &name);
};

#endif // ADAPTER_FACTORY_H