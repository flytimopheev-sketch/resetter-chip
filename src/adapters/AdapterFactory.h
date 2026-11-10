#ifndef ADAPTER_FACTORY_H
#define ADAPTER_FACTORY_H

#include "IChipAdapter.h"

#include <QString>

#include <cstdint>
#include <memory>

// Фабрика адаптеров. Спецификация адаптера приходит из UI или из
// командной строки в одном из форматов:
//   ch341a                    — CH341A с автоопределением PID
//   ch341a:1a86:5512          — CH341A-совместимая плата с явным VID/PID
//   serial:/dev/ttyUSB0       — самодельный последовательный программатор
//   serial:                   — он же с автопоиском порта
class AdapterFactory
{
public:
    enum class AdapterType {
        Ch341AI2c,
        SerialI2c,
        Unknown
    };

    struct Spec {
        AdapterType type = AdapterType::Unknown;
        quint16 vendorId = 0;
        quint16 productId = 0;
        QString portName;
        QString error;
    };

    static Spec parseSpec(const QString &spec);
    static std::unique_ptr<IChipAdapter> createAdapter(const QString &spec,
                                                       QString *errorMessage = nullptr);

    // Человекочитаемое имя типа адаптера для журнала
    static QString typeName(AdapterType type);
};

#endif // ADAPTER_FACTORY_H
