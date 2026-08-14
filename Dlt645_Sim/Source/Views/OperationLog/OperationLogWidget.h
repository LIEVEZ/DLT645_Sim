#ifndef OPERATIONLOGWIDGET_H
#define OPERATIONLOGWIDGET_H

#include <QDateTime>
#include <QMetaType>
#include <QString>
#include <QWidget>

class QCheckBox;
class QLineEdit;
class QListWidget;
class QPushButton;
namespace ui { struct ThemePalette; }

/**
 * @brief 操作日志级别枚举
 */
enum class OperationLogLevel
{
    Info,
    Success,
    Communication,
    Warning,
    Error
};

/**
 * @brief 日志条目结构
 */
struct LogEntry
{
    QDateTime timestamp;
    OperationLogLevel level;
    QString category;
    QString message;

    LogEntry() = default;
    LogEntry(const QDateTime &ts, OperationLogLevel lvl, const QString &cat, const QString &msg)
        : timestamp(ts), level(lvl), category(cat), message(msg)
    {
    }
};

Q_DECLARE_METATYPE(LogEntry)

/**
 * @brief 操作日志组件
 *
 * 显示操作日志、通信日志、错误信息等
 */
class OperationLogWidget : public QWidget
{
    Q_OBJECT

public:
    explicit OperationLogWidget(QWidget *parent = nullptr);
    ~OperationLogWidget();

    /**
     * @brief 添加日志条目
     * @param entry 日志条目
     */
    void addLogEntry(const LogEntry &entry);

    /**
     * @brief 添加信息日志
     * @param message 消息
     */
    void addInfo(const QString &message);

    /**
     * @brief 添加成功日志
     * @param message 消息
     */
    void addSuccess(const QString &message);

    /**
     * @brief 添加通信日志
     * @param message 消息
     */
    void addCommunication(const QString &message);

    /**
     * @brief 添加警告日志
     * @param message 消息
     */
    void addWarning(const QString &message);

    /**
     * @brief 添加错误日志
     * @param message 消息
     */
    void addError(const QString &message);

    /**
     * @brief 清空所有日志
     */
    void clearLogs();

    /**
     * @brief 设置最大日志条数
     * @param maxLines 最大行数
     */
    void setMaxLogLines(int maxLines);

    /**
     * @brief 设置自动滚动
     * @param enabled 是否启用
     */
    void setAutoScroll(bool enabled);

    /**
     * @brief 应用深色主题
     */
    void applyDarkTheme();

    /**
     * @brief 应用浅色主题
     */
    void applyLightTheme();

signals:
    /**
     * @brief 日志条目添加信号
     */
    void logEntryAdded(const LogEntry &entry);

private slots:
    void onClearClicked();
    void onExportClicked();
    void onFilterTextChanged(const QString &text);
    void onAutoScrollToggled(bool checked);

private:
    void setupUI();
    void addLogItem(const LogEntry &entry);
    QString getLevelIcon(OperationLogLevel level) const;
    QString getLevelColor(OperationLogLevel level) const;
    void applyTheme();          // 按当前主题重新着色所有条目
    void recolorAllItems();     // 用当前主题重新着色已有日志条目

    QListWidget *m_logListWidget;
    QLineEdit *m_filterEdit;
    QCheckBox *m_autoScrollCheckBox;
    QPushButton *m_clearButton;
    QPushButton *m_exportButton;
    int m_maxLogLines;
    bool m_autoScrollEnabled;
    bool m_darkTheme; // 当前主题（决定日志级别色取暗/亮调色板）
};

#endif // OPERATIONLOGWIDGET_H
