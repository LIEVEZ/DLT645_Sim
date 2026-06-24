#include "mainwindow.h"
#include "Controllers/ProtocolController.h"
#include "Controllers/SerialController.h"
#include "ViewModels/MainWindowViewModel.h"

#include "Views/Dialogs/CommunicationDialog.h"
#include "Views/Dialogs/EventRecordDialog.h"
#include "Views/Dialogs/FrameParseDialog.h"
#include "Views/Dialogs/DataItemManualDialog.h"
#include "Views/Dialogs/MasterQueryDialog.h"
#include "Views/Dialogs/QueryFrameDialog.h"

#include <QMessageBox>
#include <QString>
#include <QStringList>
void MainWindow::updateCommunicationDialogState()
{
    if (!m_communicationDialog)
    {
        return;
    }

    if (m_viewModel->hasConnectionConfig(m_viewModel->currentConnectionAlias()))
    {
        ConnectionConfig config = m_viewModel->connectionConfig(m_viewModel->currentConnectionAlias());
        if (config.type == ConnectionType::Serial)
        {
            m_communicationDialog->setConnectionInfo(config.serialPort, config.baudRate);
        }
        else
        {
            m_communicationDialog->setConnectionInfo(config.tcpAddress, config.tcpPort);
        }
    }

    m_communicationDialog->setConnectionStatus(m_viewModel->isConnected());
}

void MainWindow::updateMasterQueryDialogState()
{
    if (!m_masterQueryDialog)
    {
        return;
    }

    QStringList unavailablePorts;
    if (m_serialController && m_serialController->isOpen())
    {
        const QString portName = m_serialController->getConfig().portName;
        if (!portName.isEmpty())
        {
            unavailablePorts.append(portName);
        }
    }

    m_masterQueryDialog->setUnavailablePorts(unavailablePorts);
}

void MainWindow::updateQueryFrameDialogState()
{
    if (!m_queryFrameDialog)
    {
        return;
    }

    m_queryFrameDialog->setMeterModel(m_currentMeterModel);
    if (m_currentMeterModel)
    {
        m_queryFrameDialog->setDefaultMeterAddress(m_currentMeterModel->getMeterAddress());
        m_queryFrameDialog->setIncludePrefixFE(m_currentMeterModel->getIncludePrefixFE());
    }
    else
    {
        m_queryFrameDialog->setDefaultMeterAddress(QString());
        m_queryFrameDialog->setIncludePrefixFE(false);
    }
}

void MainWindow::updateFrameParseDialogState()
{
    if (!m_frameParseDialog)
    {
        return;
    }

    m_frameParseDialog->setMeterModel(m_currentMeterModel);
}

void MainWindow::onCommunicationInteraction()
{
    // 如果窗口已经存在，则显示并激活
    if (m_communicationDialog)
    {
        m_communicationDialog->show();
        m_communicationDialog->raise();
        m_communicationDialog->activateWindow();
        return;
    }

    // 创建通信交互窗口
    m_communicationDialog = new CommunicationDialog(m_viewModel->currentConnectionAlias(), this);
    m_communicationDialog->setMeterModel(m_protocolController->meterModel());

    if (m_viewModel->isDarkTheme())
    {
        m_communicationDialog->applyDarkTheme();
    }
    else
    {
        m_communicationDialog->applyLightTheme();
    }

    if (m_viewModel->hasConnectionConfig(m_viewModel->currentConnectionAlias()))
    {
        ConnectionConfig config = m_viewModel->connectionConfig(m_viewModel->currentConnectionAlias());
        if (config.type == ConnectionType::Serial)
        {
            m_communicationDialog->setConnectionInfo(config.serialPort, config.baudRate);
        }
        else
        {
            m_communicationDialog->setConnectionInfo(config.tcpAddress, config.tcpPort);
        }
    }

    // 设置连接状态
    m_communicationDialog->setConnectionStatus(m_viewModel->isConnected());

    // 连接窗口关闭信号
    connect(m_communicationDialog, &CommunicationDialog::dialogClosed,
            this, &MainWindow::onCommunicationDialogClosed);

    // 显示窗口
    m_communicationDialog->show();
}

