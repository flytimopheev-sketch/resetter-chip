#ifndef VALIDATOR_H
#define VALIDATOR_H

#include "ChipProfile.h"

#include <QByteArray>
#include <QString>
#include <QVector>

// Проверки дампа перед записью. Все методы возвращают true, если изменение
// безопасно; при ошибке заполняют errorMsg.
class Validator
{
public:
    static bool validateSize(const QByteArray &dump, const ChipProfile &profile,
                             QString *errorMsg = nullptr);

    static bool validateProtectedRanges(const QByteArray &original, const QByteArray &modified,
                                        const ChipProfile &profile,
                                        QString *errorMsg = nullptr);

    static bool validateChanges(const QByteArray &original, const QByteArray &modified,
                                const QVector<QPair<quint32, quint32>> &allowedRanges,
                                QString *errorMsg = nullptr);

    static bool validateBeforeWrite(const QByteArray &original, const QByteArray &modified,
                                    const ChipProfile &profile,
                                    const QVector<QPair<quint32, quint32>> &allowedRanges,
                                    QString *errorMsg = nullptr);
};

#endif // VALIDATOR_H
