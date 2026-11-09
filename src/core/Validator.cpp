#include "Validator.h"

bool Validator::validateSize(const QByteArray &dump, const ChipProfile &profile, QString *errorMsg)
{
    if (static_cast<quint32>(dump.size()) != profile.sizeBytes) {
        if (errorMsg) {
            *errorMsg = QString("Размер дампа (%1 байт) не соответствует чипу %2 (%3 байт)")
                            .arg(dump.size()).arg(profile.id).arg(profile.sizeBytes);
        }
        return false;
    }
    return true;
}

bool Validator::validateProtectedRanges(const QByteArray &original, const QByteArray &modified,
                                        const ChipProfile &profile, QString *errorMsg)
{
    if (profile.protectedRanges.isEmpty()) {
        return true;
    }

    for (const auto &range : profile.protectedRanges) {
        quint32 start = range.first;
        quint32 end = qMin(range.second, static_cast<quint32>(original.size()));
        for (quint32 addr = start; addr < end; ++addr) {
            if (addr >= static_cast<quint32>(modified.size())) {
                break;
            }
            if (original[addr] != modified[addr]) {
                if (errorMsg) {
                    *errorMsg = QString("Обнаружено изменение в защищённой зоне по адресу 0x%1")
                                    .arg(addr, 4, 16, QLatin1Char('0'));
                }
                return false;
            }
        }
    }
    return true;
}

bool Validator::validateChanges(const QByteArray &original, const QByteArray &modified,
                                const QVector<QPair<quint32, quint32>> &allowedRanges,
                                QString *errorMsg)
{
    int minSize = qMin(original.size(), modified.size());
    for (int i = 0; i < minSize; ++i) {
        if (original[i] != modified[i]) {
            bool inAllowedRange = false;
            for (const auto &range : allowedRanges) {
                if (i >= range.first && i < range.second) {
                    inAllowedRange = true;
                    break;
                }
            }
            if (!inAllowedRange) {
                if (errorMsg) {
                    *errorMsg = QString("Несанкционированное изменение по адресу 0x%1 (вне разрешённых зон)")
                                    .arg(i, 4, 16, QLatin1Char('0'));
                }
                return false;
            }
        }
    }
    return true;
}

bool Validator::validateBeforeWrite(const QByteArray &original, const QByteArray &modified,
                                    const ChipProfile &profile,
                                    const QVector<QPair<quint32, quint32>> &allowedRanges,
                                    QString *errorMsg)
{
    if (!validateSize(modified, profile, errorMsg)) {
        return false;
    }
    if (!validateProtectedRanges(original, modified, profile, errorMsg)) {
        return false;
    }
    if (!validateChanges(original, modified, allowedRanges, errorMsg)) {
        return false;
    }
    return true;
}