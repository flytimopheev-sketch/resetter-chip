#include "ChipWorker.h"

#include "adapters/AdapterFactory.h"
#include "core/CounterReset.h"
#include "core/DumpManager.h"
#include "core/ProfileManager.h"
#include "core/Validator.h"
#include "utils/Logger.h"

#include <QDir>
#include <QStringList>
#include <QThread>

#include <algorithm>
#include <cstring>

ChipWorker::ChipWorker(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<ChipOperationResult>("ChipOperationResult");
    qRegisterMetaType<ChipOperationType>("ChipOperationType");
}

ChipWorker::~ChipWorker()
{
    m_cancelled = true;
    if (m_adapter) {
        m_adapter->close();
        m_adapter.reset();
    }
}

void ChipWorker::cancel()
{
    m_cancelled = true;
}

void ChipWorker::reportProgress(int value, const QString &text)
{
    emit operationProgress(std::clamp(value, 0, 100), text);
}

bool ChipWorker::loadProfile(const QString &profileId, ChipOperationResult &result)
{
    const auto profile = ProfileManager::instance().resolveProfile(profileId);
    if (!profile) {
        result.error = ErrorCode::InvalidProfile;
        result.errorMessage = QStringLiteral("Профиль «%1» не найден или повреждён").arg(profileId);
        return false;
    }
    m_profile = *profile;
    return true;
}

void ChipWorker::applyProfileToAdapter()
{
    if (!m_adapter) {
        return;
    }

    ChipParams params;
    params.deviceAddress = m_chipAddress;
    params.chipSize = m_profile.sizeBytes;
    params.pageSize = m_profile.pageSize;
    params.twoByteAddress = m_profile.twoByteAddress;
    params.blockBits = m_profile.blockBits;
    params.writeCycleMs = m_profile.writeCycleMs;
    m_adapter->setChipParams(params);
}

void ChipWorker::requestConnect(const QString &adapterSpec, int chipAddress,
                                const QString &profileId)
{
    ChipOperationResult result;
    result.type = ChipOperationType::Connect;
    m_cancelled = false;

    if (!loadProfile(profileId, result)) {
        emit operationResult(result);
        emit operationFinished();
        return;
    }

    m_chipAddress = static_cast<quint8>(chipAddress & 0x7F);

    QString factoryError;
    m_adapter = std::shared_ptr<IChipAdapter>(
        AdapterFactory::createAdapter(adapterSpec, &factoryError));
    if (!m_adapter) {
        result.error = ErrorCode::AdapterNotFound;
        result.errorMessage = factoryError.isEmpty()
                                  ? QStringLiteral("Неизвестный тип программатора: %1").arg(adapterSpec)
                                  : factoryError;
        emit operationResult(result);
        emit operationFinished();
        return;
    }

    applyProfileToAdapter();

    if (!m_adapter->open()) {
        result.error = ErrorCode::AdapterOpenFailed;
        result.errorMessage = QString::fromStdString(m_adapter->lastError());
        m_adapter.reset();
        emit operationResult(result);
        emit operationFinished();
        return;
    }

    // Проверка связи с чипом: чтение первого байта
    const auto probe = m_adapter->read(0, 1);
    if (probe.empty()) {
        result.error = ErrorCode::ChipNotResponding;
        result.errorMessage = QString::fromStdString(m_adapter->lastError());
        m_adapter->close();
        m_adapter.reset();
        emit operationResult(result);
        emit operationFinished();
        return;
    }

    result.progress = 100;
    result.progressText = QStringLiteral("Подключено: %1; чип %2 отвечает на адресе 0x%3")
                              .arg(m_adapter->adapterName(), m_profile.id)
                              .arg(m_chipAddress, 2, 16, QLatin1Char('0'));
    Logger::instance().info(result.progressText);
    emit operationResult(result);
    emit operationFinished();
}

