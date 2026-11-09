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

    m_btnCancel->setEnabled(false);
    setPanelEnabled(false);
}

void OperationsPanel::setPanelEnabled(bool enabled)
{
    // Намеренно не вызываем QGroupBox::setEnabled напрямую —
    // управляем кнопками, чтобы панель оставалась читаемой
    for (QPushButton *btn : {m_btnRead, m_btnWrite, m_btnVerify, m_btnSaveDump,
                             m_btnOpenDump, m_btnResetCounter, m_btnBackup,
                             m_btnRestore, m_btnCompare}) {
        btn->setEnabled(enabled);
    }
}

void OperationsPanel::setBusy(bool busy)
{
    setPanelEnabled(!busy);
    m_btnCancel->setEnabled(busy);
}
