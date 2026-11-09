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

#include <cstring>

#include <algorithm>

ChipWorker::ChipWorker(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<ChipOperationResult>("ChipOperationResult");
    qRegisterMetaType<ChipOperationType>("ChipOperationType");
}

ChipWorker::~ChipWorker()
{
    m_cancelled = true;
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
    auto profile = ProfileManager::instance().profile(profileId);
    if (!profile) {
        result.error = ErrorCode::InvalidProfile;
        result.errorMessage = QStringLiteral("Профиль чипа «%1» не найден").arg(profileId);
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

void ChipWorker::requestConnect(const QString &adapterName, quint8 chipAddress, const QString &profileId)
{
    ChipOperationResult result;
    result.type = ChipOperationType::Connect;
    m_cancelled = false;

    if (!loadProfile(profileId, result)) {
        emit operationResult(result);
        emit operationFinished();
        return;
    }

    m_chipAddress = chipAddress;

    const AdapterFactory::AdapterType type = AdapterFactory::typeFromString(adapterName);
    m_adapter = std::shared_ptr<IChipAdapter>(AdapterFactory::createAdapter(type));
    if (!m_adapter) {
        result.error = ErrorCode::AdapterNotFound;
        result.errorMessage = QStringLiteral("Неизвестный тип программатора: %1").arg(adapterName);
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
    result.progressText = QStringLiteral("Программатор подключён, чип %1 отвечает на адресе 0x%2")
                              .arg(m_profile.id)
                              .arg(m_chipAddress, 2, 16, QLatin1Char('0'))
                              .toUpper();
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

    Q_UNUSED(profileId)
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
        result.progressText = QStringLiteral("Найдены устройства: ") + list.join(", ").toUpper();
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
        reportProgress(static_cast<int>((offset * 100u) / chipSize),
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
    m_cancelled = false;

    if (!loadProfile(profileId, result)) {
        result.type = ChipOperationType::Read;
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

    // Проверка размера дампа относительно профиля
    QString validationError;
    if (!Validator::validateSize(data, m_profile, &validationError)) {
        result.error = ErrorCode::InvalidChipSize;
        result.errorMessage = validationError;
        emit operationResult(result);
        emit operationFinished();
        return;
    }

    // Читаем текущее содержимое чипа: нужно для контроля защищённых зон
    ChipOperationResult original = readChip();
    if (original.error != ErrorCode::Success) {
        result.error = original.error;
        result.errorMessage = QStringLiteral("Не удалось прочитать исходный дамп: %1").arg(original.errorMessage);
        emit operationResult(result);
        emit operationFinished();
        return;
    }

    if (!Validator::validateProtectedRanges(original.data, data, m_profile, &validationError)) {
        result.error = ErrorCode::ProtectedArea;
        result.errorMessage = validationError + QStringLiteral(". Запись отменена для защиты критичных данных.");
        emit operationResult(result);
        emit operationFinished();
        return;
    }

    const quint32 chipSize = m_profile.sizeBytes;
    const quint32 blockSize = std::max<quint32>(1, m_profile.pageSize) * 8;

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
                result.errorMessage = QString::fromStdString(m_adapter->lastError());
                emit operationResult(result);
                emit operationFinished();
                return;
            }
        }

        offset += chunk;
        reportProgress(static_cast<int>((offset * 100u) / chipSize),
                       QStringLiteral("Запись... %1 из %2 байт").arg(offset).arg(chipSize));
    }

    result.progress = 100;
    result.progressText = QStringLiteral("Запись завершена: %1 байт").arg(chipSize);
    Logger::instance().info(result.progressText);
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
        result.errorMessage = QStringLiteral("Обнаружено %1 различий").arg(result.differences.size());
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

    const auto modified = CounterReset::resetCounter(data, m_profile);
    if (!modified.has_value()) {
        result.error = ErrorCode::InvalidProfile;
        result.errorMessage = QStringLiteral("В профиле «%1» не задано смещение счётчика").arg(m_profile.id);
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

void ChipWorker::requestBackup(const QByteArray &data, const QString &chipId, const QString &cartridgeModel)
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

    const QString backupDir = DumpManager::backupDirectory();
    if (!QDir().mkpath(backupDir)) {
        result.error = ErrorCode::FileWriteError;
        result.errorMessage = QStringLiteral("Не удалось создать каталог резервных копий: %1").arg(backupDir);
        emit operationResult(result);
        emit operationFinished();
        return;
    }

    const QString filePath = DumpManager::backupFileName(QStringLiteral("backup"));
    if (!DumpManager::saveDump(filePath, data, chipId, cartridgeModel)) {
        result.error = ErrorCode::FileWriteError;
        result.errorMessage = QStringLiteral("Не удалось сохранить резервную копию: %1").arg(filePath);
        emit operationResult(result);
        emit operationFinished();
        return;
    }

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
        ChipOperationResult tmp;
        if (loadProfile(profileId, tmp)) {
            QString validationError;
            if (!Validator::validateSize(*data, m_profile, &validationError)) {
                result.error = ErrorCode::InvalidChipSize;
                result.errorMessage = validationError;
                emit operationResult(result);
                emit operationFinished();
                return;
            }
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

