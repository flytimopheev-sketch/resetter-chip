#include "ConnectionPanel.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QRegularExpression>
#include <QVBoxLayout>

namespace {
const QString kAdapterAuto = QStringLiteral("ch341a");
const QString kAdapterCustom = QStringLiteral("ch341a-custom");
const QString kAdapterSerial = QStringLiteral("serial");
} // namespace

ConnectionPanel::ConnectionPanel(QWidget *parent)
    : QGroupBox(QStringLiteral("Панель подключения программатора"), parent)
{
    auto *layout = new QVBoxLayout(this);

    auto *topRow = new QGridLayout();
    topRow->setColumnStretch(1, 1);
    topRow->setColumnStretch(3, 1);
    topRow->setColumnStretch(5, 2);

    m_adapterCombo = new QComboBox(this);
    m_adapterCombo->addItem(QStringLiteral("CH341A / CH341 (автоопределение)"), kAdapterAuto);
    m_adapterCombo->addItem(QStringLiteral("CH341A-совместимый (свой VID:PID)"), kAdapterCustom);
    m_adapterCombo->addItem(QStringLiteral("Самодельный Arduino/ESP (последовательный порт)"),
                            kAdapterSerial);
    m_adapterCombo->setMinimumWidth(270);
    topRow->addWidget(new QLabel(QStringLiteral("Программатор:"), this), 0, 0);
    topRow->addWidget(m_adapterCombo, 0, 1);

    m_addressCombo = new QComboBox(this);
    for (int addr = 0x50; addr <= 0x57; ++addr) {
        m_addressCombo->addItem(
            QStringLiteral("0x%1").arg(addr, 2, 16, QLatin1Char('0')).toUpper(), addr);
    }
    topRow->addWidget(new QLabel(QStringLiteral("I2C-адрес:"), this), 0, 2);
    topRow->addWidget(m_addressCombo, 0, 3);

    m_profileCombo = new QComboBox(this);
    m_profileCombo->setMinimumWidth(260);
    topRow->addWidget(new QLabel(QStringLiteral("Профиль:"), this), 0, 4);
    topRow->addWidget(m_profileCombo, 0, 5);

    layout->addLayout(topRow);

    auto *detailsRow = new QHBoxLayout();

    m_customWidget = new QWidget(this);
    auto *customLayout = new QHBoxLayout(m_customWidget);
    customLayout->setContentsMargins(0, 0, 0, 0);
    customLayout->addWidget(new QLabel(QStringLiteral("VID:"), m_customWidget));
    m_vidEdit = new QLineEdit(QStringLiteral("1A86"), m_customWidget);
    m_vidEdit->setMaxLength(4);
    m_vidEdit->setFixedWidth(70);
    m_vidEdit->setPlaceholderText(QStringLiteral("1A86"));
    customLayout->addWidget(m_vidEdit);
    customLayout->addWidget(new QLabel(QStringLiteral("PID:"), m_customWidget));
    m_pidEdit = new QLineEdit(QStringLiteral("5512"), m_customWidget);
    m_pidEdit->setMaxLength(4);
    m_pidEdit->setFixedWidth(70);
    m_pidEdit->setPlaceholderText(QStringLiteral("5512"));
    customLayout->addWidget(m_pidEdit);
    customLayout->addWidget(
        new QLabel(QStringLiteral("(hex, например 1A86:5512)"), m_customWidget));
    detailsRow->addWidget(m_customWidget);

    m_serialWidget = new QWidget(this);
    auto *serialLayout = new QHBoxLayout(m_serialWidget);
    serialLayout->setContentsMargins(0, 0, 0, 0);
    serialLayout->addWidget(new QLabel(QStringLiteral("Порт:"), m_serialWidget));
    m_portEdit = new QLineEdit(m_serialWidget);
    m_portEdit->setMinimumWidth(260);
    m_portEdit->setPlaceholderText(QStringLiteral("/dev/ttyUSB0 (пусто — автопоиск)"));
    serialLayout->addWidget(m_portEdit);
    serialLayout->addWidget(new QLabel(
        QStringLiteral("Прошивка: firmware/resetter-serial-i2c"), m_serialWidget));
    detailsRow->addWidget(m_serialWidget);

    detailsRow->addStretch();
    layout->addLayout(detailsRow);

    auto *btnRow = new QHBoxLayout();
    m_connectBtn = new QPushButton(QStringLiteral("Подключить программатор"), this);
    m_connectBtn->setMinimumHeight(40);
    btnRow->addWidget(m_connectBtn);

    m_scanBtn = new QPushButton(QStringLiteral("Сканировать шину I2C"), this);
    m_scanBtn->setMinimumHeight(40);
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

    connect(m_adapterCombo, &QComboBox::currentIndexChanged, this,
            [this](int) { updateAdapterFields(); });

    connect(m_profileCombo, &QComboBox::currentIndexChanged, this, [this](int) {
        emit profileChanged(m_profileCombo->currentData().toString());
    });

    updateAdapterFields();
    updateEnabledState();
}

