#ifndef OPERATIONS_PANEL_H
#define OPERATIONS_PANEL_H

#include <QGroupBox>
#include <QPushButton>

// Панель операций с чипом. Сигналы не передают данные:
// источником истины остаётся MainWindow (текущий дамп).
class OperationsPanel : public QGroupBox
{
    Q_OBJECT

public:
    explicit OperationsPanel(QWidget *parent = nullptr);

    void setBusy(bool busy);

signals:
    void readChipRequested();
    void writeChipRequested();
    void verifyWriteRequested();
    void saveDumpRequested();
    void openDumpRequested();
    void resetCounterRequested();
    void backupRequested();
    void restoreRequested();
    void compareDumpsRequested();
    void cancelRequested();

private:
    QPushButton *m_btnRead = nullptr;
    QPushButton *m_btnWrite = nullptr;
    QPushButton *m_btnVerify = nullptr;
    QPushButton *m_btnSaveDump = nullptr;
    QPushButton *m_btnOpenDump = nullptr;
    QPushButton *m_btnResetCounter = nullptr;
    QPushButton *m_btnBackup = nullptr;
    QPushButton *m_btnRestore = nullptr;
    QPushButton *m_btnCompare = nullptr;
    QPushButton *m_btnCancel = nullptr;

    void setPanelEnabled(bool enabled);
};

#endif // OPERATIONS_PANEL_H
