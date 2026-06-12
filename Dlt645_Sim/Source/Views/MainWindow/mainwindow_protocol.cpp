#include "mainwindow.h"
#include "Services/ProtocolParser.h"

#include "Views/Dialogs/CommunicationDialog.h"
#include "Views/OperationLog/OperationLogWidget.h"

#include <QDateTime>
void MainWindow::onProtocolFrameReceived(const ProtocolFrame &frame, const QByteArray &rawFrame)
{
    // LOG_INFO(QString("Frame received: %1").arg(frame.toString()));
    const quint8 baseControlCode = frame.controlCode & 0x7F;
    const bool isResponse = (frame.controlCode & 0x80) != 0;
    const bool isErrorFrame = (baseControlCode == 0x51 || baseControlCode == 0x52);

    // 添加到操作日志
    if (m_operationLogWidget)
    {
        m_operationLogWidget->addCommunication(QString("接收帧: %1").arg(frame.toString()));
    }

    if (!m_communicationDialog)
    {
        return;
    }

    CommunicationMessage message;
    message.timestamp = QDateTime::currentDateTime();
    message.direction = MessageDirection::Receive;
    message.type = isErrorFrame ? MessageType::Error : (isResponse ? MessageType::Response : MessageType::Request);
    message.controlCode = frame.controlCode;
    message.dataLength = frame.dataLength;
    message.fullFrame = rawFrame;
    message.checksumValid = true;
    m_communicationDialog->addMessage(message);
}

void MainWindow::onProtocolFrameSent(const ProtocolFrame &frame, const QByteArray &rawFrame)
{
    // LOG_INFO(QString("===Frame sent: %1").arg(frame.toString()));
    const quint8 baseControlCode = frame.controlCode & 0x7F;
    const bool isResponse = (frame.controlCode & 0x80) != 0;
    const bool isErrorFrame = (baseControlCode == 0x51 || baseControlCode == 0x52);

    // 添加到操作日志
    if (m_operationLogWidget)
    {
        m_operationLogWidget->addCommunication(QString("发送帧: %1").arg(frame.toString()));
    }

    if (!m_communicationDialog)
    {
        return;
    }

    CommunicationMessage message;
    message.timestamp = QDateTime::currentDateTime();
    message.direction = MessageDirection::Transmit;
    message.type = isErrorFrame ? MessageType::Error : (isResponse ? MessageType::Response : MessageType::Request);
    message.controlCode = frame.controlCode;
    message.dataLength = frame.dataLength;
    message.fullFrame = rawFrame;
    message.checksumValid = true;
    m_communicationDialog->addMessage(message);
}

void MainWindow::onRawDataReceived(const QByteArray &data)
{
    Q_UNUSED(data);
}

void MainWindow::onRawDataSent(const QByteArray &data)
{
    Q_UNUSED(data);
}