void MainWindow::onCommunicationDialogClosed()
{
    // 手动删除窗口
    if (m_communicationDialog)
    {
        m_communicationDialog->deleteLater();
        m_communicationDialog = nullptr;
    }
}

void MainWindow::onMasterQueryWindow()
{
    if (m_masterQueryDialog)
    {
        updateMasterQueryDialogState();
        m_masterQueryDialog->show();
        m_masterQueryDialog->raise();
        m_masterQueryDialog->activateWindow();
        return;
    }

    m_masterQueryDialog = new MasterQueryDialog(this);
    updateMasterQueryDialogState();

    if (m_currentMeterModel)
    {
        m_masterQueryDialog->setDefaultMeterAddress(m_currentMeterModel->getMeterAddress());
        m_masterQueryDialog->setDefaultIncludePrefixFE(m_currentMeterModel->getIncludePrefixFE());
    }

    if (m_viewModel->isDarkTheme())
    {
        m_masterQueryDialog->applyDarkTheme();
    }
    else
    {
        m_masterQueryDialog->applyLightTheme();
    }

    connect(m_masterQueryDialog, &MasterQueryDialog::dialogClosed,
            this, &MainWindow::onMasterQueryDialogClosed);

    m_masterQueryDialog->show();
}

void MainWindow::onMasterQueryDialogClosed()
{
    if (m_masterQueryDialog)
    {
        m_masterQueryDialog->deleteLater();
        m_masterQueryDialog = nullptr;
    }
}

void MainWindow::onQueryFrameGenerator()
{
    if (m_queryFrameDialog)
    {
        updateQueryFrameDialogState();
        m_queryFrameDialog->show();
        m_queryFrameDialog->raise();
        m_queryFrameDialog->activateWindow();
        return;
    }

    m_queryFrameDialog = new QueryFrameDialog(this);
    updateQueryFrameDialogState();

    if (m_viewModel->isDarkTheme())
    {
        m_queryFrameDialog->applyDarkTheme();
    }
    else
    {
        m_queryFrameDialog->applyLightTheme();
    }

    connect(m_queryFrameDialog, &QueryFrameDialog::dialogClosed,
            this, &MainWindow::onQueryFrameDialogClosed);

    m_queryFrameDialog->show();
}

void MainWindow::onQueryFrameDialogClosed()
{
    if (m_queryFrameDialog)
    {
        m_queryFrameDialog->deleteLater();
        m_queryFrameDialog = nullptr;
    }
}

void MainWindow::onFrameParseDialog()
{
    if (m_frameParseDialog)
    {
        updateFrameParseDialogState();
        m_frameParseDialog->show();
        m_frameParseDialog->raise();
        m_frameParseDialog->activateWindow();
        return;
    }

    m_frameParseDialog = new FrameParseDialog(this);
    updateFrameParseDialogState();

    if (m_viewModel->isDarkTheme())
    {
        m_frameParseDialog->applyDarkTheme();
    }
    else
    {
        m_frameParseDialog->applyLightTheme();
    }

    m_frameParseDialog->show();
}

void MainWindow::onDataItemManualDialog()
{
    if (m_dataItemManualDialog)
    {
        m_dataItemManualDialog->show();
        m_dataItemManualDialog->raise();
        m_dataItemManualDialog->activateWindow();
        return;
    }

    m_dataItemManualDialog = new DataItemManualDialog(this);

    if (m_viewModel->isDarkTheme())
    {
        m_dataItemManualDialog->applyDarkTheme();
    }
    else
    {
        m_dataItemManualDialog->applyLightTheme();
    }

    m_dataItemManualDialog->show();
}

