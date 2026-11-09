#include "CartridgePanel.h"

#include <QGridLayout>

CartridgePanel::CartridgePanel(QWidget *parent)
    : QGroupBox(QStringLiteral("Информация о картридже и чипе"), parent)
{
    auto *layout = new QGridLayout(this);

    auto addRow = [this, layout](int row, const QString &caption, QLabel *&value) {
        auto *label = new QLabel(caption, this);
        label->setMinimumWidth(160);
        value = new QLabel(QStringLiteral("—"), this);
        layout->addWidget(label, row, 0);
        layout->addWidget(value, row, 1);
    };

    addRow(0, QStringLiteral("Модель принтера:"), m_printerModelLabel);
    addRow(1, QStringLiteral("Модель картриджа:"), m_cartridgeModelLabel);
    addRow(2, QStringLiteral("Тип чипа:"), m_chipTypeLabel);
    addRow(3, QStringLiteral("Объём памяти:"), m_memorySizeLabel);
    addRow(4, QStringLiteral("Счётчик страниц:"), m_pageCounterLabel);
    addRow(5, QStringLiteral("Статус чипа:"), m_chipStatusLabel);

    layout->setColumnStretch(1, 1);

    m_printerModelLabel->setText(QStringLiteral("—"));
    setInfoEnabled(false);
}

void CartridgePanel::setInfoEnabled(bool enabled)
{
    // Панель всегда видима, меняется только доступность
    for (QLabel *label : {m_printerModelLabel, m_cartridgeModelLabel, m_chipTypeLabel,
                          m_memorySizeLabel, m_pageCounterLabel, m_chipStatusLabel}) {
        label->setEnabled(enabled);
    }

    if (!enabled) {
        m_printerModelLabel->setText(QStringLiteral("—"));
        m_cartridgeModelLabel->setText(m_cartridgeModel.isEmpty() ? QStringLiteral("—") : m_cartridgeModel);
        m_chipTypeLabel->setText(m_chipId.isEmpty() ? QStringLiteral("—") : m_chipId);
        m_memorySizeLabel->setText(QStringLiteral("—"));
        m_pageCounterLabel->setText(QStringLiteral("—"));
        m_chipStatusLabel->setText(QStringLiteral("Неизвестно"));
        m_chipStatusLabel->setStyleSheet(QString());
    }
}

void CartridgePanel::updateFromDump(const QByteArray &dump, const QString &chipId,
                                    const QString &chipName, quint32 chipSize,
                                    bool counterKnown, quint32 counterValue)
{
    m_chipId = chipId;

    m_chipTypeLabel->setText(chipName.isEmpty() ? chipId : QStringLiteral("%1 (%2)").arg(chipId, chipName));
    m_memorySizeLabel->setText(QStringLiteral("%1 байт (прочитано %2)")
                                   .arg(chipSize)
                                   .arg(dump.size()));

    if (counterKnown) {
        m_pageCounterLabel->setText(QString::number(counterValue));
    } else {
        m_pageCounterLabel->setText(QStringLiteral("не задан в профиле"));
    }

    m_cartridgeModelLabel->setText(m_cartridgeModel.isEmpty() ? QStringLiteral("—") : m_cartridgeModel);
    m_chipStatusLabel->setText(QStringLiteral("Прочитано"));
    m_chipStatusLabel->setStyleSheet(QStringLiteral("color: #60cb84;"));
}

void CartridgePanel::setChipId(const QString &chipId)
{
    m_chipId = chipId;
}

void CartridgePanel::setCartridgeModel(const QString &model)
{
    m_cartridgeModel = model;
    m_cartridgeModelLabel->setText(model.isEmpty() ? QStringLiteral("—") : model);
}

QString CartridgePanel::chipId() const
{
    return m_chipId;
}

QString CartridgePanel::cartridgeModel() const
{
    return m_cartridgeModel;
}
