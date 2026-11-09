#include "StatusBar.h"

StatusBar::StatusBar(QWidget *parent)
    : QStatusBar(parent)
{
    m_adapterLabel = new QLabel(QStringLiteral("Программатор: отключён"), this);
    m_adapterLabel->setStyleSheet(QStringLiteral("color: #f48771;"));
    addWidget(m_adapterLabel);

    m_dumpLabel = new QLabel(QStringLiteral("Дамп: не загружен"), this);
    addWidget(m_dumpLabel);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setMaximumWidth(220);
    m_progressBar->setRange(0, 100);
    m_progressBar->setVisible(false);
    addWidget(m_progressBar);

    m_progressLabel = new QLabel(QString(), this);
    addWidget(m_progressLabel);
}

void StatusBar::setAdapterState(bool connected)
{
    m_adapterLabel->setText(connected ? QStringLiteral("Программатор: подключён")
                                      : QStringLiteral("Программатор: отключён"));
    m_adapterLabel->setStyleSheet(connected ? QStringLiteral("color: #60cb84;")
                                            : QStringLiteral("color: #f48771;"));
}

void StatusBar::setDumpLoaded(bool loaded, int sizeBytes)
{
    if (loaded) {
        m_dumpLabel->setText(QStringLiteral("Дамп: загружен (%1 байт)").arg(sizeBytes));
    } else {
        m_dumpLabel->setText(QStringLiteral("Дамп: не загружен"));
    }
}

void StatusBar::setModified(bool modified)
{
    m_dumpLabel->setStyleSheet(modified ? QStringLiteral("color: #dcdcaa;")
                                        : QStringLiteral("color: #d4d4d4;"));
}

void StatusBar::setProgress(int value, const QString &text)
{
    const bool finished = (value <= 0 && text.isEmpty()) || value >= 100;

    if (value <= 0 && text.isEmpty()) {
        m_progressBar->setVisible(false);
        m_progressLabel->clear();
        return;
    }

    m_progressBar->setVisible(true);
    m_progressBar->setValue(qBound(0, value, 100));
    m_progressLabel->setText(text);

    if (finished) {
        m_progressBar->setVisible(true);
    }
}

void StatusBar::showError(const QString &message)
{
    showMessage(message, 8000);
}
