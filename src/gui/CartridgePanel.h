#ifndef CARTRIDGE_PANEL_H
#define CARTRIDGE_PANEL_H

#include <QByteArray>
#include <QGroupBox>
#include <QLabel>
#include <QString>

// Панель информации о картридже и чипе
class CartridgePanel : public QGroupBox
{
    Q_OBJECT

public:
    explicit CartridgePanel(QWidget *parent = nullptr);

    void setInfoEnabled(bool enabled);

    // Обновить сведения по прочитанному дампу
    void updateFromDump(const QByteArray &dump, const QString &chipId,
                        const QString &chipName, quint32 chipSize,
                        bool counterKnown, quint32 counterValue);

    void setChipId(const QString &chipId);
    void setCartridgeModel(const QString &model);

    QString chipId() const;
    QString cartridgeModel() const;

private:
    QLabel *m_printerModelLabel = nullptr;
    QLabel *m_cartridgeModelLabel = nullptr;
    QLabel *m_chipTypeLabel = nullptr;
    QLabel *m_memorySizeLabel = nullptr;
    QLabel *m_pageCounterLabel = nullptr;
    QLabel *m_chipStatusLabel = nullptr;

    QString m_chipId;
    QString m_cartridgeModel;
};

#endif // CARTRIDGE_PANEL_H
