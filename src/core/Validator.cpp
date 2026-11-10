#include "Validator.h"

namespace {

bool rangesOverlap(quint64 aStart, quint64 aEnd, quint64 bStart, quint64 bEnd)
{
    return aStart < bEnd && aEnd > bStart;
}

void setError(QString *errorMsg, const QString &text)
{
    if (errorMsg) {
        *errorMsg = text;
    }
}

} // namespace

bool Validator::validateSize(const QByteArray &dump, const ChipProfile &profile, QString *errorMsg)
{
    if (static_cast<quint32>(dump.size()) != profile.sizeBytes) {
        setError(errorMsg, QStringLiteral("Размер дампа (%1 байт) не соответствует чипу %2 (%3 байт)")
                               .arg(dump.size())
                               .arg(profile.id)
                               .arg(profile.sizeBytes));
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

    if (original.size() != modified.size()) {
        setError(errorMsg, QStringLiteral("Размеры исходного и нового дампа не совпадают"));
        return false;
    }

    const quint64 size = static_cast<quint64>(original.size());
    for (const auto &range : profile.protectedRanges) {
        const quint64 start = range.first;
        const quint64 end = qMin<quint64>(range.second, size);
        if (start >= end) {
            continue;
        }

        for (quint64 addr = start; addr < end; ++addr) {
            const int index = static_cast<int>(addr);
            if (original.at(index) != modified.at(index)) {
                setError(errorMsg, QStringLiteral("Обнаружено изменение в защищённой зоне по адресу 0x%1")
                                       .arg(addr, 4, 16, QLatin1Char('0')));
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
    if (original.size() != modified.size()) {
        setError(errorMsg, QStringLiteral("Размеры исходного и нового дампа не совпадают"));
        return false;
    }

    // Пустой список разрешённых зон означает «ограничений нет»:
    // защищённые диапазоны проверяются отдельным методом.
    if (allowedRanges.isEmpty()) {
        return true;
    }

    for (int i = 0; i < modified.size(); ++i) {
        if (original.at(i) == modified.at(i)) {
            continue;
        }

        bool allowed = false;
        for (const auto &range : allowedRanges) {
            if (rangesOverlap(static_cast<quint64>(i), static_cast<quint64>(i) + 1,
                              range.first, range.second)) {
                allowed = true;
                break;
            }
        }

        if (!allowed) {
            setError(errorMsg, QStringLiteral("Несанкционированное изменение по адресу 0x%1 (вне разрешённых зон)")
                                   .arg(i, 4, 16, QLatin1Char('0')));
            return false;
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
    if (original.size() != modified.size()) {
        setError(errorMsg, QStringLiteral("Размер прочитанного дампа не совпадает с новым дампом"));
        return false;
    }
    if (!validateProtectedRanges(original, modified, profile, errorMsg)) {
        return false;
    }
    return validateChanges(original, modified, allowedRanges, errorMsg);
}
