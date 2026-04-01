#include "CommunicationBarWidget.h"
#include "Services/ProtocolParser.h"
#include <QDateTime>

CommunicationBarWidget::CommunicationBarWidget(QWidget *parent)
    : QWidget(parent), m_connectionStatusLabel(new QLabel(this)), m_frameCountLabel(new QLabel(this)), m_errorCountLabel(new QLabel(this)), m_lastCommunicationLabel(new QLabel(this)), m_connected(false), m_framesSent(0), m_framesReceived(0), m_errorCount(0), m_refreshTimer(new QTimer(this))
{
    setupUI();

    // 设置刷新定时器
    m_refreshTimer->setInterval(1000); // 1秒刷新一次
    connect(m_refreshTimer, &QTimer::timeout, this, &CommunicationBarWidget::onRefreshTimer);
    m_refreshTimer->start();
}

CommunicationBarWidget::~CommunicationBarWidget()
{
    if (m_refreshTimer)
    {
        m_refreshTimer->stop();
    }
}

void CommunicationBarWidget::setupUI()
{
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(10, 5, 10, 5);
    mainLayout->setSpacing(20);

    // 连接状态
    m_connectionStatusLabel->setText("未连接");
    m_connectionStatusLabel->setStyleSheet("color: #F14C4C; font-weight: bold;");
    mainLayout->addWidget(m_connectionStatusLabel);

    mainLayout->addSpacing(30);

    // 帧计数
    m_frameCountLabel->setText("发送: 0 | 接收: 0");
    mainLayout->addWidget(m_frameCountLabel);

    mainLayout->addSpacing(30);

    // 错误计数
    m_errorCountLabel->setText("错误: 0");
    m_errorCountLabel->setStyleSheet("color: #FFA500;");
    mainLayout->addWidget(m_errorCountLabel);

    mainLayout->addSpacing(30);

    // 最后通信时间
    m_lastCommunicationLabel->setText("最后通信: --");
    mainLayout->addWidget(m_lastCommunicationLabel);

    mainLayout->addStretch();

    // // 设置样式
    // setStyleSheet(R"(
    //     CommunicationBarWidget {
    //         background-color: #2D2D30;
    //         border-bottom: 1px solid #3C3F41;
    //     }

    //     QLabel {
    //         color: #D4D4D4;
    //         font-family: "Microsoft YaHei";
    //         font-size: 10pt;
    //     }
    // )");
}

void CommunicationBarWidget::updateConnectionStatus(bool connected)
{
    m_connected = connected;
    updateDisplay();
}

void CommunicationBarWidget::updateFrameCount(int sent, int received)
{
    m_framesSent = sent;
    m_framesReceived = received;
    updateDisplay();
}

void CommunicationBarWidget::updateErrorCount(int errors)
{
    m_errorCount = errors;
    updateDisplay();
}

void CommunicationBarWidget::updateLastCommunication(const QDateTime &timestamp)
{
    m_lastCommunication = timestamp;
    updateDisplay();
}

void CommunicationBarWidget::resetStatistics()
{
    m_framesSent = 0;
    m_framesReceived = 0;
    m_errorCount = 0;
    m_lastCommunication = QDateTime();
    updateDisplay();
}

void CommunicationBarWidget::onRefreshTimer()
{
    updateDisplay();
}

void CommunicationBarWidget::updateDisplay()
{
    // 更新连接状态
    if (m_connected)
    {
        m_connectionStatusLabel->setText("已连接");
        m_connectionStatusLabel->setStyleSheet("color: #50C878; font-weight: bold;");
    }
    else
    {
        m_connectionStatusLabel->setText("未连接");
        m_connectionStatusLabel->setStyleSheet("color: #F14C4C; font-weight: bold;");
    }

    // 更新帧计数
    m_frameCountLabel->setText(QString("发送: %1 | 接收: %2")
                                   .arg(m_framesSent)
                                   .arg(m_framesReceived));

    // 更新错误计数
    m_errorCountLabel->setText(QString("错误: %1").arg(m_errorCount));
    if (m_errorCount > 0)
    {
        m_errorCountLabel->setStyleSheet("color: #FF4444; font-weight: bold;");
    }
    else
    {
        m_errorCountLabel->setStyleSheet("color: #50C878;");
    }

    // 更新最后通信时间
    if (m_lastCommunication.isValid())
    {
        QString timeStr = m_lastCommunication.toString("HH:mm:ss");
        m_lastCommunicationLabel->setText(QString("最后通信: %1").arg(timeStr));
    }
    else
    {
        m_lastCommunicationLabel->setText("最后通信: --");
    }
}
