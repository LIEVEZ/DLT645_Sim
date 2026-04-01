#ifndef COMMUNICATIONBARWIDGET_H
#define COMMUNICATIONBARWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QHBoxLayout>
#include <QTimer>
#include <QDateTime>
#include "../../Services/ProtocolParser.h"

/**
 * @brief 通信细节栏组件
 *
 * 显示通信状态、帧计数、错误统计等信息
 */
class CommunicationBarWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CommunicationBarWidget(QWidget *parent = nullptr);
    ~CommunicationBarWidget();

    /**
     * @brief 更新连接状态
     * @param connected 是否连接
     */
    void updateConnectionStatus(bool connected);

    /**
     * @brief 更新帧计数
     * @param sent 发送帧数
     * @param received 接收帧数
     */
    void updateFrameCount(int sent, int received);

    /**
     * @brief 更新错误计数
     * @param errors 错误数
     */
    void updateErrorCount(int errors);

    /**
     * @brief 更新最后通信时间
     * @param timestamp 时间戳
     */
    void updateLastCommunication(const QDateTime &timestamp);

    /**
     * @brief 重置统计信息
     */
    void resetStatistics();

signals:
    /**
     * @brief 刷新请求信号
     */
    void refreshRequested();

private slots:
    void onRefreshTimer();

private:
    void setupUI();
    void updateDisplay();

    QLabel *m_connectionStatusLabel;
    QLabel *m_frameCountLabel;
    QLabel *m_errorCountLabel;
    QLabel *m_lastCommunicationLabel;

    bool m_connected;
    int m_framesSent;
    int m_framesReceived;
    int m_errorCount;
    QDateTime m_lastCommunication;
    QTimer *m_refreshTimer;
};

#endif // COMMUNICATIONBARWIDGET_H
