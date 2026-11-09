#ifndef LOGGER_H
#define LOGGER_H

#include <QFile>
#include <QMutex>
#include <QString>
#include <QTextStream>

// Потокобезопасный логгер приложения (синглтон)
class Logger
{
public:
    static Logger &instance();

    void init(const QString &logFilePath);

    void info(const QString &message);
    void warning(const QString &message);
    void error(const QString &message);
    void debug(const QString &message);

private:
    Logger() = default;
    ~Logger() = default;
    Logger(const Logger &) = delete;
    Logger &operator=(const Logger &) = delete;

    // Запись строки в файл. Мьютекс захватывается внутри.
    void write(const QString &level, const QString &message);

    QString currentTimestamp() const;

    QFile m_logFile;
    QTextStream m_stream;
    QMutex m_mutex;
    bool m_initialized = false;
};

#endif // LOGGER_H
