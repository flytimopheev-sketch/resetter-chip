#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <QByteArray>
#include <QMainWindow>
#include <QString>

#include "core/ChipWorker.h"

class CartridgePanel;
class ConnectionPanel;
class HexEditor;
class LogPanel;
class OperationsPanel;
class StatusBar;

class QThread;

// Главное окно: связывает интерфейс и рабочий поток USB/последовательных операций
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onConnectRequested();
    void onDisconnectRequested();
    void onScanRequested();
    void onProfileChanged(const QString &profileId);

    void onReadRequested();
    void onWriteRequested();
    void onVerifyRequested();
    void onResetCounterRequested();
    void onBackupRequested();
    void onRestoreRequested();
    void onCompareRequested();
    void onSaveDumpRequested();
    void onOpenDumpRequested();
    void onCancelRequested();

    void onHexDataChanged(const QByteArray &data);
    void onWorkerResult(const ChipOperationResult &result);
    void onWorkerProgress(int value, const QString &text);
    void onWorkerFinished();

private:
    void setupUi();
    void setupConnections();
    void setupWorker();
    void loadProfiles();
    void updateTitle();
    void updateProfileInfo();
    void updateActions();

    void setBusy(bool busy);
    void showError(const QString &title, const QString &message);
    void applyDump(const QByteArray &dump, const QString &chipId);
    void updateChipInfoFromDump(const QByteArray &dump);
    bool dumpMatchesCurrentProfile() const;
    void selectProfileForDump(const QByteArray &dump, const QString &preferredProfileId);

    QString currentProfileId() const;
    void setDumpAvailable(bool available);

    ConnectionPanel *m_connectionPanel = nullptr;
    CartridgePanel *m_cartridgePanel = nullptr;
    OperationsPanel *m_operationsPanel = nullptr;
    HexEditor *m_hexEditor = nullptr;
    LogPanel *m_logPanel = nullptr;
    StatusBar *m_statusBar = nullptr;

    QThread *m_workerThread = nullptr;
    ChipWorker *m_worker = nullptr;

    QByteArray m_currentDump;
    QString m_profileId;
    bool m_connected = false;
    bool m_busy = false;
    bool m_hasDump = false;
};

#endif // MAIN_WINDOW_H
