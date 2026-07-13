#include "mainwindow.h"
#include "Version.h"
#include "Controllers/ConnectionManager.h"
#include "Controllers/ProtocolController.h"
#include "Controllers/SerialController.h"
#include "Controllers/TcpController.h"
#include "Services/DataGenerator.h"
#include "ViewModels/MainWindowViewModel.h"

#include "Utils/Logger.h"
#include "Views/DataTable/DataTableWidget.h"
#include "Views/DeviceTree/DeviceTreeWidget.h"
#include "Views/OperationLog/OperationLogWidget.h"

#include <QAction>
#include <QIcon>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSize>
#include <QSplitter>
#include <QStatusBar>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>
void MainWindow::setupUI()
{
    // 设置窗口属性
    setWindowTitle(QStringLiteral("DLT645 电表模拟器"));
    setMinimumSize(1200, 800);
    resize(1440, 900);

    // 设置菜单栏
    setupMenuBar();

    // 设置工具栏
    setupToolBar();

    // 设置状态栏
    setupStatusBar();

    // 创建底部分割器（垂直）
    m_bottomSplitter = new QSplitter(Qt::Vertical, this);

    // 上部：通信细节栏组件（显示实时数据）
    m_realTimeBarWidget = new QWidget(this);
    QHBoxLayout *commLayout = new QHBoxLayout(m_realTimeBarWidget);
    m_realTimeBarWidget->setObjectName("communicationBarWidget");
    commLayout->setContentsMargins(10, 5, 10, 5);
    commLayout->setSpacing(15);

    QLabel *commLabel = new QLabel("实时数据:", m_realTimeBarWidget);
    commLayout->addWidget(commLabel);

    // 实时数据显示
    m_realTimeDataLabel = new QLabel("正向有功总电能: 000000.00 | 反向有功总电能: 000000.00 | A相电压: 220.0 | A相电流: 5.000 | A相相角: 0.0", m_realTimeBarWidget);
    commLayout->addWidget(m_realTimeDataLabel);

    commLayout->addStretch();

    m_realTimeDataTimer = new QTimer(this);
    m_realTimeDataTimer->setInterval(2000);
    connect(m_realTimeDataTimer, &QTimer::timeout, this, &MainWindow::onRealTimeDataRotationTimeout);
    m_realTimeDataTimer->start();
    updateRealTimeDataDisplay(false);

    // 添加到底部分割器
    m_bottomSplitter->addWidget(m_realTimeBarWidget);

    // 创建中央布局
    QWidget *centralWidget = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 创建主分割器（水平）
    m_mainSplitter = new QSplitter(Qt::Horizontal, this);

    // 左侧：设备树容器
    QWidget *leftContainer = new QWidget(this);
    QVBoxLayout *leftLayout = new QVBoxLayout(leftContainer);
    leftContainer->setObjectName("leftContainer");
    leftLayout->setContentsMargins(5, 5, 5, 5);
    leftLayout->setSpacing(5);

    // 设备树组件
    DeviceTreeWidget *deviceTreeWidget = new DeviceTreeWidget(leftContainer);
    leftLayout->addWidget(deviceTreeWidget);

    // 中部：数据表格容器
    QWidget *centerContainer = new QWidget(this);
    QVBoxLayout *centerLayout = new QVBoxLayout(centerContainer);
    centerContainer->setObjectName("centerContainer");
    centerLayout->setContentsMargins(5, 5, 5, 5);
    centerLayout->setSpacing(5);

    // 数据表格组件
    m_dataTableWidget = new DataTableWidget(centerContainer);
    centerLayout->addWidget(m_dataTableWidget);

    // 右侧：操作日志容器
    QWidget *rightContainer = new QWidget(this);
    QVBoxLayout *rightLayout = new QVBoxLayout(rightContainer);
    rightContainer->setObjectName("rightContainer");
    rightLayout->setContentsMargins(5, 5, 5, 5);
    rightLayout->setSpacing(5);

    QLabel *rightLabel = new QLabel("操作日志", rightContainer);
    rightLayout->addWidget(rightLabel);

    // 操作日志组件
    m_operationLogWidget = new OperationLogWidget(rightContainer);
    rightLayout->addWidget(m_operationLogWidget);

    // 添加到分割器
    m_mainSplitter->addWidget(leftContainer);
    m_mainSplitter->addWidget(centerContainer);
    m_mainSplitter->addWidget(rightContainer);
    m_mainSplitter->setSizes(QList<int>() << 250 << 950 << 240);
    m_mainSplitter->setStretchFactor(0, 1);
    m_mainSplitter->setStretchFactor(1, 4);
    m_mainSplitter->setStretchFactor(2, 1);

    mainLayout->addWidget(m_mainSplitter);

    // 添加到底部分割器
    m_bottomSplitter->addWidget(centralWidget);
    m_bottomSplitter->setSizes(QList<int>() << 50 << 700);
    m_bottomSplitter->setStretchFactor(0, 0);
    m_bottomSplitter->setStretchFactor(1, 1);

    setCentralWidget(m_bottomSplitter);
}

