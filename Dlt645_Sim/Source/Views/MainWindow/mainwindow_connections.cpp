#include "mainwindow.h"
#include "Models/ConnectionInfoMapper.h"
#include "Controllers/ConnectionManager.h"
#include "Controllers/ProtocolController.h"
#include "Controllers/SerialController.h"
#include "ViewModels/MainWindowViewModel.h"

#include "Utils/Logger.h"
#include "Views/DeviceTree/DeviceTreeWidget.h"
#include "Views/Dialogs/CommunicationDialog.h"
#include "Views/Dialogs/NewConnectionDialog.h"
#include "Views/OperationLog/OperationLogWidget.h"

#include <QMessageBox>
void MainWindow::onNewConnection()
{
    NewConnectionDialog dialog(this);
    DeviceTreeWidget *deviceTree = deviceTreeWidget();
    if (deviceTree)
    {
        dialog.setConnectedPorts(deviceTree->connectedPortsExcept(QString()));
    }
    if (dialog.exec() == QDialog::Accepted)
    {
        const ConnectionConfig config = dialog.getConnectionConfig();
        openConnectionInternal(config.alias, config, true);
    }
}

void MainWindow::onOpenConnection()
{
    if (m_viewModel->currentConnectionAlias().isEmpty())
    {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请先选择一个连接"));
        return;
    }
    if (!m_viewModel->hasConnectionConfig(m_viewModel->currentConnectionAlias()))
    {
        QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("未找到连接配置"));
        return;
    }
    const ConnectionConfig config = m_viewModel->connectionConfig(m_viewModel->currentConnectionAlias());
    openConnectionInternal(m_viewModel->currentConnectionAlias(), config, false);
}

void MainWindow::onCloseConnection()
{
    if (!m_viewModel->isCloseConnectionVisible())
    {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("连接已经关闭"));
        return;
    }

    const QString connectionAlias = m_viewModel->currentConnectionAlias();
    closeConnectionInternal(connectionAlias);

    // 添加到操作日志
    if (m_operationLogWidget)
    {
        m_operationLogWidget->addInfo(QString("连接已关闭: %1").arg(connectionAlias));
    }

    LOG_INFO(QString("Connection closed: %1").arg(connectionAlias));
}

void MainWindow::onConnectionStateChanged(const QString &connectionAlias, bool connected)
{
    if (connected)
    {
        m_viewModel->markConnectionOpening(connectionAlias, true);
    }
    else
    {
        m_viewModel->markConnectionClosed(connectionAlias);
    }
    updateConnectionStatus(connected);

    // 按信号携带的连接别名更新节点，避免切换连接时污染其他节点状态。
    if (!connectionAlias.isEmpty())
    {
        DeviceTreeWidget *deviceTree = deviceTreeWidget();
        if (deviceTree)
        {
            deviceTree->setConnectionStatus(connectionAlias, connected);
        }
    }

    if (connected)
    {
        LOG_INFO(QString("Connection established: %1").arg(connectionAlias));
    }
    else
    {
        LOG_INFO(QString("Connection closed: %1").arg(connectionAlias));
    }
}

bool MainWindow::openConnectionInternal(const QString &connectionAlias, const ConnectionConfig &config, bool addConnectionNode)
{
    DeviceTreeWidget *deviceTree = deviceTreeWidget();
    const QString alias = connectionAlias.isEmpty() ? config.alias : connectionAlias;
    const QString endpoint = ConnectionManager::endpoint(config);
    if (deviceTree && deviceTree->isPortConnected(endpoint))
    {
        QMessageBox::warning(this, "Error", QString("Connection %1 is already in use").arg(endpoint));
        return false;
    }
    const ConnectionOpenResult openResult = m_connectionManager->openConnection(config);
    if (!openResult.started)
    {
        const QString errorText = openResult.errorMessage.isEmpty()
                                      ? QString("Failed to open connection: %1").arg(endpoint)
                                      : openResult.errorMessage;
        QMessageBox::critical(this, "Error", errorText);
        if (m_operationLogWidget)
        {
            m_operationLogWidget->addError(errorText);
        }
        return false;
    }

    attachProtocolTransport(alias);
    refreshProtocolMeters();

    const bool connectedNow = openResult.connected;
    if (deviceTree)
    {
        if (addConnectionNode)
        {
            deviceTree->addConnection(ConnectionInfoMapper::fromConfig(config, connectedNow));
        }
        else
        {
            deviceTree->setConnectionStatus(alias, connectedNow);
        }
    }
    m_viewModel->upsertConnectionConfig(config);
    m_viewModel->markConnectionOpening(alias, connectedNow);
    updateConnectionStatus(connectedNow);
    updateCommunicationDialogState();
    if (m_communicationDialog)
    {
        m_communicationDialog->clearAllMessages();
    }
    if (m_operationLogWidget)
    {
        const QString message = connectedNow
                                    ? QString("Connection opened: %1 (%2)").arg(alias, endpoint)
                                    : QString("Connecting: %1 (%2)").arg(alias, endpoint);
        if (connectedNow)
        {
            m_operationLogWidget->addSuccess(message);
        }
        else
        {
            m_operationLogWidget->addInfo(message);
        }
    }
    LOG_INFO(QString("%1: %2, Endpoint: %3")
                 .arg(connectedNow ? QStringLiteral("Connection opened") : QStringLiteral("Connection started"))
                 .arg(alias)
                 .arg(endpoint));
    return true;
}

