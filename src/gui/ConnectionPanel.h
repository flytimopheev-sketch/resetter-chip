#ifndef CONNECTION_PANEL_H
#define CONNECTION_PANEL_H

#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QStringList>

// Панель подключения программатора и выбора профиля чипа
class ConnectionPanel : public QGroupBox
{
    Q_OBJECT

public:
    explicit ConnectionPanel(QWidget *parent = nullptr);

    void setConnected(bool connected);
    bool isConnected() const;

    QString selectedAdapter() const;
    quint8 chipAddress() const;

    void setProfileIds(const QStringList &ids);
    QString selectedProfile() const;

    void showStatusMessage(const QString &text, bool ok);

signals:
    void connectRequested();
    void disconnectRequested();
    void scanRequested();
    void profileChanged(const QString &profileId);

private:
    QPushButton *m_connectBtn = nullptr;
    QPushButton *m_scanBtn = nullptr;
    QLabel *m_statusLabel = nullptr;
    QComboBox *m_adapterCombo = nullptr;
    QComboBox *m_addressCombo = nullptr;
    QComboBox *m_profileCombo = nullptr;
    bool m_connected = false;
};

#endif // CONNECTION_PANEL_H
