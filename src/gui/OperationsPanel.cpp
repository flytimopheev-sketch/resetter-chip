#include "OperationsPanel.h"

#include <QGridLayout>

OperationsPanel::OperationsPanel(QWidget *parent)
    : QGroupBox(QStringLiteral("Операции с чипом"), parent)
{
    auto *layout = new QGridLayout(this);

    auto addBtn = [&](int row, int col, const QString &text, QPushButton *&ptr) {
        ptr = new QPushButton(text, this);
        ptr->setMinimumHeight(40);
        layout->addWidget(ptr, row, col);
    };

    addBtn(0, 0, QStringLiteral("Читать чип"), m_btnRead);
    addBtn(0, 1, QStringLiteral("Записать в чип"), m_btnWrite);
    addBtn(1, 0, QStringLiteral("Проверить запись"), m_btnVerify);
    addBtn(1, 1, QStringLiteral("Обнулить счётчик"), m_btnResetCounter);
    addBtn(2, 0, QStringLiteral("Сохранить дамп"), m_btnSaveDump);
    addBtn(2, 1, QStringLiteral("Открыть дамп"), m_btnOpenDump);
    addBtn(3, 0, QStringLiteral("Создать резервную копию"), m_btnBackup);
    addBtn(3, 1, QStringLiteral("Восстановить из копии"), m_btnRestore);
    addBtn(4, 0, QStringLiteral("Сравнить дампы"), m_btnCompare);
    addBtn(4, 1, QStringLiteral("Отменить операцию"), m_btnCancel);

    layout->setColumnStretch(0, 1);
    layout->setColumnStretch(1, 1);

    connect(m_btnRead, &QPushButton::clicked, this, &OperationsPanel::readChipRequested);
    connect(m_btnWrite, &QPushButton::clicked, this, &OperationsPanel::writeChipRequested);
    connect(m_btnVerify, &QPushButton::clicked, this, &OperationsPanel::verifyWriteRequested);
    connect(m_btnSaveDump, &QPushButton::clicked, this, &OperationsPanel::saveDumpRequested);
    connect(m_btnOpenDump, &QPushButton::clicked, this, &OperationsPanel::openDumpRequested);
    connect(m_btnResetCounter, &QPushButton::clicked, this, &OperationsPanel::resetCounterRequested);
    connect(m_btnBackup, &QPushButton::clicked, this, &OperationsPanel::backupRequested);
    connect(m_btnRestore, &QPushButton::clicked, this, &OperationsPanel::restoreRequested);
    connect(m_btnCompare, &QPushButton::clicked, this, &OperationsPanel::compareDumpsRequested);
    connect(m_btnCancel, &QPushButton::clicked, this, &OperationsPanel::cancelRequested);

    updateButtonStates();
}

void OperationsPanel::setState(bool connected, bool hasDump, bool hasCounter,
                               bool dumpMatchesProfile)
{
    m_connected = connected;
    m_hasDump = hasDump;
    m_hasCounter = hasCounter;
    m_dumpMatchesProfile = dumpMatchesProfile;
    updateButtonStates();
}

void OperationsPanel::setBusy(bool busy)
{
    m_busy = busy;
    updateButtonStates();
}

void OperationsPanel::updateButtonStates()
{
    const bool idle = !m_busy;
    const bool dumpReady = m_hasDump && m_dumpMatchesProfile;

    m_btnRead->setEnabled(m_connected && idle);
    m_btnWrite->setEnabled(m_connected && dumpReady && idle);
    m_btnVerify->setEnabled(m_connected && dumpReady && idle);
    m_btnResetCounter->setEnabled(dumpReady && m_hasCounter && idle);

    m_btnSaveDump->setEnabled(m_hasDump && idle);
    m_btnOpenDump->setEnabled(idle);
    m_btnBackup->setEnabled(m_hasDump && idle);
    m_btnRestore->setEnabled(idle);
    m_btnCompare->setEnabled(m_hasDump && idle);

    m_btnCancel->setEnabled(m_busy);
}
