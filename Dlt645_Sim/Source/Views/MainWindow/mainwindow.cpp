#include "mainwindow.h"
#include "Controllers/ConnectionManager.h"
#include "Controllers/ProtocolController.h"
#include "Controllers/SerialController.h"
#include "Controllers/TcpController.h"
#include "ViewModels/MainWindowViewModel.h"
#include "ui_mainwindow.h"

#include "Services/MeterDataStore.h"
#include "Utils/Logger.h"
#include "Views/DataTable/DataTableWidget.h"
#include "Views/OperationLog/OperationLogWidget.h"
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow), m_serialController(new SerialController(this)), m_tcpController(new TcpController(this)), m_connectionManager(new ConnectionManager(m_serialController, m_tcpController, this)), m_protocolController(new ProtocolController(this)), m_viewModel(new MainWindowViewModel(this)), m_operationLogWidget(nullptr), m_dataTableWidget(nullptr), m_mainSplitter(nullptr), m_bottomSplitter(nullptr), m_connectionButtonStack(nullptr), m_connectionStatusLabel(nullptr), m_meterAddressLabel(nullptr), m_realTimeBarWidget(nullptr), m_realTimeDataLabel(nullptr), m_realTimeDataTimer(nullptr), m_realTimePhaseIndex(0), m_newConnectionAction(nullptr), m_newMeterAction(nullptr), m_batchNewMeterAction(nullptr), m_openConnectionAction(nullptr), m_openConnectionSeparatorAction(nullptr), m_closeConnectionAction(nullptr), m_closeConnectionSeparatorAction(nullptr), m_commInteractionAction(nullptr), m_masterQueryAction(nullptr), m_queryFrameAction(nullptr), m_frameParseAction(nullptr), m_dataItemManualAction(nullptr), m_darkThemeAction(nullptr), m_lightThemeAction(nullptr), m_communicationDialog(nullptr), m_masterQueryDialog(nullptr), m_queryFrameDialog(nullptr), m_frameParseDialog(nullptr), m_dataItemManualDialog(nullptr), m_meterDataStore(new MeterDataStore(this)), m_currentMeterModel(nullptr)
{
    ui->setupUi(this);
    setupUI();
    setupConnections();
    m_protocolController->setMeterModel(nullptr);
    if (m_dataTableWidget)
    {
        m_dataTableWidget->setMeterModel(nullptr);
    }
    applyDarkTheme();

    // 初始化工具栏按钮状态
    updateConnectionStatus(m_viewModel->isConnected());

    LOG_INFO("DLT645 Simulator started");
}

MainWindow::~MainWindow()
{

    if (m_operationLogWidget)
    {
        m_operationLogWidget->addInfo(QString("准备关闭连接: %1").arg(m_viewModel->currentConnectionAlias()));
    }
    closeConnectionInternal(m_viewModel->currentConnectionAlias());
    if (m_operationLogWidget)
    {
        m_operationLogWidget->addInfo(QString("连接已关闭: %1").arg(m_viewModel->currentConnectionAlias()));
    }
    LOG_INFO(QString("Connection closed: %1").arg(m_viewModel->currentConnectionAlias()));
    delete ui;
}