void MainWindow::setupConnections()
{
    connect(m_viewModel, &MainWindowViewModel::stateChanged, this, [this]()
            {
                updateConnectionStatus(m_viewModel->isConnected());
                updateCommunicationDialogState();
                updateMasterQueryDialogState(); });
    connect(m_viewModel, &MainWindowViewModel::hasMeterChanged,
            m_protocolController, &ProtocolController::setHasMeter);

    // 连接协议控制器和通信控制器
    m_protocolController->setSerialController(m_serialController);
    m_protocolController->setTcpController(m_tcpController);

    // 连接通信状态信号
    connect(m_connectionManager, &ConnectionManager::connectionStateChanged,
            this, &MainWindow::onConnectionStateChanged);
    connect(m_connectionManager, &ConnectionManager::errorOccurred,
            this, [this](const QString &errorString)
            {
                if (m_operationLogWidget)
                {
                    m_operationLogWidget->addError(errorString);
                }
                QMessageBox::warning(this, QStringLiteral("Connection Error"), errorString);
                m_viewModel->markConnectionClosed(m_viewModel->currentConnectionAlias());
                updateConnectionStatus(false);
                updateCommunicationDialogState();
            });

    // 连接协议控制器信号
    connect(m_protocolController, &ProtocolController::frameReceived,
            this, &MainWindow::onProtocolFrameReceived);
    connect(m_protocolController, &ProtocolController::frameSent,
            this, &MainWindow::onProtocolFrameSent);
    connect(m_protocolController, &ProtocolController::rawDataReceived,
            this, &MainWindow::onRawDataReceived);
    connect(m_protocolController, &ProtocolController::rawDataSent,
            this, &MainWindow::onRawDataSent);

    // 连接数据生成器信号
    connect(m_dataGenerator, &DataGenerator::dataUpdated,
            this, &MainWindow::onDataUpdated);

    // 连接 Logger 信号到操作日志
    connect(Logger::instance(), &Logger::logEntryAdded,
            this, &MainWindow::onLoggerLogEntryAdded);

    // 连接设备树的连接选中信号
    DeviceTreeWidget *deviceTree = deviceTreeWidget();
    if (deviceTree)
    {
        connect(deviceTree, &DeviceTreeWidget::connectionSelected,
                this, &MainWindow::onConnectionSelected);
        connect(deviceTree, &DeviceTreeWidget::meterSelected,
                this, &MainWindow::onMeterSelected);
        connect(deviceTree, &DeviceTreeWidget::meterInfoSelected,
                this, &MainWindow::onMeterInfoSelected);
        // 将 categorySelected 信号连接到 DataTableWidget
        connect(deviceTree, &DeviceTreeWidget::categorySelected,
                m_dataTableWidget, &DataTableWidget::onCategorySelected);
        connect(deviceTree, &DeviceTreeWidget::openConnectionRequested,
                this, &MainWindow::onOpenConnectionRequested);
        connect(deviceTree, &DeviceTreeWidget::closeConnectionRequested,
                this, &MainWindow::onCloseConnectionRequested);
        connect(deviceTree, &DeviceTreeWidget::deleteConnectionRequested,
                this, &MainWindow::onDeleteConnectionRequested);
        connect(deviceTree, &DeviceTreeWidget::deleteAllMetersRequested,
                this, &MainWindow::onDeleteAllMetersRequested);
        connect(deviceTree, &DeviceTreeWidget::editConnectionRequested,
                this, &MainWindow::onEditConnectionRequested);
        connect(deviceTree, &DeviceTreeWidget::newMeterRequested,
                this, &MainWindow::onNewMeterRequested);
        connect(deviceTree, &DeviceTreeWidget::batchNewMeterRequested,
                this, &MainWindow::onBatchNewMeterRequested);
        connect(deviceTree, &DeviceTreeWidget::deleteMeterRequested,
                this, &MainWindow::onDeleteMeterRequested);
        connect(deviceTree, &DeviceTreeWidget::eventRecordSettingsRequested,
                this, &MainWindow::onEventRecordSettingsRequested);
    }
}

