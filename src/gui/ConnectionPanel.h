#ifndef CONNECTION_PANEL_H
#define CONNECTION_PANEL_H

#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStringList>
#include <QWidget>

// Панель подключения программатора и выбора профиля чипа/картриджа.
// Поддерживает CH341A с автоопределением, CH341A-совместимые платы
// с произвольным VID/PID и самодельный последовательный программатор.
class ConnectionPanel : public QGroupBox
{
    Q_OBJECT

public:
    explicit ConnectionPanel(QWidget *parent = nullptr);

    void setConnected(bool connected);
    bool isConnected() const;

    // Строка спецификации для AdapterFactory: ch341a, ch341a:VID:PID, serial:PORT
    QString adapterSpec(QString *errorMessage = nullptr) const;
    quint8 chipAddress() const;
    void setChipAddress(quint8 address);

    // Профили: сначала чипы, затем картриджи (в выпадающем списке с префиксом)
    void setProfiles(const QStringList &chipIds, const QStringList &cartridgeIds);
    QString selectedProfile() const;
    bool selectProfile(const QString &profileId);

    void showStatusMessage(const QString &text, bool ok);
    void setBusy(bool busy);

signals:
    void connectRequested();
    void disconnectRequested();
    void scanRequested();
    void profileChanged(const QString &profileId);

private:
    void updateAdapterFields();
    void updateEnabledState();

    QPushButton *m_connectBtn = nullptr;
    QPushButton *m_scanBtn = nullptr;
    QLabel *m_statusLabel = nullptr;

    QComboBox *m_adapterCombo = nullptr;
    QComboBox *m_addressCombo = nullptr;
    QComboBox *m_profileCombo = nullptr;

    QWidget *m_customWidget = nullptr;
    QLineEdit *m_vidEdit = nullptr;
    QLineEdit *m_pidEdit = nullptr;

    QWidget *m_serialWidget = nullptr;
    QLineEdit *m_portEdit = nullptr;

    bool m_connected = false;
    bool m_busy = false;
};

#endif // CONNECTION_PANEL_H
