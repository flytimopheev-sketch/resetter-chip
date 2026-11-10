#ifndef CHIP_WORKER_H
#define CHIP_WORKER_H

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QVector>

#include <atomic>
#include <memory>

#include "adapters/IChipAdapter.h"
#include "core/ChipProfile.h"
#include "utils/ErrorCodes.h"

// Типы операций с чипом
enum class ChipOperationType {
    Connect,
    Disconnect,
    Scan,
    Read,
    Write,
    Verify,
    ResetCounter,
    Backup,
    Restore,
    Compare
};

// Результат операции
struct ChipOperationResult {
    ChipOperationType type = ChipOperationType::Read;
    ErrorCode error = ErrorCode::Success;
    QString errorMessage;
    QByteArray data;
    QVector<quint32> differences;
    QVector<quint8> detectedAddresses;
    int progress = 0;
    QString progressText;
    QString backupPath;          // путь автоматической резервной копии перед записью
};

Q_DECLARE_METATYPE(ChipOperationResult)
Q_DECLARE_METATYPE(ChipOperationType)

// Рабочий объект. Все слоты выполняются в рабочем потоке,
// поэтому состояние (адаптер, профиль, буферы) не требует блокировок.
class ChipWorker : public QObject
{
    Q_OBJECT

public:
    explicit ChipWorker(QObject *parent = nullptr);
    ~ChipWorker() override;

    // Потокобезопасная отмена (флаг атомарный)
    void cancel();

public slots:
    void requestConnect(const QString &adapterSpec, int chipAddress, const QString &profileId);
    void requestDisconnect();
    void requestScan(const QString &profileId);
    void requestRead(const QString &profileId);
    void requestWrite(const QByteArray &data, const QString &profileId);
    void requestVerify(const QByteArray &expected, const QString &profileId);
    void requestResetCounter(const QByteArray &data, const QString &profileId);
    void requestBackup(const QByteArray &data, const QString &chipId, const QString &cartridgeModel);
    void requestRestore(const QString &backupPath, const QString &profileId);
    void requestCompare(const QByteArray &a, const QByteArray &b);

signals:
    void operationResult(const ChipOperationResult &result);
    void operationProgress(int value, const QString &text);
    void operationFinished();

private:
    std::shared_ptr<IChipAdapter> m_adapter;
    ChipProfile m_profile;
    quint8 m_chipAddress = 0x50;

    std::atomic<bool> m_cancelled{false};

    // Загрузить профиль по ID (чип или картридж); при ошибке заполняет result
    bool loadProfile(const QString &profileId, ChipOperationResult &result);

    // Применить параметры профиля к адаптеру
    void applyProfileToAdapter();

    // Полное чтение чипа (с учётом размера профиля)
    ChipOperationResult readChip();

    // Автоматическая резервная копия перед записью
    bool saveSafetyBackup(const QByteArray &data, QString *filePath, QString *error);

    void reportProgress(int value, const QString &text);
};

#endif // CHIP_WORKER_H
