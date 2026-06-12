#include "mainwindow.h"
#include "Models/MeterModel.h"
#include "ViewModels/MainWindowViewModel.h"

#include "Utils/Logger.h"
#include "Views/OperationLog/OperationLogWidget.h"

#include <QAction>
#include <QLabel>
#include <QWidget>
void MainWindow::onDataUpdated()
{
    // 数据更新时刷新界面
    updateRealTimeDataDisplay(false);
}

void MainWindow::onRealTimeDataRotationTimeout()
{
    updateRealTimeDataDisplay(true);
}

void MainWindow::updateConnectionStatus(bool connected)
{
    Q_UNUSED(connected);
    if (m_connectionStatusLabel)
    {
        m_connectionStatusLabel->setText(m_viewModel->connectionStatusText());
        m_connectionStatusLabel->setStyleSheet(QString("padding: 2px 10px; color: %1;")
                                                   .arg(m_viewModel->connectionStatusColor().name()));
    }
    if (m_meterAddressLabel)
    {
        m_meterAddressLabel->setText(m_viewModel->meterAddressText());
    }
    updateToolBarButtons();
}

void MainWindow::updateToolBarButtons()
{
    // 更新连接按钮状态（互斥显示）
    if (m_viewModel->isCloseConnectionVisible())
    {
        m_openConnectionAction->setVisible(false);
        if (m_openConnectionSeparatorAction)
        {
            m_openConnectionSeparatorAction->setVisible(false);
        }
        m_closeConnectionAction->setVisible(true);
        if (m_closeConnectionSeparatorAction)
        {
            m_closeConnectionSeparatorAction->setVisible(true);
        }
    }
    else
    {
        m_openConnectionAction->setVisible(m_viewModel->isOpenConnectionVisible());
        if (m_openConnectionSeparatorAction)
        {
            m_openConnectionSeparatorAction->setVisible(m_viewModel->isOpenConnectionVisible());
        }
        m_closeConnectionAction->setVisible(false);
        if (m_closeConnectionSeparatorAction)
        {
            m_closeConnectionSeparatorAction->setVisible(false);
        }
    }

    // 更新新建电表按钮状态（未选中连接时禁用）
    m_newMeterAction->setEnabled(m_viewModel->isNewMeterEnabled());
    if (m_batchNewMeterAction)
    {
        m_batchNewMeterAction->setEnabled(m_viewModel->isNewMeterEnabled());
    }

    // 更新通信交互按钮状态（连接已打开时启用）
    m_commInteractionAction->setEnabled(m_viewModel->isCommunicationInteractionEnabled());
}

void MainWindow::updateRealTimeDataDisplay(bool advancePhase)
{
    if (!m_realTimeDataLabel || !m_realTimeBarWidget)
    {
        return;
    }

    const bool hasCurrentMeter = (m_currentMeterModel != nullptr) && !m_viewModel->currentMeterAddress().isEmpty();
    m_realTimeBarWidget->setVisible(hasCurrentMeter);
    if (!hasCurrentMeter)
    {
        return;
    }

    static const struct PhaseDisplayInfo
    {
        const char *name;
        const char *voltageCode;
        const char *currentCode;
        const char *angleCode;
    } phaseInfos[] = {
        {"A相", "0x02010100", "0x02020100", "0x02070100"},
        {"B相", "0x02010200", "0x02020200", "0x02070200"},
        {"C相", "0x02010300", "0x02020300", "0x02070300"}};

    const int phaseCount = (m_currentMeterModel && m_currentMeterModel->isSinglePhase())
                               ? 1
                               : static_cast<int>(sizeof(phaseInfos) / sizeof(phaseInfos[0]));
    if (advancePhase)
    {
        m_realTimePhaseIndex = (m_realTimePhaseIndex + 1) % phaseCount;
    }

    double forwardEnergy = 0.0;
    double reverseEnergy = 0.0;
    double phaseVoltage = 0.0;
    double phaseCurrent = 0.0;
    double phaseAngle = 0.0;

    if (m_currentMeterModel)
    {
        forwardEnergy = m_currentMeterModel->getDataValue("0x00010000").toDouble();
        reverseEnergy = m_currentMeterModel->getDataValue("0x00020000").toDouble();
        phaseVoltage = m_currentMeterModel->getDataValue(phaseInfos[m_realTimePhaseIndex].voltageCode).toDouble();
        phaseCurrent = m_currentMeterModel->getDataValue(phaseInfos[m_realTimePhaseIndex].currentCode).toDouble();
        phaseAngle = m_currentMeterModel->getDataValue(phaseInfos[m_realTimePhaseIndex].angleCode).toDouble();
    }

    m_realTimeDataLabel->setText(QStringLiteral("正向有功总电能: %1 | 反向有功总电能: %2 | %3电压: %4 | %3电流: %5 | %3相角: %6")
                                     .arg(forwardEnergy, 0, 'f', 2)
                                     .arg(reverseEnergy, 0, 'f', 2)
                                     .arg(QString::fromUtf8(phaseInfos[m_realTimePhaseIndex].name))
                                     .arg(phaseVoltage, 0, 'f', 1)
                                     .arg(phaseCurrent, 0, 'f', 3)
                                     .arg(phaseAngle, 0, 'f', 1));
}

void MainWindow::onLoggerLogEntryAdded(LogLevel level, const QDateTime &timestamp, const QString &message)
{
    Q_UNUSED(timestamp);
    if (!m_operationLogWidget)
    {
        return;
    }

    // 根据日志级别添加到操作日志
    switch (level)
    {
    case LogLevel::Debug:
        // 调试日志不显示到操作日志窗口
        break;
    case LogLevel::Info:
        m_operationLogWidget->addInfo(message);
        break;
    case LogLevel::Warning:
        m_operationLogWidget->addWarning(message);
        break;
    case LogLevel::Error:
    case LogLevel::Critical:
        m_operationLogWidget->addError(message);
        break;
    }
}

void MainWindow::onSearchTextChanged(const QString &text)
{
    // 搜索功能 - 过滤数据表格
    // TODO: 实现数据表格的搜索过滤功能
    LOG_INFO(QString("Search text changed: %1").arg(text));
}
