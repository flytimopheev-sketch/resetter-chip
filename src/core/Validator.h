#ifndef VALIDATOR_H
#define VALIDATOR_H

#include "ChipProfile.h"
#include <QByteArray>
#include <QString>

// Валидация дампов перед записью в чип
class Validator
{
public:
    // Проверить, что дамп соответствует профилю чипа по размеру
    static bool validateSize(const QByteArray &dump, const ChipProfile &profile, QString *errorMsg = nullptr);

    // Проверить, что в защищённые зоны не записываются критичные данные
    static bool validateProtectedRanges(const QByteArray &original, const QByteArray &modified,
                                        const ChipProfile &profile, QString *errorMsg = nullptr);

    // Проверить, что изменённые байты находятся только в разрешённых полях
    static bool validateChanges(const QByteArray &original, const QByteArray &modified,
                                const QVector<QPair<quint32, quint32>> &allowedRanges,
                                QString *errorMsg = nullptr);

    // Общая валидация перед записью
    static bool validateBeforeWrite(const QByteArray &original, const QByteArray &modified,
                                    const ChipProfile &profile,
                                    const QVector<QPair<quint32, quint32>> &allowedRanges,
                                    QString *errorMsg = nullptr);
};

#endif // VALIDATOR_H