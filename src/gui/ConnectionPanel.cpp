#include "ConnectionPanel.h"

#include <QHBoxLayout>
#include <QVBoxLayout>

ConnectionPanel::ConnectionPanel(QWidget *parent)
    : QGroupBox(QStringLiteral("Панель подключения программатора"), parent)
{
    auto *layout = new QVBoxLayout(this);

    auto *topRow = new QHBoxLayout();

    m_adapterCombo = new QComboBox(this);
    m_adapterCombo->addItem(QStringLiteral("CH341A (I2C)"), QStringLiteral("CH341A"));
    topRow->addWidget(new QLabel(QStringLiteral("Программатор:"), this));
    topRow->addWidget(m_adapterCombo);

    m_addressCombo = new QComboBox(this);
    for (int addr = 0x50; addr <= 0x57; ++addr) {
        m_addressCombo->addItem(QStringLiteral("0x%1").arg(addr, 2, 16, QLatin1Char('0')).toUpper(), addr);
    }
    topRow->addWidget(new QLabel(QStringLiteral("I2C-адрес:"), this));
    topRow->addWidget(m_addressCombo);

    m_profileCombo = new QComboBox(this);
    m_profileCombo->setMinimumWidth(160);
    topRow->addWidget(new QLabel(QStringLiteral("Профиль чипа:"), this));
    topRow->addWidget(m_profileCombo);

    topRow->addStretch();
    layout->addLayout(topRow);

    auto *btnRow = new QHBoxLayout();
    m_connectBtn = new QPushButton(QStringLiteral("Подключить программатор"), this);
    m_connectBtn->setMinimumHeight(40);
    btnRow->addWidget(m_connectBtn);

    m_scanBtn = new QPushButton(QStringLiteral("Сканировать шину I2C"), this);
    m_scanBtn->setMinimumHeight(40);
    m_scanBtn->setEnabled(false);
    btnRow->addWidget(m_scanBtn);

    m_statusLabel = new QLabel(QStringLiteral("Статус: не подключено"), this);
    m_statusLabel->setStyleSheet(QStringLiteral("color: #f48771; font-weight: bold;"));
    btnRow->addWidget(m_statusLabel);
    btnRow->addStretch();

    layout->addLayout(btnRow);

    connect(m_connectBtn, &QPushButton::clicked, this, [this]() {
        if (m_connected) {
            emit disconnectRequested();
        } else {
            emit connectRequested();
        }
    });

    connect(m_scanBtn, &QPushButton::clicked, this, &ConnectionPanel::scanRequested);

    connect(m_profileCombo, &QComboBox::currentTextChanged, this, [this](const QString &id) {
        emit profileChanged(id);
    });
}

void ConnectionPanel::setConnected(bool connected)
{
    m_connected = connected;
    m_connectBtn->setText(connected ? QStringLiteral("Отключить программатор")
                                    : QStringLiteral("Подключить программатор"));
    m_statusLabel->setText(connected ? QStringLiteral("Статус: подключено")
                                     : QStringLiteral("Статус: не подключено"));
    m_statusLabel->setStyleSheet(connected
                                     ? QStringLiteral("color: #60cb84; font-weight: bold;")
                                     : QStringLiteral("color: #f48771; font-weight: bold;"));
    m_adapterCombo->setEnabled(!connected);
    m_addressCombo->setEnabled(!connected);
    m_scanBtn->setEnabled(connected);
}

void ConnectionPanel::showStatusMessage(const QString &text, bool ok)
{
    m_statusLabel->setText(text);
    m_statusLabel->setStyleSheet(ok ? QStringLiteral("color: #60cb84; font-weight: bold;")
                                    : QStringLiteral("color: #f48771; font-weight: bold;"));
}

bool ConnectionPanel::isConnected() const
{
    return m_connected;
}

QString ConnectionPanel::selectedAdapter() const
{
    return m_adapterCombo->currentData().toString();
}

quint8 ConnectionPanel::chipAddress() const
{
    return static_cast<quint8>(m_addressCombo->currentData().toUInt());
}

void ConnectionPanel::setProfileIds(const QStringList &ids)
{
    const QString current = m_profileCombo->currentText();
    m_profileCombo->blockSignals(true);
    m_profileCombo->clear();
    m_profileCombo->addItems(ids);

    const int index = m_profileCombo->findText(current);
    if (index >= 0) {
        m_profileCombo->setCurrentIndex(index);
    }
    m_profileCombo->blockSignals(false);
}

QString ConnectionPanel::selectedProfile() const
{
    return m_profileCombo->currentText();
}
