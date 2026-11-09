#ifndef STATUS_BAR_H
#define STATUS_BAR_H

#include <QLabel>
#include <QProgressBar>
#include <QStatusBar>

// Статусная строка приложения
class StatusBar : public QStatusBar
{
    Q_OBJECT

public:
    explicit StatusBar(QWidget *parent = nullptr);

    void setAdapterState(bool connected);
    void setDumpLoaded(bool loaded, int sizeBytes = 0);
    void setModified(bool modified);
    void setProgress(int value, const QString &text);
    void showError(const QString &message);

private:
    QLabel *m_adapterLabel = nullptr;
    QLabel *m_dumpLabel = nullptr;
    QLabel *m_progressLabel = nullptr;
    QProgressBar *m_progressBar = nullptr;
};

#endif // STATUS_BAR_H
