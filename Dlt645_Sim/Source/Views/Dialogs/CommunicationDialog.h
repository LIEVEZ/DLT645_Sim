#ifndef COMMUNICATIONDIALOG_H
#define COMMUNICATIONDIALOG_H

#include <QDialog>
#include <QCloseEvent>
#include <QTableWidget>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QLineEdit>
#include <QComboBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QSplitter>
#include <QTimer>
#include <QDateTime>

class MeterModel;
class FrameParseResultWidget;

/**
 * @brief 报文方向枚举
 */
enum class MessageDirection
{
    Receive, // 接收 (R <-)
    Transmit // 发送 (T ->)
};

/**
 * @brief 报文类型枚举
 */
enum class MessageType
{
    Request,  // 请求帧
    Response, // 应答帧
    Error     // 错误帧
};

/**
 * @brief 报文显示格式枚举
 */
enum class DisplayFormat
{
    Hex,  // HEX格式
    ASCII // ASCII格式
};

/**
 * @brief 通信报文结构
 */
struct CommunicationMessage
{
    QDateTime timestamp;        // 时间戳
    MessageDirection direction; // 方向
    MessageType type;           // 类型
    quint8 controlCode;         // 控制码
    quint8 dataLength;          // 数据长度
    QByteArray fullFrame;       // 完整报文
    QString parsedData;         // 解析后的数据
    bool checksumValid;         // 校验和是否有效

    CommunicationMessage()
        : direction(MessageDirection::Receive), type(MessageType::Request), controlCode(0), dataLength(0), checksumValid(false)
    {
    }
};

/**
 * @brief 通信统计信息结构
 */
struct CommunicationStatistics
{
    int sentFrames;     // 发送帧数
    int receivedFrames; // 接收帧数
    int errorFrames;    // 错误帧数
    double successRate; // 成功率

    CommunicationStatistics()
        : sentFrames(0), receivedFrames(0), errorFrames(0), successRate(100.0)
    {
    }
};

/**
 * @brief 通信交互窗口
 *
 * 用于监视和显示DLT645协议通信报文
 * 纯监视模式，不支持主动发送报文
 */
class CommunicationDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CommunicationDialog(const QString &connectionName, QWidget *parent = nullptr);
    ~CommunicationDialog();

    /**
     * @brief 设置连接信息
     * @param portName 串口号
     * @param baudRate 波特率
     */
    void setConnectionInfo(const QString &portName, int baudRate);

    /**
     * @brief 设置连接状态
     * @param connected 是否已连接
     */
    void setConnectionStatus(bool connected);

    /**
     * @brief 应用深色主题
     */
    void applyDarkTheme();

    /**
     * @brief 应用浅色主题
     */
    void applyLightTheme();
    void setMeterModel(MeterModel *meterModel);

    /**
     * @brief 添加通信报文
     * @param message 报文数据
     */
    void addMessage(const CommunicationMessage &message);

    /**
     * @brief 清空所有报文
     */
    void clearAllMessages();

    /**
     * @brief 获取统计信息
     * @return 统计信息
     */
    CommunicationStatistics getStatistics() const;

    /**
     * @brief 导出报文记录
     * @param filePath 文件路径
     * @return 是否成功
     */
    bool exportMessages(const QString &filePath);

public slots:
    /**
     * @brief 暂停/恢复报文显示
     */
    void togglePause();

    /**
     * @brief 刷新报文列表
     */
    void refreshMessageList();

signals:
    /**
     * @brief 报文添加信号
     * @param message 报文数据
     */
    void messageAdded(const CommunicationMessage &message);

    /**
     * @brief 窗口关闭信号
     */
    void dialogClosed();

private slots:
    void onClearClicked();
    void onExportClicked();
    void onFilterChanged(const QString &text);
    void onFilterTypeChanged();
    void onDisplayFormatChanged(int index);
    void onMessageSelectionChanged();
    void onAutoScrollChanged(int state);

private:
    void setupUI();
    void setupConnectionInfoBar();
    void setupFilterBar();
    void setupMessageTable();
    void setupMessageDetail();
    void setupStatisticsBar();
    void updateStatistics();
    void updateConnectionStatus();
    void parseFrame(const QByteArray &frame, QString &parsedResult);
    QString formatMessage(const CommunicationMessage &message, DisplayFormat format) const;
    QString formatHex(const QByteArray &data) const;
    QString formatAscii(const QByteArray &data) const;
    QString getControlCodeName(quint8 code) const;
    QString getMessageTypeIcon(MessageType type) const;
    QString getDirectionIcon(MessageDirection direction) const;
    bool shouldShowMessage(const CommunicationMessage &message) const;
    void updateMessageDetail(int row);
    void closeEvent(QCloseEvent *event) override;

    // 连接信息
    QString m_connectionName;
    QString m_portName;
    int m_baudRate;
    bool m_isConnected;
    MeterModel *m_meterModel;

    // UI控件
    QWidget *m_connectionInfoBar;  // 连接信息栏容器
    QLabel *m_labelConnectionInfo; // 连接信息标签
    QLabel *m_labelStatus;
    QPushButton *m_btnClear;
    QPushButton *m_btnPause;
    QPushButton *m_btnExport;

    QWidget *m_filterBar; // 过滤栏容器
    QLineEdit *m_lineEditFilter;
    QCheckBox *m_checkBoxRequest;
    QCheckBox *m_checkBoxResponse;
    QCheckBox *m_checkBoxError;
    QComboBox *m_comboDisplayFormat;

    QTableWidget *m_tableMessages;
    FrameParseResultWidget *m_detailWidget;

    QLabel *m_labelStatistics;

    // 数据
    QList<CommunicationMessage> m_messages;
    CommunicationStatistics m_statistics;
    bool m_isPaused;
    bool m_autoScroll;
    DisplayFormat m_displayFormat;

    // 过滤器
    bool m_filterRequest;
    bool m_filterResponse;
    bool m_filterError;
    QString m_filterText;
};

#endif // COMMUNICATIONDIALOG_H
