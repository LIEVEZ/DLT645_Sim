#include "Logger.h"
#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>
#include <iostream>

Logger *Logger::s_instance = nullptr;

Logger::Logger(QObject *parent)
    : QObject(parent),
      m_logFile(nullptr),
      m_logStream(nullptr),
      m_logLevel(LogLevel::Debug),
      m_consoleOutputEnabled(true),
      m_fileOutputEnabled(false),
      m_logFilePath(),
      m_maxFileSize(10 * 1024 * 1024),  // 默认 10MB
      m_maxBackupCount(5),               // 默认保留 5 个备份
      m_currentFileSize(0)
{
}

Logger::~Logger()
{
    if (m_logStream)
    {
        m_logStream->flush();
        // unique_ptr 自动析构，无需手动 delete
    }
    if (m_logFile)
    {
        m_logFile->close();
        // unique_ptr 自动析构，无需手动 delete
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
        m_logStream.reset();
    }

    if (m_logFile)
    {
        m_logFile->close();
        m_logFile.reset();
    }

    m_logFilePath = filePath;

    if (!filePath.isEmpty())
    {
        // 确保目录存在
        QFileInfo fileInfo(filePath);
        QDir dir = fileInfo.dir();
        if (!dir.exists())
        {
            dir.mkpath(".");
        }

        m_logFile = std::make_unique<QFile>(filePath);
        if (m_logFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        {
            m_currentFileSize = m_logFile->size();
            m_logStream = std::make_unique<QTextStream>(m_logFile.get());
            m_logStream->setCodec("UTF-8");
            m_fileOutputEnabled = true;
        }
        else
        {
            m_logFile.reset();
            m_fileOutputEnabled = false;
        }
    }
}

void Logger::setLogRotation(qint64 maxFileSize, int maxBackupCount)
{
    QMutexLocker locker(&m_mutex);
    m_maxFileSize = maxFileSize;
    m_maxBackupCount = maxBackupCount;
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
        // 检查是否需要轮转
        rotateLogFileIfNeeded();

        *m_logStream << formattedMessage << "\n";
        m_logStream->flush();

        // 更新当前文件大小
        m_currentFileSize += formattedMessage.toUtf8().size() + 1; // +1 for newline
    }

    // 发射信号
    emit logEntryAdded(level, now, message);
}

void Logger::rotateLogFileIfNeeded()
{
    // 如果未设置大小限制，或当前文件大小未超限，则不轮转
    if (m_maxFileSize <= 0 || m_currentFileSize < m_maxFileSize)
    {
        return;
    }

    performLogRotation();
}

void Logger::performLogRotation()
{
    if (m_logFilePath.isEmpty())
    {
        return;
    }

    // 关闭当前日志文件
    if (m_logStream)
    {
        m_logStream->flush();
        m_logStream.reset();
    }

    if (m_logFile)
    {
        m_logFile->close();
        m_logFile.reset();
    }

    // 删除最旧的备份文件（如果存在）
    if (m_maxBackupCount > 0)
    {
        QString oldestBackup = QString("%1.%2").arg(m_logFilePath).arg(m_maxBackupCount);
        if (QFile::exists(oldestBackup))
        {
            QFile::remove(oldestBackup);
        }

        // 重命名现有备份文件：app.log.1 -> app.log.2, app.log.2 -> app.log.3, ...
        for (int i = m_maxBackupCount - 1; i > 0; --i)
        {
            QString srcName = QString("%1.%2").arg(m_logFilePath).arg(i);
            QString dstName = QString("%1.%2").arg(m_logFilePath).arg(i + 1);
            if (QFile::exists(srcName))
            {
                QFile::remove(dstName); // 确保目标不存在
                QFile::rename(srcName, dstName);
            }
        }

        // 将当前日志文件重命名为 .1
        QString firstBackup = QString("%1.1").arg(m_logFilePath);
        QFile::remove(firstBackup); // 确保目标不存在
        QFile::rename(m_logFilePath, firstBackup);
    }

    // 重新打开日志文件
    m_logFile = std::make_unique<QFile>(m_logFilePath);
    if (m_logFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
    {
        m_currentFileSize = 0;
        m_logStream = std::make_unique<QTextStream>(m_logFile.get());
        m_logStream->setCodec("UTF-8");

        // 写入轮转标记
        QString rotationMark = QString("[%1] [INFO] === Log file rotated ===\n")
                                   .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz"));
        *m_logStream << rotationMark;
        m_logStream->flush();
        m_currentFileSize = rotationMark.toUtf8().size();
    }
    else
    {
        m_logFile.reset();
        m_fileOutputEnabled = false;
    }
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