void ChipWorker::requestDisconnect()
{
    ChipOperationResult result;
    result.type = ChipOperationType::Disconnect;

    if (m_adapter) {
        m_adapter->close();
        m_adapter.reset();
    }

    result.progress = 100;
    result.progressText = QStringLiteral("Программатор отключён");
    emit operationResult(result);
    emit operationFinished();
}

void ChipWorker::requestScan(const QString &profileId)
{
    Q_UNUSED(profileId)

    ChipOperationResult result;
    result.type = ChipOperationType::Scan;
    m_cancelled = false;

    if (!m_adapter || !m_adapter->isOpen()) {
        result.error = ErrorCode::AdapterNotOpened;
        result.errorMessage = errorToString(result.error);
        emit operationResult(result);
        emit operationFinished();
        return;
    }

    const std::vector<uint8_t> detected = m_adapter->scanBus();
    for (uint8_t addrByte : detected) {
        result.detectedAddresses.append(static_cast<quint8>(addrByte));
    }
    result.progress = 100;

    if (result.detectedAddresses.isEmpty()) {
        result.progressText = QStringLiteral("Устройства на шине I2C не обнаружены");
    } else {
        QStringList list;
        for (quint8 addr : result.detectedAddresses) {
            list << QStringLiteral("0x%1").arg(addr, 2, 16, QLatin1Char('0'));
        }
        result.progressText = QStringLiteral("Найдены устройства: ") + list.join(QStringLiteral(", "));
    }
    Logger::instance().info(result.progressText);
    emit operationResult(result);
    emit operationFinished();
}

ChipOperationResult ChipWorker::readChip()
{
    ChipOperationResult result;
    result.type = ChipOperationType::Read;

    if (!m_adapter || !m_adapter->isOpen()) {
        result.error = ErrorCode::AdapterNotOpened;
        result.errorMessage = errorToString(result.error);
        return result;
    }

    const quint32 chipSize = m_profile.sizeBytes;
    if (chipSize == 0) {
        result.error = ErrorCode::InvalidProfile;
        result.errorMessage = errorToString(result.error);
        return result;
    }

    // Читаем крупными блоками: адаптер сам делит их на USB/I2C-транзакции.
    const quint32 blockSize = std::max<quint32>(1, m_profile.pageSize) * 8;

    QByteArray data;
    data.reserve(static_cast<int>(chipSize));

    for (quint32 offset = 0; offset < chipSize; ) {
        if (m_cancelled) {
            result.error = ErrorCode::OperationCancelled;
            result.errorMessage = errorToString(result.error);
            return result;
        }

        const quint32 toRead = std::min(blockSize, chipSize - offset);
        const auto bytes = m_adapter->read(offset, toRead);

        if (bytes.size() != toRead) {
            result.error = ErrorCode::ChipNotResponding;
            result.errorMessage = QString::fromStdString(m_adapter->lastError());
            return result;
        }

        data.append(reinterpret_cast<const char *>(bytes.data()), static_cast<int>(bytes.size()));
        offset += toRead;
        reportProgress(static_cast<int>((static_cast<quint64>(offset) * 100u) / chipSize),
                       QStringLiteral("Чтение... %1 из %2 байт").arg(offset).arg(chipSize));
    }

    result.data = data;
    result.progress = 100;
    result.progressText = QStringLiteral("Прочитано %1 байт").arg(data.size());
    return result;
}

void ChipWorker::requestRead(const QString &profileId)
{
    ChipOperationResult result;
    result.type = ChipOperationType::Read;
    m_cancelled = false;

    if (!loadProfile(profileId, result)) {
        emit operationResult(result);
        emit operationFinished();
        return;
    }
    applyProfileToAdapter();

    result = readChip();
    if (result.error == ErrorCode::Success) {
        Logger::instance().info(result.progressText);
    } else {
        Logger::instance().error(result.errorMessage);
    }

    emit operationResult(result);
    emit operationFinished();
}

