#include "mainwindow.h"
#include "Services/ProtocolParser.h"
#include "ViewModels/MainWindowViewModel.h"

#include "Views/Dialogs/CommunicationDialog.h"
#include "Views/OperationLog/OperationLogWidget.h"

#include <QDateTime>
void MainWindow::onProtocolFrameReceived(const QString &connectionAlias,
                                         const ProtocolFrame &frame,
                                         const QByteArray &rawFrame)
{
    // LOG_INFO(QString("Frame received: %1").arg(frame.toString()));
    const quint8 baseControlCode = frame.controlCode & 0x7F;
    const bool isResponse = (frame.controlCode & 0x80) != 0;
    const bool isErrorFrame = (baseControlCode == 0x51 || baseControlCode == 0x52);

    // 添加到操作日志
    if (m_operationLogWidget)
    {
        m_operationLogWidget->addCommunication(QString("[%1] 接收帧: %2")
                                                    .arg(connectionAlias, frame.toString()));
    }

    if (!m_communicationDialog ||
        m_viewModel->currentConnectionAlias() != connectionAlias)
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

void MainWindow::onProtocolFrameSent(const QString &connectionAlias,
                                     const ProtocolFrame &frame,
                                     const QByteArray &rawFrame)
{
    // LOG_INFO(QString("===Frame sent: %1").arg(frame.toString()));
    const quint8 baseControlCode = frame.controlCode & 0x7F;
    const bool isResponse = (frame.controlCode & 0x80) != 0;
    const bool isErrorFrame = (baseControlCode == 0x51 || baseControlCode == 0x52);

    // 添加到操作日志
    if (m_operationLogWidget)
    {
        m_operationLogWidget->addCommunication(QString("[%1] 发送帧: %2")
                                                    .arg(connectionAlias, frame.toString()));
    }

    if (!m_communicationDialog ||
        m_viewModel->currentConnectionAlias() != connectionAlias)
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

void MainWindow::onRawDataReceived(const QString &connectionAlias, const QByteArray &data)
{
    Q_UNUSED(connectionAlias);
    Q_UNUSED(data);
}

void MainWindow::onRawDataSent(const QString &connectionAlias, const QByteArray &data)
{
    Q_UNUSED(connectionAlias);
    Q_UNUSED(data);
}
