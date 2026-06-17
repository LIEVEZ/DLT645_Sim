#ifndef LOGGER_H
#define LOGGER_H

#include <QObject>
#include <QString>
#include <QFile>
#include <QTextStream>
#include <QMutex>
#include <QDateTime>
#include <memory>
#include "LoggerTypes.h"

/**
 * @brief 日志工具类
 *
 * 提供线程安全的日志记录功能
 */
class Logger : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 获取Logger单例实例
     * @return Logger实例指针
     */
    static Logger *instance();

    /**
     * @brief 记录调试日志
     * @param message 日志消息
     */
    void debug(const QString &message);

    /**
     * @brief 记录信息日志
     * @param message 日志消息
     */
    void info(const QString &message);

    /**
     * @brief 记录警告日志
     * @param message 日志消息
     */
    void warning(const QString &message);

    /**
     * @brief 记录错误日志
     * @param message 日志消息
     */
    void error(const QString &message);

    /**
     * @brief 记录严重错误日志
     * @param message 日志消息
     */
    void critical(const QString &message);

    /**
     * @brief 设置日志文件路径
     * @param filePath 日志文件路径
     */
    void setLogFile(const QString &filePath);

    /**
     * @brief 设置日志轮转参数
     * @param maxFileSize 单个日志文件最大大小（字节），0 表示不限制
     * @param maxBackupCount 保留的历史日志文件数量
     */
    void setLogRotation(qint64 maxFileSize, int maxBackupCount);

    /**
     * @brief 设置日志级别
     * @param level 最低日志级别
     */
    void setLogLevel(LogLevel level);

    /**
     * @brief 启用/禁用控制台输出
     * @param enabled 是否启用
     */
    void setConsoleOutputEnabled(bool enabled);

    /**
     * @brief 启用/禁用文件输出
     * @param enabled 是否启用
     */
    void setFileOutputEnabled(bool enabled);

signals:
    /**
     * @brief 日志记录信号
     * @param level 日志级别
     * @param timestamp 时间戳
     * @param message 日志消息
     */
    void logEntryAdded(LogLevel level, const QDateTime &timestamp, const QString &message);

private:
    explicit Logger(QObject *parent = nullptr);
    ~Logger();
    Logger(const Logger &) = delete;
    Logger &operator=(const Logger &) = delete;

    void writeLog(LogLevel level, const QString &message);
    QString levelToString(LogLevel level) const;
    QString formatLogMessage(LogLevel level, const QString &message) const;
    void rotateLogFileIfNeeded();
    void performLogRotation();

    static Logger *s_instance;
    std::unique_ptr<QFile> m_logFile;
    std::unique_ptr<QTextStream> m_logStream;
    QMutex m_mutex;
    LogLevel m_logLevel;
    bool m_consoleOutputEnabled;
    bool m_fileOutputEnabled;
    QString m_logFilePath;
    qint64 m_maxFileSize;
    int m_maxBackupCount;
    qint64 m_currentFileSize;
};

// 便捷宏定义
#define LOG_DEBUG(msg) Logger::instance()->debug(msg)
#define LOG_INFO(msg) Logger::instance()->info(msg)
#define LOG_WARNING(msg) Logger::instance()->warning(msg)
#define LOG_ERROR(msg) Logger::instance()->error(msg)
#define LOG_CRITICAL(msg) Logger::instance()->critical(msg)

#endif // LOGGER_H