bool ChipWorker::saveSafetyBackup(const QByteArray &data, QString *filePath, QString *error)
{
    const QString path = DumpManager::backupFileName(
        QStringLiteral("before_write_%1").arg(m_profile.id));
    if (!DumpManager::saveDump(path, data, m_profile.id, QString())) {
        if (error) {
            *error = QStringLiteral("Не удалось сохранить автоматическую резервную копию: %1").arg(path);
        }
        return false;
    }

    if (filePath) {
        *filePath = path;
    }
    Logger::instance().info(QStringLiteral("Автоматическая резервная копия: ") + path);
    return true;
}

void ChipWorker::requestWrite(const QByteArray &data, const QString &profileId)
{
    ChipOperationResult result;
    result.type = ChipOperationType::Write;
    m_cancelled = false;

    if (!loadProfile(profileId, result)) {
        emit operationResult(result);
        emit operationFinished();
        return;
    }

    if (!m_adapter || !m_adapter->isOpen()) {
        result.error = ErrorCode::AdapterNotOpened;
        result.errorMessage = errorToString(result.error);
        emit operationResult(result);
        emit operationFinished();
        return;
    }

    applyProfileToAdapter();

    QString validationError;
    if (!Validator::validateSize(data, m_profile, &validationError)) {
        result.error = ErrorCode::InvalidChipSize;
        result.errorMessage = validationError;
        emit operationResult(result);
        emit operationFinished();
        return;
    }

    // Читаем текущее содержимое чипа: нужно для контроля защищённых зон
    // и для пропуска неизменённых блоков.
    ChipOperationResult original = readChip();
    if (original.error != ErrorCode::Success) {
        result.error = original.error;
        result.errorMessage = QStringLiteral("Не удалось прочитать исходный дамп: %1")
                                  .arg(original.errorMessage);
        emit operationResult(result);
        emit operationFinished();
        return;
    }

    if (!Validator::validateBeforeWrite(original.data, data, m_profile,
                                        m_profile.allowedRanges, &validationError)) {
        result.error = ErrorCode::ProtectedArea;
        result.errorMessage = validationError
                              + QStringLiteral(". Запись отменена для защиты критичных данных.");
        emit operationResult(result);
        emit operationFinished();
        return;
    }

    if (DumpManager::compareDumps(original.data, data).isEmpty()) {
        result.progress = 100;
        result.progressText = QStringLiteral("Изменений нет — запись в чип не требуется");
        Logger::instance().info(result.progressText);
        emit operationResult(result);
        emit operationFinished();
        return;
    }

    // Страховка: перед изменением чипа всегда сохраняем оригинал.
    QString backupPath;
    QString backupError;
    if (!saveSafetyBackup(original.data, &backupPath, &backupError)) {
        result.error = ErrorCode::FileWriteError;
        result.errorMessage = backupError;
        emit operationResult(result);
        emit operationFinished();
        return;
    }
    result.backupPath = backupPath;

    const quint32 chipSize = m_profile.sizeBytes;
    const quint32 blockSize = std::max<quint32>(1, m_profile.pageSize) * 8;
    bool wroteAny = false;

    for (quint32 offset = 0; offset < chipSize; ) {
        if (m_cancelled) {
            result.error = ErrorCode::OperationCancelled;
            result.errorMessage = errorToString(result.error);
            emit operationResult(result);
            emit operationFinished();
            return;
        }

        const quint32 chunk = std::min(blockSize, chipSize - offset);

        // Пропускаем блоки, которые не изменились — меньше износ чипа
        const bool same = (original.data.mid(static_cast<int>(offset), static_cast<int>(chunk))
                           == data.mid(static_cast<int>(offset), static_cast<int>(chunk)));
        if (!same) {
            std::vector<uint8_t> bytes(static_cast<size_t>(chunk));
            std::memcpy(bytes.data(), data.constData() + offset, static_cast<size_t>(chunk));
            if (!m_adapter->write(offset, bytes)) {
                result.error = ErrorCode::WriteVerificationFailed;
                result.errorMessage = QStringLiteral("Ошибка записи по адресу 0x%1: %2")
                                          .arg(offset, 4, 16, QLatin1Char('0'))
                                          .arg(QString::fromStdString(m_adapter->lastError()));
                emit operationResult(result);
                emit operationFinished();
                return;
            }
            wroteAny = true;
        }

        offset += chunk;
        reportProgress(static_cast<int>((static_cast<quint64>(offset) * 100u) / chipSize),
                       QStringLiteral("Запись... %1 из %2 байт").arg(offset).arg(chipSize));
    }

    if (!wroteAny) {
        result.progress = 100;
        result.progressText = QStringLiteral("Изменений нет — запись в чип не требуется");
        emit operationResult(result);
        emit operationFinished();
        return;
    }

    // Автоматическая верификация после записи — обязательный шаг.
    reportProgress(99, QStringLiteral("Проверка записанных данных..."));
    ChipOperationResult verified = readChip();
    if (verified.error != ErrorCode::Success) {
        result.error = verified.error;
        result.errorMessage = QStringLiteral("Запись выполнена, но проверить чип не удалось: %1")
                                  .arg(verified.errorMessage);
        emit operationResult(result);
        emit operationFinished();
        return;
    }

    result.differences = DumpManager::compareDumps(data, verified.data);
    result.progress = 100;

    if (!result.differences.isEmpty()) {
        result.error = ErrorCode::WriteVerificationFailed;
        result.errorMessage = QStringLiteral("Запись выполнена, но верификация не пройдена: "
                                             "различий %1, первое по адресу 0x%2")
                                  .arg(result.differences.size())
                                  .arg(result.differences.first(), 4, 16, QLatin1Char('0'));
        Logger::instance().error(result.errorMessage);
    } else {
        result.progressText = QStringLiteral("Запись и верификация завершены: %1 байт. "
                                             "Резервная копия: %2")
                                  .arg(chipSize)
                                  .arg(backupPath);
        Logger::instance().info(result.progressText);
    }

    emit operationResult(result);
    emit operationFinished();
}