void ConnectionPanel::updateAdapterFields()
{
    const QString type = m_adapterCombo->currentData().toString();
    m_customWidget->setVisible(type == kAdapterCustom);
    m_serialWidget->setVisible(type == kAdapterSerial);
}

void ConnectionPanel::updateEnabledState()
{
    const bool editable = !m_connected && !m_busy;

    m_adapterCombo->setEnabled(editable);
    m_addressCombo->setEnabled(editable);
    m_profileCombo->setEnabled(editable);
    m_vidEdit->setEnabled(editable);
    m_pidEdit->setEnabled(editable);
    m_portEdit->setEnabled(editable);

    m_connectBtn->setEnabled(!m_busy);
    m_scanBtn->setEnabled(m_connected && !m_busy);
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
    updateEnabledState();
}

void ConnectionPanel::setBusy(bool busy)
{
    m_busy = busy;
    updateEnabledState();
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

QString ConnectionPanel::adapterSpec(QString *errorMessage) const
{
    const QString type = m_adapterCombo->currentData().toString();

    if (type == kAdapterCustom) {
        const QString vidText = m_vidEdit->text().trimmed();
        const QString pidText = m_pidEdit->text().trimmed();
        const QRegularExpression hexPattern(QStringLiteral("^[0-9A-Fa-f]{1,4}$"));

        bool vidOk = false;
        bool pidOk = false;
        const uint vid = vidText.toUInt(&vidOk, 16);
        const uint pid = pidText.toUInt(&pidOk, 16);
        if (!hexPattern.match(vidText).hasMatch() || !hexPattern.match(pidText).hasMatch()
            || !vidOk || !pidOk || vid == 0 || pid == 0) {
            if (errorMessage) {
                *errorMessage = QStringLiteral(
                    "Укажите VID и PID в hex (1–4 цифры), например 1A86 и 5512");
            }
            return QString();
        }
        return QStringLiteral("ch341a:%1:%2")
            .arg(vid, 4, 16, QLatin1Char('0'))
            .arg(pid, 4, 16, QLatin1Char('0'))
            .toUpper();
    }

    if (type == kAdapterSerial) {
        // Пустой порт означает автопоиск /dev/ttyUSB0, /dev/ttyACM0, /dev/serial0
        return QStringLiteral("serial:") + m_portEdit->text().trimmed();
    }

    return kAdapterAuto;
}

quint8 ConnectionPanel::chipAddress() const
{
    return static_cast<quint8>(m_addressCombo->currentData().toUInt());
}

void ConnectionPanel::setChipAddress(quint8 address)
{
    const int index = m_addressCombo->findData(static_cast<int>(address));
    if (index >= 0) {
        m_addressCombo->setCurrentIndex(index);
    }
}

void ConnectionPanel::setProfiles(const QStringList &chipIds, const QStringList &cartridgeIds)
{
    const QString current = selectedProfile();

    m_profileCombo->blockSignals(true);
    m_profileCombo->clear();

    for (const QString &id : chipIds) {
        m_profileCombo->addItem(id, id);
    }

    if (!cartridgeIds.isEmpty()) {
        if (!chipIds.isEmpty()) {
            m_profileCombo->insertSeparator(m_profileCombo->count());
        }
        for (const QString &id : cartridgeIds) {
            m_profileCombo->addItem(QStringLiteral("Картридж: %1").arg(id), id);
        }
    }

    const int index = m_profileCombo->findData(current);
    if (index >= 0) {
        m_profileCombo->setCurrentIndex(index);
    }
    m_profileCombo->blockSignals(false);
}

QString ConnectionPanel::selectedProfile() const
{
    return m_profileCombo->currentData().toString();
}

bool ConnectionPanel::selectProfile(const QString &profileId)
{
    const int index = m_profileCombo->findData(profileId);
    if (index < 0) {
        return false;
    }
    m_profileCombo->setCurrentIndex(index);
    return true;
}