void MainWindow::closeConnectionInternal(const QString &connectionAlias)
{
    const QString alias = connectionAlias.isEmpty()
                              ? m_viewModel->currentConnectionAlias()
                              : connectionAlias;

    if (m_connectionManager)
    {
        m_connectionManager->closeConnection(alias);
    }
    m_viewModel->markConnectionClosed(alias);
    DeviceTreeWidget *deviceTree = deviceTreeWidget();
    if (deviceTree && !alias.isEmpty())
    {
        deviceTree->setConnectionStatus(alias, false);
    }
    updateToolBarButtons();
    updateCommunicationDialogState();
}

void MainWindow::onConnectionSelected(const QString &connectionAlias)
{
    m_viewModel->selectConnection(connectionAlias);
    clearCurrentMeter();
}

void MainWindow::onOpenConnectionRequested(const QString &connectionAlias)
{
    if (m_connectionManager->isOpen(connectionAlias) ||
        m_connectionManager->isConnecting(connectionAlias))
    {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("连接已经打开"));
        return;
    }
    if (!m_viewModel->hasConnectionConfig(connectionAlias))
    {
        QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("未找到连接配置"));
        return;
    }
    const ConnectionConfig config = m_viewModel->connectionConfig(connectionAlias);
    openConnectionInternal(connectionAlias, config, false);
}

void MainWindow::onCloseConnectionRequested(const QString &connectionAlias)
{
    if (!m_connectionManager->isOpen(connectionAlias) &&
        !m_connectionManager->isConnecting(connectionAlias))
    {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("连接已经关闭"));
        return;
    }
    closeConnectionInternal(connectionAlias);
}

void MainWindow::onDeleteConnectionRequested(const QString &connectionAlias)
{
    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        QStringLiteral("确认删除"),
        QStringLiteral("确定要删除连接 \"%1\" 吗？").arg(connectionAlias),
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes)
    {
        const QStringList connectionMeterAddresses = collectMeterAddressesForConnection(connectionAlias);

        if (m_connectionManager->isOpen(connectionAlias) ||
            m_connectionManager->isConnecting(connectionAlias))
        {
            closeConnectionInternal(connectionAlias);
        }

        // 从设备树和协议注册表中清理该连接下的电表
        DeviceTreeWidget *deviceTree = deviceTreeWidget();
        if (deviceTree)
        {
            deviceTree->deleteConnection(connectionAlias);
        }

        clearMeterModels(connectionMeterAddresses);

        removeProtocolController(connectionAlias);
        m_connectionManager->removeConnection(connectionAlias);

        m_viewModel->removeConnectionConfig(connectionAlias);
        refreshMeterPresence();

        LOG_INFO(QString("Connection deleted: %1").arg(connectionAlias));
        QMessageBox::information(this, QStringLiteral("成功"), QStringLiteral("连接已删除"));
    }
}

void MainWindow::onEditConnectionRequested(const QString &connectionAlias)
{
    if (!m_viewModel->hasConnectionConfig(connectionAlias))
    {
        QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("未找到连接配置"));
        return;
    }

    if (m_connectionManager->isOpen(connectionAlias) ||
        m_connectionManager->isConnecting(connectionAlias))
    {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先关闭当前连接，再编辑连接配置"));
        return;
    }

    DeviceTreeWidget *deviceTree = deviceTreeWidget();
    ConnectionConfig originalConfig = m_viewModel->connectionConfig(connectionAlias);

    NewConnectionDialog dialog(this);
    dialog.setEditMode(true);
    dialog.setConnectionConfig(originalConfig);

    if (deviceTree)
    {
        dialog.setConnectedPorts(deviceTree->connectedPortsExcept(connectionAlias));
        dialog.setConnectionConfig(originalConfig);
    }

    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    const ConnectionConfig updatedConfig = dialog.getConnectionConfig();
    if (updatedConfig.alias != connectionAlias && m_viewModel->hasConnectionConfig(updatedConfig.alias))
    {
        QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("连接别名 %1 已存在").arg(updatedConfig.alias));
        return;
    }

    if (updatedConfig.type == ConnectionType::Serial && deviceTree && deviceTree->isPortConnected(updatedConfig.serialPort))
    {
        QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("串口 %1 已被连接，请使用其他串口").arg(updatedConfig.serialPort));
        return;
    }

    const QStringList connectionMeterAddresses = collectMeterAddressesForConnection(connectionAlias);
    ProtocolController *oldProtocolController = protocolController(connectionAlias);
    if (oldProtocolController)
    {
        oldProtocolController->setSerialController(nullptr);
        oldProtocolController->setTcpController(nullptr);
    }
    m_connectionManager->removeConnection(connectionAlias);

    if (updatedConfig.alias != connectionAlias)
    {
        m_viewModel->removeConnectionConfig(connectionAlias);
        removeProtocolController(connectionAlias);
    }
    m_viewModel->upsertConnectionConfig(updatedConfig);

    if (deviceTree)
    {
        deviceTree->updateConnection(connectionAlias, ConnectionInfoMapper::fromConfig(updatedConfig, false));
    }

    ProtocolController *updatedProtocolController = ensureProtocolController(updatedConfig.alias);
    for (const QString &address : connectionMeterAddresses)
    {
        MeterModel *meterModel = m_meterModelsByAddress.value(address, nullptr);
        if (meterModel)
        {
            updatedProtocolController->registerMeterModel(address, meterModel);
        }
    }
    refreshProtocolMeters();

    m_viewModel->selectConnection(updatedConfig.alias);

    LOG_INFO(QString("Connection updated: %1 -> %2")
                 .arg(connectionAlias, updatedConfig.alias));
    QMessageBox::information(this, QStringLiteral("成功"), QStringLiteral("连接配置已更新"));
}
