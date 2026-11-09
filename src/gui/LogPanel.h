#ifndef LOG_PANEL_H
#define LOG_PANEL_H

#include <QWidget>
#include <QPlainTextEdit>
#include <QDateTime>
#include <QString>

// Панель журнала операций
class LogPanel : public QWidget
{
    Q_OBJECT

public:
    explicit LogPanel(QWidget *parent = nullptr);

    void addMessage(const QString &message, const QString &type = "info");
    void clear();

private:
    QPlainTextEdit *m_logEdit = nullptr;
};

#endif // LOG_PANEL_H