void ChipWorker::requestVerify(const QByteArray &expected, const QString &profileId)
{
    ChipOperationResult result;
    result.type = ChipOperationType::Verify;
    m_cancelled = false;

    if (!loadProfile(profileId, result)) {
        emit operationResult(result);
        emit operationFinished();
        return;
    }
    applyProfileToAdapter();

    ChipOperationResult actual = readChip();
    if (actual.error != ErrorCode::Success) {
        result.error = actual.error;
        result.errorMessage = actual.errorMessage;
        emit operationResult(result);
        emit operationFinished();
        return;
    }

    result.differences = DumpManager::compareDumps(expected, actual.data);
    if (result.differences.isEmpty()) {
        result.progressText = QStringLiteral("Верификация пройдена: данные совпадают");
        Logger::instance().info(result.progressText);
    } else {
        result.error = ErrorCode::WriteVerificationFailed;
        result.errorMessage = QStringLiteral("Обнаружено различий: %1, первое по адресу 0x%2")
                                  .arg(result.differences.size())
                                  .arg(result.differences.first(), 4, 16, QLatin1Char('0'));
        Logger::instance().error(result.errorMessage);
    }
    result.progress = 100;
    emit operationResult(result);
    emit operationFinished();
}

void ChipWorker::requestResetCounter(const QByteArray &data, const QString &profileId)
{
    ChipOperationResult result;
    result.type = ChipOperationType::ResetCounter;
    m_cancelled = false;

    if (!loadProfile(profileId, result)) {
        emit operationResult(result);
        emit operationFinished();
        return;
    }

    QString validationError;
    if (!Validator::validateSize(data, m_profile, &validationError)) {
        result.error = ErrorCode::InvalidChipSize;
        result.errorMessage = validationError;
        emit operationResult(result);
        emit operationFinished();
        return;
    }

    const auto modified = CounterReset::resetCounter(data, m_profile);
    if (!modified.has_value()) {
        result.error = ErrorCode::InvalidProfile;
        result.errorMessage = QStringLiteral("В профиле «%1» не задано корректное смещение счётчика")
                                  .arg(m_profile.id);
        emit operationResult(result);
        emit operationFinished();
        return;
    }

    result.data = *modified;
    result.progress = 100;
    result.progressText = QStringLiteral("Счётчик страниц обнулён в дампе (смещение 0x%1, размер %2 байт)")
                              .arg(m_profile.counterOffset, 4, 16, QLatin1Char('0'))
                              .arg(m_profile.counterSize);
    Logger::instance().info(result.progressText);
    emit operationResult(result);
    emit operationFinished();
}

