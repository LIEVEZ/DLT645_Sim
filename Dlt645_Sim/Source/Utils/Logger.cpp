#include "Logger.h"
#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>
#include <iostream>

Logger *Logger::s_instance = nullptr;

Logger::Logger(QObject *parent)
    : QObject(parent), m_logFile(nullptr), m_logStream(nullptr), m_logLevel(LogLevel::Debug), m_consoleOutputEnabled(true), m_fileOutputEnabled(false)
{
}

Logger::~Logger()
{
    if (m_logStream)
    {
        m_logStream->flush();
        delete m_logStream;
    }
    if (m_logFile)
    {
        m_logFile->close();
        delete m_logFile;
    }
}

Logger *Logger::instance()
{
    if (!s_instance)
    {
        s_instance = new Logger();
    }
    return s_instance;
}

void Logger::debug(const QString &message)
{
    writeLog(LogLevel::Debug, message);
}

void Logger::info(const QString &message)
{
    writeLog(LogLevel::Info, message);
}

void Logger::warning(const QString &message)
{
    writeLog(LogLevel::Warning, message);
}

void Logger::error(const QString &message)
{
    writeLog(LogLevel::Error, message);
}

void Logger::critical(const QString &message)
{
    writeLog(LogLevel::Critical, message);
}

void Logger::setLogFile(const QString &filePath)
{
    QMutexLocker locker(&m_mutex);

    if (m_logStream)
    {
        m_logStream->flush();
        delete m_logStream;
        m_logStream = nullptr;
    }

    if (m_logFile)
    {
        m_logFile->close();
        delete m_logFile;
        m_logFile = nullptr;
    }

    if (!filePath.isEmpty())
    {
        // 确保目录存在
        QFileInfo fileInfo(filePath);
        QDir dir = fileInfo.dir();
        if (!dir.exists())
        {
            dir.mkpath(".");
        }

        m_logFile = new QFile(filePath);
        if (m_logFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        {
            m_logStream = new QTextStream(m_logFile);
            m_logStream->setCodec("UTF-8");
            m_fileOutputEnabled = true;
        }
        else
        {
            delete m_logFile;
            m_logFile = nullptr;
            m_fileOutputEnabled = false;
        }
    }
}

void Logger::setLogLevel(LogLevel level)
{
    QMutexLocker locker(&m_mutex);
    m_logLevel = level;
}

void Logger::setConsoleOutputEnabled(bool enabled)
{
    QMutexLocker locker(&m_mutex);
    m_consoleOutputEnabled = enabled;
}

void Logger::setFileOutputEnabled(bool enabled)
{
    QMutexLocker locker(&m_mutex);
    m_fileOutputEnabled = enabled;
}

void Logger::writeLog(LogLevel level, const QString &message)
{
    QMutexLocker locker(&m_mutex);

    // 检查日志级别
    if (level < m_logLevel)
    {
        return;
    }

    QDateTime now = QDateTime::currentDateTime();
    QString formattedMessage = formatLogMessage(level, message);

    // 控制台输出
    if (m_consoleOutputEnabled)
    {
        std::cout << formattedMessage.toStdString() << std::endl;
    }

    // 文件输出
    if (m_fileOutputEnabled && m_logStream)
    {
        *m_logStream << formattedMessage << "\n";
        m_logStream->flush();
    }

    // 发射信号
    emit logEntryAdded(level, now, message);
}

QString Logger::levelToString(LogLevel level) const
{
    switch (level)
    {
    case LogLevel::Debug:
        return "DEBUG";
    case LogLevel::Info:
        return "INFO";
    case LogLevel::Warning:
        return "WARNING";
    case LogLevel::Error:
        return "ERROR";
    case LogLevel::Critical:
        return "CRITICAL";
    default:
        return "UNKNOWN";
    }
}

QString Logger::formatLogMessage(LogLevel level, const QString &message) const
{
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    return QString("[%1] [%2] %3").arg(timestamp, levelToString(level), message);
}
