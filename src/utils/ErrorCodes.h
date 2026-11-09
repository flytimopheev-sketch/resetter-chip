#ifndef ERROR_CODES_H
#define ERROR_CODES_H

#include <QString>

// Коды ошибок программы
enum class ErrorCode {
    Success = 0,
    AdapterNotFound = 1,
    AdapterOpenFailed = 2,
    AdapterNotOpened = 3,
    ChipNotResponding = 4,
    InvalidChipSize = 5,
    ChecksumMismatch = 6,
    WriteVerificationFailed = 7,
    ProtectedArea = 8,
    BackupNotFound = 9,
    InsufficientPermissions = 10,
    InvalidProfile = 11,
    FileReadError = 12,
    FileWriteError = 13,
    OperationCancelled = 14,
    UnknownError = 99
};

// Человекочитаемое описание кода ошибки.
// Единственный экземпляр таблицы на всю программу (без копий в каждом .cpp).
inline QString errorToString(ErrorCode code)
{
    switch (code) {
    case ErrorCode::Success:               return QStringLiteral("Операция выполнена успешно");
    case ErrorCode::AdapterNotFound:       return QStringLiteral("Программатор CH341A не найден");
    case ErrorCode::AdapterOpenFailed:     return QStringLiteral("Не удалось открыть USB-устройство: недостаточно прав");
    case ErrorCode::AdapterNotOpened:      return QStringLiteral("Программатор не открыт");
    case ErrorCode::ChipNotResponding:     return QStringLiteral("Чип не отвечает по указанному I2C-адресу");
    case ErrorCode::InvalidChipSize:       return QStringLiteral("Размер дампа не соответствует выбранному чипу");
    case ErrorCode::ChecksumMismatch:      return QStringLiteral("Контрольная сумма не совпадает");
    case ErrorCode::WriteVerificationFailed: return QStringLiteral("Запись завершена, но верификация не пройдена");
    case ErrorCode::ProtectedArea:         return QStringLiteral("Область памяти защищена профилем чипа");
    case ErrorCode::BackupNotFound:        return QStringLiteral("Не найдена резервная копия");
    case ErrorCode::InsufficientPermissions: return QStringLiteral("Недостаточно прав для доступа к USB-устройству");
    case ErrorCode::InvalidProfile:        return QStringLiteral("Неверный или отсутствующий профиль чипа");
    case ErrorCode::FileReadError:         return QStringLiteral("Ошибка чтения файла");
    case ErrorCode::FileWriteError:        return QStringLiteral("Ошибка записи файла");
    case ErrorCode::OperationCancelled:    return QStringLiteral("Операция отменена пользователем");
    case ErrorCode::UnknownError:          return QStringLiteral("Неизвестная ошибка");
    }
    return QStringLiteral("Неизвестная ошибка");
}

#endif // ERROR_CODES_H