void MainWindow::setupMenuBar()
{
    QMenuBar *menuBar = this->menuBar();

    // 文件菜单
    QMenu *fileMenu = menuBar->addMenu(QStringLiteral("文件(&F)"));

    QAction *exitAction = fileMenu->addAction(QStringLiteral("退出(&X)"));
    QAction *importConfigAction = fileMenu->addAction(QStringLiteral("导入配置..."));
    importConfigAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+O")));
    connect(importConfigAction, &QAction::triggered, this, &MainWindow::onImportConfiguration);

    QAction *exportConfigAction = fileMenu->addAction(QStringLiteral("导出配置..."));
    exportConfigAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+S")));
    connect(exportConfigAction, &QAction::triggered, this, &MainWindow::onExportConfiguration);

    fileMenu->insertAction(exitAction, importConfigAction);
    fileMenu->insertAction(exitAction, exportConfigAction);
    fileMenu->insertSeparator(exitAction);

    exitAction->setShortcut(QKeySequence("Ctrl+Q"));
    connect(exitAction, &QAction::triggered, this, &QMainWindow::close);

    // 视图菜单
    QMenu *viewMenu = menuBar->addMenu(QStringLiteral("视图(&V)"));

    m_darkThemeAction = viewMenu->addAction(QStringLiteral("深色主题"));
    m_darkThemeAction->setCheckable(true);
    m_darkThemeAction->setChecked(true);
    connect(m_darkThemeAction, &QAction::triggered, this, &MainWindow::onToggleDarkTheme);

    m_lightThemeAction = viewMenu->addAction(QStringLiteral("浅色主题"));
    m_lightThemeAction->setCheckable(true);
    m_lightThemeAction->setChecked(false);
    connect(m_lightThemeAction, &QAction::triggered, this, &MainWindow::onToggleLightTheme);

    // 工具菜单
    QMenu *toolsMenu = menuBar->addMenu(QStringLiteral("工具(&T)"));

    QAction *dataItemManualAction = toolsMenu->addAction(QStringLiteral("数据项手册(&M)"));
    dataItemManualAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+H")));
    dataItemManualAction->setIcon(QIcon(QStringLiteral(":/icons/manual.svg")));
    connect(dataItemManualAction, &QAction::triggered, this, &MainWindow::onDataItemManualDialog);

    // 帮助菜单
    QMenu *helpMenu = menuBar->addMenu(QStringLiteral("帮助(&H)"));

    QAction *aboutAction = helpMenu->addAction(QStringLiteral("关于(&A)"));
    connect(aboutAction, &QAction::triggered, [=]()
            { QMessageBox::about(this, QStringLiteral("关于 " APP_PRODUCT_NAME),
                                 QStringLiteral(APP_PRODUCT_NAME "\n\n"
                                                "软件版本: " APP_VERSION_SW "\n"
                                                "硬件版本: " APP_VERSION_HW "\n"
                                                "固件版本: " APP_VERSION_FW "\n"
                                                "构建日期: " APP_BUILD_DATE "\n\n"
                                                "基于 Qt5 开发的工业级电表模拟器\n\n"
                                                "支持串口通信、七类数据项、实时数据模拟")); });
}

