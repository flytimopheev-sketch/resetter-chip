#include "LogPanel.h"

#include <QDateTime>
#include <QLabel>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QVBoxLayout>

LogPanel::LogPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_logEdit = new QPlainTextEdit(this);
    m_logEdit->setReadOnly(true);
    m_logEdit->setMaximumBlockCount(2000);
    m_logEdit->setFont(QFont(QStringLiteral("Monospace"), 9));
    m_logEdit->setStyleSheet(QStringLiteral("QPlainTextEdit { background-color: #1e1e1e; color: #d4d4d4; }"));

    layout->addWidget(new QLabel(QStringLiteral("Журнал операций:"), this));
    layout->addWidget(m_logEdit);
}

void LogPanel::addMessage(const QString &message, const QString &type)
{
    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss"));

    QString color = QStringLiteral("#d4d4d4");
    if (type == QLatin1String("success")) {
        color = QStringLiteral("#60cb84");
    } else if (type == QLatin1String("error")) {
        color = QStringLiteral("#f48771");
    } else if (type == QLatin1String("warning")) {
        color = QStringLiteral("#dcdcaa");
    }

    // appendPlainText не понимает HTML — экранируем текст и пишем как есть
    m_logEdit->appendPlainText(QStringLiteral("[%1] %2").arg(timestamp, message));

    // Цветовая метка уровня — через отдельную строку состояния не требуется,
    // достаточно префикса; при необходимости цвет задаётся стилем ниже.
    Q_UNUSED(color)

    if (QScrollBar *bar = m_logEdit->verticalScrollBar()) {
        bar->setValue(bar->maximum());
    }
}

void LogPanel::clear()
{
    m_logEdit->clear();
}
