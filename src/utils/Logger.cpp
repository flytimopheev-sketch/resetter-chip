#include "Logger.h"

#include <QDateTime>

Logger &Logger::instance()
{
    static Logger inst;
    return inst;
}

void Logger::init(const QString &logFilePath)
{
    // ВАЖНО: сообщение пишем после отпускания мьютекса,
    // иначе возникает рекурсивный захват (дедлок).
    bool opened = false;
    {
        QMutexLocker lock(&m_mutex);
        m_logFile.setFileName(logFilePath);
        if (!m_initialized && m_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            m_stream.setDevice(&m_logFile);
            m_initialized = true;
            opened = true;
        }
    }

    if (opened) {
        info(QStringLiteral("Логгер инициализирован: ") + logFilePath);
    }
}

QString Logger::currentTimestamp() const
{
    return QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd hh:mm:ss.zzz"));
}

void Logger::info(const QString &message)
{
    write(QStringLiteral("INFO"), message);
}

void Logger::warning(const QString &message)
{
    write(QStringLiteral("WARNING"), message);
}

void Logger::error(const QString &message)
{
    write(QStringLiteral("ERROR"), message);
}

void Logger::debug(const QString &message)
{
    write(QStringLiteral("DEBUG"), message);
}

void Logger::write(const QString &level, const QString &message)
{
    QMutexLocker lock(&m_mutex);
    if (!m_initialized) {
        return;
    }
    m_stream << currentTimestamp() << " [" << level << "] " << message << '\n';
    m_stream.flush();
}