void MainWindow::setupToolBar()
{
    QToolBar *toolBar = addToolBar(QStringLiteral("主工具栏"));
    toolBar->setMovable(false);
    toolBar->setFloatable(false);
    toolBar->setObjectName(QStringLiteral("mainToolBar"));
    toolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    toolBar->setIconSize(QSize(18, 18));

    // 新建连接
    m_newConnectionAction = new QAction(QStringLiteral("新建连接"), this);
    m_newConnectionAction->setShortcut(QKeySequence("Ctrl+N"));
    m_newConnectionAction->setToolTip(QStringLiteral("打开串口/TCP 配置对话框"));
    m_newConnectionAction->setText(QStringLiteral("新建连接"));
    m_newConnectionAction->setIcon(QIcon(QStringLiteral(":/icons/new-connection.svg")));
    connect(m_newConnectionAction, &QAction::triggered, this, &MainWindow::onNewConnection);
    toolBar->addAction(m_newConnectionAction);
    toolBar->addSeparator();

    // 新建电表
    m_newMeterAction = new QAction(QStringLiteral("新建电表"), this);
    m_newMeterAction->setShortcut(QKeySequence("Ctrl+M"));
    m_newMeterAction->setToolTip(QStringLiteral("打开电表配置对话框"));
    m_newMeterAction->setText(QStringLiteral("新建电表"));
    m_newMeterAction->setIcon(QIcon(QStringLiteral(":/icons/new-meter.svg")));
    connect(m_newMeterAction, &QAction::triggered, this, &MainWindow::onNewMeterDialog);
    toolBar->addAction(m_newMeterAction);
    toolBar->addSeparator();

    m_batchNewMeterAction = new QAction(QStringLiteral("批量新建"), this);
    m_batchNewMeterAction->setShortcut(QKeySequence("Ctrl+B"));
    m_batchNewMeterAction->setToolTip(QStringLiteral("在当前连接下批量新建电表"));
    m_batchNewMeterAction->setText(QStringLiteral("批量新建"));
    m_batchNewMeterAction->setIcon(QIcon(QStringLiteral(":/icons/batch-new-meter.svg")));
    connect(m_batchNewMeterAction, &QAction::triggered, this, &MainWindow::onBatchNewMeterDialog);
    toolBar->addAction(m_batchNewMeterAction);
    toolBar->addSeparator();

    // 打开连接
    m_openConnectionAction = new QAction(QStringLiteral("打开连接"), this);
    m_openConnectionAction->setShortcut(QKeySequence("Ctrl+O"));
    m_openConnectionAction->setToolTip(QStringLiteral("激活选中的通信连接"));
    m_openConnectionAction->setText(QStringLiteral("打开连接"));
    m_openConnectionAction->setIcon(QIcon(QStringLiteral(":/icons/open-connection.svg")));
    connect(m_openConnectionAction, &QAction::triggered, this, &MainWindow::onOpenConnection);
    toolBar->addAction(m_openConnectionAction);
    m_openConnectionSeparatorAction = toolBar->addSeparator();

    // 关闭连接
    m_closeConnectionAction = new QAction(QStringLiteral("关闭连接"), this);
    m_closeConnectionAction->setShortcut(QKeySequence("Ctrl+W"));
    m_closeConnectionAction->setToolTip(QStringLiteral("断开当前通信连接"));
    m_closeConnectionAction->setText(QStringLiteral("关闭连接"));
    m_closeConnectionAction->setIcon(QIcon(QStringLiteral(":/icons/close-connection.svg")));
    connect(m_closeConnectionAction, &QAction::triggered, this, &MainWindow::onCloseConnection);
    toolBar->addAction(m_closeConnectionAction);
    m_closeConnectionSeparatorAction = toolBar->addSeparator();

    // 通信交互
    m_commInteractionAction = new QAction(QStringLiteral("通信交互"), this);
    m_commInteractionAction->setShortcut(QKeySequence("Ctrl+I"));
    m_commInteractionAction->setToolTip(QStringLiteral("打开通信调试窗口"));
    m_commInteractionAction->setText(QStringLiteral("通信交互"));
    m_commInteractionAction->setIcon(QIcon(QStringLiteral(":/icons/communication.svg")));
    connect(m_commInteractionAction, &QAction::triggered, this, &MainWindow::onCommunicationInteraction);
    toolBar->addAction(m_commInteractionAction);
    toolBar->addSeparator();

    m_masterQueryAction = new QAction(QStringLiteral("主站查询"), this);
    m_masterQueryAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+I")));
    m_masterQueryAction->setToolTip(QStringLiteral("打开独立主站查询窗口"));
    m_masterQueryAction->setIcon(QIcon(QStringLiteral(":/icons/master-query.svg")));
    connect(m_masterQueryAction, &QAction::triggered, this, &MainWindow::onMasterQueryWindow);
    toolBar->addAction(m_masterQueryAction);
    toolBar->addSeparator();

    m_queryFrameAction = new QAction(QStringLiteral("查询帧生成"), this);
    m_queryFrameAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+G")));
    m_queryFrameAction->setToolTip(QStringLiteral("打开 DLT645 查询帧生成窗口"));
    m_queryFrameAction->setIcon(QIcon(QStringLiteral(":/icons/query-frame.svg")));
    connect(m_queryFrameAction, &QAction::triggered, this, &MainWindow::onQueryFrameGenerator);
    toolBar->addAction(m_queryFrameAction);
    toolBar->addSeparator();

    m_frameParseAction = new QAction(QStringLiteral("解析帧"), this);
    m_frameParseAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+G")));
    m_frameParseAction->setToolTip(QStringLiteral("打开 DLT645 帧解析窗口"));
    m_frameParseAction->setIcon(QIcon(QStringLiteral(":/icons/frame-parse.svg")));
    connect(m_frameParseAction, &QAction::triggered, this, &MainWindow::onFrameParseDialog);
    toolBar->addAction(m_frameParseAction);
}

void MainWindow::setupStatusBar()
{
    QStatusBar *statusBar = this->statusBar();
    m_connectionStatusLabel = new QLabel(m_viewModel->connectionStatusText(), statusBar);
    statusBar->addPermanentWidget(m_connectionStatusLabel);
    m_meterAddressLabel = new QLabel(m_viewModel->meterAddressText(), statusBar);
    statusBar->addPermanentWidget(m_meterAddressLabel, 0);
    updateConnectionStatus(m_viewModel->isConnected());
}