void ChipWorker::requestBackup(const QByteArray &data, const QString &chipId,
                               const QString &cartridgeModel)
{
    ChipOperationResult result;
    result.type = ChipOperationType::Backup;

    if (data.isEmpty()) {
        result.error = ErrorCode::FileWriteError;
        result.errorMessage = QStringLiteral("Нет данных для резервного копирования");
        emit operationResult(result);
        emit operationFinished();
        return;
    }

    const QString filePath = DumpManager::backupFileName(
        QStringLiteral("manual_%1").arg(chipId.isEmpty() ? QStringLiteral("chip") : chipId));
    if (!DumpManager::saveDump(filePath, data, chipId, cartridgeModel)) {
        result.error = ErrorCode::FileWriteError;
        result.errorMessage = QStringLiteral("Не удалось сохранить резервную копию: %1").arg(filePath);
        emit operationResult(result);
        emit operationFinished();
        return;
    }

    result.backupPath = filePath;
    result.progress = 100;
    result.progressText = QStringLiteral("Резервная копия сохранена: %1").arg(filePath);
    Logger::instance().info(result.progressText);
    emit operationResult(result);
    emit operationFinished();
}

void ChipWorker::requestRestore(const QString &backupPath, const QString &profileId)
{
    ChipOperationResult result;
    result.type = ChipOperationType::Restore;

    const auto data = DumpManager::loadDump(backupPath);
    if (!data.has_value()) {
        result.error = ErrorCode::BackupNotFound;
        result.errorMessage = QStringLiteral("Не удалось прочитать файл: %1").arg(backupPath);
        emit operationResult(result);
        emit operationFinished();
        return;
    }

    if (!profileId.isEmpty()) {
        if (!loadProfile(profileId, result)) {
            emit operationResult(result);
            emit operationFinished();
            return;
        }

        QString validationError;
        if (!Validator::validateSize(*data, m_profile, &validationError)) {
            result.error = ErrorCode::InvalidChipSize;
            result.errorMessage = validationError;
            emit operationResult(result);
            emit operationFinished();
            return;
        }
    }

    result.data = *data;
    result.progress = 100;
    result.progressText = QStringLiteral("Загружено из резервной копии: %1").arg(backupPath);
    Logger::instance().info(result.progressText);
    emit operationResult(result);
    emit operationFinished();
}

void ChipWorker::requestCompare(const QByteArray &a, const QByteArray &b)
{
    ChipOperationResult result;
    result.type = ChipOperationType::Compare;

    result.differences = DumpManager::compareDumps(a, b);
    result.progress = 100;
    if (result.differences.isEmpty()) {
        result.progressText = QStringLiteral("Дампы идентичны");
    } else {
        result.progressText = QStringLiteral("Найдено различий: %1 (первое по адресу 0x%2)")
                                  .arg(result.differences.size())
                                  .arg(result.differences.first(), 4, 16, QLatin1Char('0'));
    }
    emit operationResult(result);
    emit operationFinished();
}
