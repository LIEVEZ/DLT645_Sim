#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "Utils/Logger.h"
#include "Views/OperationLog/OperationLogWidget.h"
#include "Views/DeviceTree/DeviceTreeWidget.h"
#include "Views/DataTable/DataTableWidget.h"
#include "Views/Dialogs/NewConnectionDialog.h"
#include "Views/Dialogs/NewMeterDialog.h"
#include "Views/Dialogs/CommunicationDialog.h"
#include "Views/CommunicationBar/CommunicationBarWidget.h"
#include "Services/MeterDataStore.h"
#include <QSerialPortInfo>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QAction>
#include <QToolBar>
#include <QStatusBar>
#include <QMenuBar>
#include <QMenu>
#include <QSplitter>
#include <QTimer>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QDebug>
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow), m_serialController(new SerialController(this)), m_tcpController(new TcpController(this)), m_protocolController(new ProtocolController(this)), m_dataGenerator(new DataGenerator(this)), m_viewModel(new MainWindowViewModel(this)), m_operationLogWidget(nullptr), m_dataTableWidget(nullptr), m_mainSplitter(nullptr), m_bottomSplitter(nullptr), m_connectionButtonStack(nullptr), m_connectionStatusLabel(nullptr), m_meterAddressLabel(nullptr), m_realTimeBarWidget(nullptr), m_realTimeDataLabel(nullptr), m_realTimeDataTimer(nullptr), m_realTimePhaseIndex(0), m_newConnectionAction(nullptr), m_newMeterAction(nullptr), m_openConnectionAction(nullptr), m_closeConnectionAction(nullptr), m_commInteractionAction(nullptr), m_darkThemeAction(nullptr), m_lightThemeAction(nullptr), m_communicationDialog(nullptr), m_meterDataStore(new MeterDataStore(this)), m_currentMeterModel(nullptr)
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

    // 初始化数据生成器
    // m_dataGenerator->start(1000); // 1秒刷新一次

    // 初始化工具栏按钮状态
    updateConnectionStatus(m_viewModel->isConnected());

    LOG_INFO("DLT645 Simulator started");
}

MainWindow::~MainWindow()
{

    if (m_operationLogWidget)
    {
        m_operationLogWidget->addInfo(QString("日志操作成功? %1").arg(m_viewModel->currentConnectionAlias()));
    }
    closeConnectionInternal(m_viewModel->currentConnectionAlias());
    if (m_operationLogWidget)
    {
        m_operationLogWidget->addInfo(QString("日志操作失败? %1").arg(m_viewModel->currentConnectionAlias()));
    }
    LOG_INFO(QString("Connection closed: %1").arg(m_viewModel->currentConnectionAlias()));
    if (m_serialController && m_serialController->isOpen())
    {
        m_serialController->closePort();
    }
    if (m_tcpController && m_tcpController->isOpen())
    {
        m_tcpController->closeConnection();
    }
    delete ui;
}

void MainWindow::setupUI()
{
    // 设置窗口属性
    setWindowTitle("DLT645 电表模拟器");
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
    m_mainSplitter->setSizes(QList<int>() << 300 << 800 << 300);
    m_mainSplitter->setStretchFactor(0, 1);
    m_mainSplitter->setStretchFactor(1, 3);
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
                updateCommunicationDialogState(); });
    connect(m_viewModel, &MainWindowViewModel::hasMeterChanged,
            m_protocolController, &ProtocolController::setHasMeter);

    // 连接协议控制器和串口控制器
    m_protocolController->setSerialController(m_serialController);
    m_protocolController->setTcpController(m_tcpController);

    // 连接串口信号
    connect(m_serialController, &SerialController::connectionStateChanged,
            this, &MainWindow::onConnectionStateChanged);
    connect(m_tcpController, &TcpController::connectionStateChanged,
            this, &MainWindow::onConnectionStateChanged);

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

    // 连接Logger信号到操作日志
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
        // 将categorySelected信号连接到DataTableWidget
        connect(deviceTree, &DeviceTreeWidget::categorySelected,
                m_dataTableWidget, &DataTableWidget::onCategorySelected);
        connect(deviceTree, &DeviceTreeWidget::openConnectionRequested,
                this, &MainWindow::onOpenConnectionRequested);
        connect(deviceTree, &DeviceTreeWidget::closeConnectionRequested,
                this, &MainWindow::onCloseConnectionRequested);
        connect(deviceTree, &DeviceTreeWidget::deleteConnectionRequested,
                this, &MainWindow::onDeleteConnectionRequested);
        connect(deviceTree, &DeviceTreeWidget::editConnectionRequested,
                this, &MainWindow::onEditConnectionRequested);
        connect(deviceTree, &DeviceTreeWidget::newMeterRequested,
                this, &MainWindow::onNewMeterRequested);
        connect(deviceTree, &DeviceTreeWidget::deleteMeterRequested,
                this, &MainWindow::onDeleteMeterRequested);
    }
}

void MainWindow::setupMenuBar()
{
    QMenuBar *menuBar = this->menuBar();

    // 文件菜单
    QMenu *fileMenu = menuBar->addMenu("文件(&F)");

    QAction *exitAction = fileMenu->addAction("退出(&X)");
    exitAction->setShortcut(QKeySequence("Ctrl+Q"));
    connect(exitAction, &QAction::triggered, this, &QMainWindow::close);

    // 视图菜单
    QMenu *viewMenu = menuBar->addMenu("视图(&V)");

    m_darkThemeAction = viewMenu->addAction("深色主题");
    m_darkThemeAction->setCheckable(true);
    m_darkThemeAction->setChecked(true);
    connect(m_darkThemeAction, &QAction::triggered, this, &MainWindow::onToggleDarkTheme);

    m_lightThemeAction = viewMenu->addAction("浅色主题");
    m_lightThemeAction->setCheckable(true);
    m_lightThemeAction->setChecked(false);
    connect(m_lightThemeAction, &QAction::triggered, this, &MainWindow::onToggleLightTheme);

    // 帮助菜单
    QMenu *helpMenu = menuBar->addMenu("帮助(&H)");

    QAction *aboutAction = helpMenu->addAction("关于(&A)");
    connect(aboutAction, &QAction::triggered, [=]()
            { QMessageBox::about(this, "关于 DLT645 电表模拟器",
                                 "DLT645-2007 电表模拟器\n\n"
                                 "版本: 1.0.0\n\n"
                                 "基于Qt5开发的工业级电表模拟器\n\n"
                                 "支持串口通信、七类数据项、实时数据模拟"); });
}

void MainWindow::setupToolBar()
{
    QToolBar *toolBar = addToolBar("主工具栏");
    toolBar->setMovable(false);
    toolBar->setFloatable(false);
    toolBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

    // 新建连接
    m_newConnectionAction = new QAction("🔗 新建连接", this);
    m_newConnectionAction->setShortcut(QKeySequence("Ctrl+N"));
    m_newConnectionAction->setToolTip("打开串口/TCP配置对话框");
    connect(m_newConnectionAction, &QAction::triggered, this, &MainWindow::onNewConnection);
    toolBar->addAction(m_newConnectionAction);

    // 新建电表
    m_newMeterAction = new QAction("📊 新建电表", this);
    m_newMeterAction->setShortcut(QKeySequence("Ctrl+M"));
    m_newMeterAction->setToolTip("打开电表配置对话框");
    connect(m_newMeterAction, &QAction::triggered, this, &MainWindow::onNewMeterDialog);
    toolBar->addAction(m_newMeterAction);

    toolBar->addSeparator();

    // 打开连接
    m_openConnectionAction = new QAction("▶️ 打开连接", this);
    m_openConnectionAction->setShortcut(QKeySequence("Ctrl+O"));
    m_openConnectionAction->setToolTip("激活选中的通信连接");
    connect(m_openConnectionAction, &QAction::triggered, this, &MainWindow::onOpenConnection);
    toolBar->addAction(m_openConnectionAction);

    // 关闭连接
    m_closeConnectionAction = new QAction("⏹️ 关闭连接", this);
    m_closeConnectionAction->setShortcut(QKeySequence("Ctrl+W"));
    m_closeConnectionAction->setToolTip("断开当前通信连接");
    connect(m_closeConnectionAction, &QAction::triggered, this, &MainWindow::onCloseConnection);
    toolBar->addAction(m_closeConnectionAction);

    toolBar->addSeparator();

    // 通信交互
    m_commInteractionAction = new QAction("💬 通信交互", this);
    m_commInteractionAction->setShortcut(QKeySequence("Ctrl+I"));
    m_commInteractionAction->setToolTip("打开通信调试窗口");
    connect(m_commInteractionAction, &QAction::triggered, this, &MainWindow::onCommunicationInteraction);
    toolBar->addAction(m_commInteractionAction);

    toolBar->addSeparator();
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
void MainWindow::applyDarkTheme()
{
    QString styleSheet = R"(
        QMainWindow {
            background-color: #2B2B2B;
        }
        
        QMenuBar {
            background-color: #3C3F41;
            color: #FFFFFF;
            border-bottom: 1px solid #555555;
        }
        
        QMenuBar::item {
            padding: 5px 15px;
        }
        
        QMenuBar::item:selected {
            background-color: #094771;
        }
        
        QToolBar {
            background-color: #4E5254;
            border-bottom: 1px solid #3C3F41;
            spacing: 10px;
            padding: 5px;
        }
        
        QToolButton {
            background-color: transparent;
            border: 1px solid transparent;
            border-radius: 4px;
            padding: 6px 12px;
            color: #FFFFFF;
            font-size: 10pt;
            min-width: 80px;
        }
        
        QToolButton:hover {
            background-color: #5A5E60;
            border: 1px solid #6A6E70;
        }
        
        QToolButton:pressed {
            background-color: #3C3F41;
        }
        
        QToolButton[popupMode="1"] {
            padding-right: 20px;
        }
        
        QSplitter::handle {
            background-color: #555555;
        }
        
        QStatusBar {
            background-color: #4E5254;
            color: #FFFFFF;
        }
        
        QLabel {
            color: #FFFFFF;
            font-family: "Microsoft YaHei";
        }
        
        QTreeWidget {
            background-color: #2B2B2B;
            color: #FFFFFF;
            border: none;
            outline: none;
        }
        
        QTreeWidget::item {
            padding: 3px;
            border-bottom: 1px solid #3C3F41;
        }
        
        QTreeWidget::item:selected {
            background-color: #094771;
            color: #FFFFFF;
        }
        
        QTreeWidget::item:hover {
            background-color: #3C3F41;
        }
        
        QTableWidget {
            background-color: #2B2B2B;
            color: #FFFFFF;
            border: none;
            gridline-color: #3C3F41;
        }
        
        QTableWidget::item {
            padding: 5px;
            border-bottom: 1px solid #3C3F41;
        }
        
        QTableWidget::item:selected {
            background-color: #094771;
            color: #FFFFFF;
        }
        
        QHeaderView::section {
            background-color: #3C3F41;
            color: #FFFFFF;
            padding: 5px;
            border: none;
            border-bottom: 1px solid #555555;
        }
        
        QLineEdit {
            background-color: #3C3F41;
            color: #FFFFFF;
            border: 1px solid #555555;
            border-radius: 3px;
            padding: 5px;
        }

        QLineEdit#searchBox {
            padding-top: 4px;
            padding-right: 8px;
            padding-bottom: 4px;
        }
        
        QLineEdit:focus {
            border: 1px solid #094771;
        }
        
        QPushButton {
            background-color: #3C3F41;
            color: #FFFFFF;
            border: 1px solid #555555;
            border-radius: 3px;
            padding: 5px 15px;
        }
        
        QPushButton:hover {
            background-color: #4E5254;
        }
        
        QPushButton:pressed {
            background-color: #2B2B2B;
        }
        
        QComboBox {
            background-color: #3C3F41;
            color: #FFFFFF;
            border: 1px solid #555555;
            border-radius: 3px;
            padding: 5px;
        }
        
        QComboBox::drop-down {
            border: none;
        }
        
        QComboBox QAbstractItemView {
            background-color: #3C3F41;
            color: #FFFFFF;
            selection-background-color: #094771;
        }
        
        QListWidget {
            background-color: #2B2B2B;
            color: #FFFFFF;
            border: none;
            outline: none;
        }
        
        QListWidget::item {
            padding: 3px;
            border-bottom: 1px solid #3C3F41;
        }
        
        QListWidget::item:selected {
            background-color: #094771;
            color: #FFFFFF;
        }
        
        QListWidget::item:hover {
            background-color: #3C3F41;
        }
        
        QCheckBox {
            color: #FFFFFF;
            spacing: 4px;
        }
        
        QCheckBox::indicator {
            width: 16px;
            height: 16px;
            border: 1px solid #555555;
            border-radius: 3px;
            background-color: #3C3F41;
        }
        
        QCheckBox::indicator:checked {
            background-color: #094771;
            border: 1px solid #094771;
        }
        
        QSpinBox {
            background-color: #3C3F41;
            color: #FFFFFF;
            border: 1px solid #555555;
            border-radius: 3px;
            padding: 3px;
        }
        
        QSpinBox:focus {
            border: 1px solid #094771;
        }
        
        QSpinBox::up-button, QSpinBox::down-button {
            background-color: #4E5254;
            border: none;
            width: 16px;
        }
        
        QSpinBox::up-button:hover, QSpinBox::down-button:hover {
            background-color: #5A5E60;
        }
        
        QGroupBox {
            border: 1px solid #555555;
            border-radius: 5px;
            margin-top: 10px;
            padding-top: 10px;
            font-weight: bold;
            color: #FFFFFF;
        }
        
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 5px;
            color: #4EC9B0;
        }
        
        QTabWidget::pane {
            border: 1px solid #555555;
            background-color: #2B2B2B;
        }
        
        QTabBar::tab {
            background-color: #3C3F41;
            color: #D4D4D4;
            padding: 8px 20px;
            border-top-left-radius: 3px;
            border-top-right-radius: 3px;
        }
        
        QTabBar::tab:selected {
            background-color: #094771;
            color: #FFFFFF;
        }
        
        QDialog {
            background-color: #2B2B2B;
        }
    )";

    setStyleSheet(styleSheet);

    // 更新容器背景色
    QWidget *commBar = findChild<QWidget *>("communicationBarWidget");
    if (commBar)
        commBar->setStyleSheet("background-color: #4E5254;");

    QWidget *leftCont = findChild<QWidget *>("leftContainer");
    if (leftCont)
        leftCont->setStyleSheet("background-color: #2B2B2B;");

    QWidget *centerCont = findChild<QWidget *>("centerContainer");
    if (centerCont)
        centerCont->setStyleSheet("background-color: #2B2B2B;");

    QWidget *rightCont = findChild<QWidget *>("rightContainer");
    if (rightCont)
        rightCont->setStyleSheet("background-color: #2B2B2B;");
}

void MainWindow::applyLightTheme()
{
    QString styleSheet = R"(
        QMainWindow {
            background-color: #FFFFFF;
        }
        
        QMenuBar {
            background-color: #F0F0F0;
            color: #000000;
            border-bottom: 1px solid #CCCCCC;
        }
        
        QMenuBar::item {
            padding: 5px 15px;
        }
        
        QMenuBar::item:selected {
            background-color: #E0E0E0;
        }
        
        QToolBar {
            background-color: #F5F5F5;
            border-bottom: 1px solid #E0E0E0;
            spacing: 10px;
            padding: 5px;
        }
        
        QToolButton {
            background-color: transparent;
            border: 1px solid transparent;
            border-radius: 4px;
            padding: 6px 12px;
            color: #000000;
            font-size: 10pt;
            min-width: 80px;
        }
        
        QToolButton:hover {
            background-color: #E8E8E8;
            border: 1px solid #D0D0D0;
        }
        
        QToolButton:pressed {
            background-color: #D0D0D0;
        }
        
        QToolButton[popupMode="1"] {
            padding-right: 20px;
        }
        
        QSplitter::handle {
            background-color: #CCCCCC;
        }
        
        QStatusBar {
            background-color: #F5F5F5;
            color: #000000;
        }
        
        QLabel {
            color: #000000;
            font-family: "Microsoft YaHei";
        }
        
        QTreeWidget {
            background-color: #FFFFFF;
            color: #000000;
            border: none;
            outline: none;
        }
        
        QTreeWidget::item {
            padding: 3px;
            border-bottom: 1px solid #E0E0E0;
        }
        
        QTreeWidget::item:selected {
            background-color: #0078D7;
            color: #FFFFFF;
        }
        
        QTreeWidget::item:hover {
            background-color: #F0F0F0;
        }
        
        QTableWidget {
            background-color: #FFFFFF;
            color: #000000;
            border: none;
            gridline-color: #E0E0E0;
        }
        
        QTableWidget::item {
            padding: 5px;
            border-bottom: 1px solid #E0E0E0;
        }
        
        QTableWidget::item:selected {
            background-color: #0078D7;
            color: #FFFFFF;
        }
        
        QHeaderView::section {
            background-color: #F0F0F0;
            color: #000000;
            padding: 5px;
            border: none;
            border-bottom: 1px solid #CCCCCC;
        }
        
        QLineEdit {
            background-color: #FFFFFF;
            color: #000000;
            border: 1px solid #CCCCCC;
            border-radius: 3px;
            padding: 5px;
        }

        QLineEdit#searchBox {
            padding-top: 4px;
            padding-right: 8px;
            padding-bottom: 4px;
        }
        
        QLineEdit:focus {
            border: 1px solid #0078D7;
        }
        
        QPushButton {
            background-color: #F0F0F0;
            color: #000000;
            border: 1px solid #CCCCCC;
            border-radius: 3px;
            padding: 5px 15px;
        }
        
        QPushButton:hover {
            background-color: #E0E0E0;
        }
        
        QPushButton:pressed {
            background-color: #D0D0D0;
        }
        
        QComboBox {
            background-color: #FFFFFF;
            color: #000000;
            border: 1px solid #CCCCCC;
            border-radius: 3px;
            padding: 5px;
        }
        
        QComboBox::drop-down {
            border: none;
        }
        
        QComboBox QAbstractItemView {
            background-color: #FFFFFF;
            color: #000000;
            selection-background-color: #0078D7;
        }
        
        QListWidget {
            background-color: #FFFFFF;
            color: #000000;
            border: none;
            outline: none;
        }
        
        QListWidget::item {
            padding: 3px;
            border-bottom: 1px solid #E0E0E0;
        }
        
        QListWidget::item:selected {
            background-color: #0078D7;
            color: #FFFFFF;
        }
        
        QListWidget::item:hover {
            background-color: #F0F0F0;
        }
        
        QCheckBox {
            color: #000000;
            spacing: 4px;
        }
        
        QCheckBox::indicator {
            width: 16px;
            height: 16px;
            border: 1px solid #CCCCCC;
            border-radius: 3px;
            background-color: #FFFFFF;
        }
        
        QCheckBox::indicator:checked {
            background-color: #0078D7;
            border: 1px solid #0078D7;
        }
        
        QSpinBox {
            background-color: #FFFFFF;
            color: #000000;
            border: 1px solid #CCCCCC;
            border-radius: 3px;
            padding: 3px;
        }
        
        QSpinBox:focus {
            border: 1px solid #0078D7;
        }
        
        QSpinBox::up-button, QSpinBox::down-button {
            background-color: #F0F0F0;
            border: none;
            width: 16px;
        }
        
        QSpinBox::up-button:hover, QSpinBox::down-button:hover {
            background-color: #E0E0E0;
        }
        
        QGroupBox {
            border: 1px solid #CCCCCC;
            border-radius: 5px;
            margin-top: 10px;
            padding-top: 10px;
            font-weight: bold;
            color: #000000;
        }
        
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 5px;
            color: #0078D7;
        }
        
        QTabWidget::pane {
            border: 1px solid #CCCCCC;
            background-color: #FFFFFF;
        }
        
        QTabBar::tab {
            background-color: #F0F0F0;
            color: #000000;
            padding: 8px 20px;
            border-top-left-radius: 3px;
            border-top-right-radius: 3px;
        }
        
        QTabBar::tab:selected {
            background-color: #0078D7;
            color: #FFFFFF;
        }
        
        QDialog {
            background-color: #FFFFFF;
        }
    )";

    setStyleSheet(styleSheet);

    // 更新容器背景色
    QWidget *commBar = findChild<QWidget *>("communicationBarWidget");
    if (commBar)
        commBar->setStyleSheet("background-color: #F5F5F5;");

    QWidget *leftCont = findChild<QWidget *>("leftContainer");
    if (leftCont)
        leftCont->setStyleSheet("background-color: #FFFFFF;");

    QWidget *centerCont = findChild<QWidget *>("centerContainer");
    if (centerCont)
        centerCont->setStyleSheet("background-color: #FFFFFF;");

    QWidget *rightCont = findChild<QWidget *>("rightContainer");
    if (rightCont)
        rightCont->setStyleSheet("background-color: #FFFFFF;");
}

void MainWindow::onToggleDarkTheme(bool enabled)
{
    if (enabled)
    {
        m_viewModel->setDarkTheme(true);
        m_darkThemeAction->setChecked(true);
        m_lightThemeAction->setChecked(false);
        applyDarkTheme();

        // 更新通信交互窗口主题
        if (m_communicationDialog)
        {
            m_communicationDialog->applyDarkTheme();
        }
    }
}

void MainWindow::onToggleLightTheme(bool enabled)
{
    if (enabled)
    {
        m_viewModel->setDarkTheme(false);
        m_darkThemeAction->setChecked(false);
        m_lightThemeAction->setChecked(true);
        applyLightTheme();

        // 更新通信交互窗口主题
        if (m_communicationDialog)
        {
            m_communicationDialog->applyLightTheme();
        }
    }
}

void MainWindow::onNewConnection()
{
    NewConnectionDialog dialog(this);
    DeviceTreeWidget *deviceTree = deviceTreeWidget();
    if (deviceTree)
    {
        QStringList connectedPorts;
        // 遍历所有连接节点，收集已连接的端口
        for (int i = 0; i < deviceTree->getTreeWidget()->topLevelItemCount(); ++i)
        {
            QTreeWidgetItem *item = deviceTree->getTreeWidget()->topLevelItem(i);
            if (item)
            {
                const QString port = item->data(0, Qt::UserRole + 3).toString();
                const bool isConnected = item->data(0, Qt::UserRole + 2).toBool();
                if (isConnected)
                {
                    connectedPorts.append(port);
                }
            }
        }
        dialog.setConnectedPorts(connectedPorts);
    }
    if (dialog.exec() == QDialog::Accepted)
    {
        const ConnectionConfig config = dialog.getConnectionConfig();
        openConnectionInternal(config.alias, config, true);
    }
}
void MainWindow::onNewMeterDialog()
{
    // 检查是否选中了连接节点
    DeviceTreeWidget *deviceTree = deviceTreeWidget();
    if (!deviceTree || !deviceTree->canAddMeter())
    {
        QMessageBox::warning(this, "提示", "请先选择一个连接节点");
        return;
    }

    QString connectionAlias = deviceTree->getSelectedConnectionAlias();
    if (connectionAlias.isEmpty())
    {
        QMessageBox::warning(this, "提示", "请先选择一个连接节点");
        return;
    }

    NewMeterDialog dialog(this);

    // 设置当前电表配置
    MeterConfig currentConfig;
    dialog.setMeterConfig(currentConfig);

    if (dialog.exec() == QDialog::Accepted)
    {
        MeterConfig config = dialog.getMeterConfig();

        // 创建 MeterInfo 结构体
        MeterInfo meterInfo;
        meterInfo.alias = config.alias;
        meterInfo.address = config.address;
        meterInfo.operatorCode = config.operatorCode;
        meterInfo.password = config.password;
        meterInfo.includeLeadingFE = config.includePrefixFE;

        // 添加电表节点到设备树（第2层级）
        deviceTree->addMeter(connectionAlias, meterInfo);

        refreshMeterPresence();
        m_viewModel->setCurrentMeterAddress(config.address);

        // 更新状态栏显示
        QStatusBar *statusBar = this->statusBar();
        QList<QLabel *> labels = statusBar->findChildren<QLabel *>();
        if (labels.size() >= 2)
        {
            labels[1]->setText("地址: " + config.address);
        }

        LOG_INFO(QString("Meter added: Alias=%1, Address=%2, OperatorCode=%3, IncludePrefixFE=%4")
                     .arg(config.alias)
                     .arg(config.address)
                     .arg(config.operatorCode)
                     .arg(config.includePrefixFE ? "true" : "false"));
    }
}

void MainWindow::onOpenConnection()
{
    if (m_viewModel->isConnected())
    {
        QMessageBox::information(this, "提示", "已经有连接打开，请先关闭现有连接");
        return;
    }
    if (m_viewModel->currentConnectionAlias().isEmpty())
    {
        QMessageBox::warning(this, "提示", "请先选择一个连接");
        return;
    }
    if (!m_viewModel->hasConnectionConfig(m_viewModel->currentConnectionAlias()))
    {
        QMessageBox::warning(this, "错误", "未找到连接配置");
        return;
    }
    const ConnectionConfig config = m_viewModel->connectionConfig(m_viewModel->currentConnectionAlias());
    openConnectionInternal(m_viewModel->currentConnectionAlias(), config, false);
}
void MainWindow::onCloseConnection()
{
    if (!m_viewModel->isConnected())
    {
        QMessageBox::information(this, "提示", "连接已经关闭");
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

void MainWindow::onDataUpdated()
{
    // 数据更新时刷新UI
    updateRealTimeDataDisplay(false);
}

void MainWindow::onRealTimeDataRotationTimeout()
{
    updateRealTimeDataDisplay(true);
}

void MainWindow::onConnectionStateChanged(bool connected)
{
    m_viewModel->setConnected(connected);
    updateConnectionStatus(connected);

    // 更新设备树中当前连接节点的状态
    if (!m_viewModel->currentConnectionAlias().isEmpty())
    {
        DeviceTreeWidget *deviceTree = deviceTreeWidget();
        if (deviceTree)
        {
            deviceTree->setConnectionStatus(m_viewModel->currentConnectionAlias(), connected);
        }
    }

    if (connected)
    {
        LOG_INFO("Connection established");
    }
    else
    {
        LOG_INFO("Connection closed");
    }
}

void MainWindow::onProtocolFrameReceived(const ProtocolFrame &frame)
{
    // LOG_INFO(QString("Frame received: %1").arg(frame.toString()));

    // 添加到操作日志
    if (m_operationLogWidget)
    {
        m_operationLogWidget->addCommunication(QString("接收帧: %1").arg(frame.toString()));
    }
}

void MainWindow::onProtocolFrameSent(const ProtocolFrame &frame)
{
    // LOG_INFO(QString("===Frame sent: %1").arg(frame.toString()));

    // 添加到操作日志
    if (m_operationLogWidget)
    {
        m_operationLogWidget->addCommunication(QString("发送帧: %1").arg(frame.toString()));
    }
}

void MainWindow::onRawDataReceived(const QByteArray &data)
{
    // 将原始数据（包含前导码）转换为CommunicationMessage并添加到通信交互窗口
    if (m_communicationDialog)
    {
        CommunicationMessage message;
        message.timestamp = QDateTime::currentDateTime();
        message.direction = MessageDirection::Receive;
        message.type = MessageType::Request;
        message.fullFrame = data; // 使用原始数据，包含前导码
        message.checksumValid = true;

        // 从原始数据中解析控制码和数据长度（跳过前导码）
        int startIndex = 0;
        while (startIndex < data.size() && static_cast<quint8>(data[startIndex]) == 0xFE)
        {
            startIndex++;
        }
        if (startIndex + 9 < data.size())
        {
            message.controlCode = static_cast<quint8>(data[startIndex + 8]);
            message.dataLength = static_cast<quint8>(data[startIndex + 9]);
        }

        m_communicationDialog->addMessage(message);
    }
}

void MainWindow::onRawDataSent(const QByteArray &data)
{
    // 将原始数据（包含前导码）转换为CommunicationMessage并添加到通信交互窗口
    if (m_communicationDialog)
    {
        CommunicationMessage message;
        message.timestamp = QDateTime::currentDateTime();
        message.direction = MessageDirection::Transmit;
        message.type = MessageType::Response;
        message.fullFrame = data;
        message.checksumValid = true;

        // 从原始数据中解析控制码和数据长度（跳过前导码）
        int startIndex = 0;
        while (startIndex < data.size() && static_cast<quint8>(data[startIndex]) == 0xFE)
        {
            startIndex++;
        }
        if (startIndex + 9 < data.size())
        {
            message.controlCode = static_cast<quint8>(data[startIndex + 8]);
            message.dataLength = static_cast<quint8>(data[startIndex + 9]);
        }

        m_communicationDialog->addMessage(message);
    }
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
    if (m_viewModel->isConnected())
    {
        m_openConnectionAction->setVisible(false);
        m_closeConnectionAction->setVisible(true);
    }
    else
    {
        m_openConnectionAction->setVisible(true);
        m_closeConnectionAction->setVisible(false);
    }

    // 更新建电表按钮状态（未选中连接时禁用）
    m_newMeterAction->setEnabled(m_viewModel->isNewMeterEnabled());

    // 更新通信交互按钮状态（连接已打开时启用）
    m_commInteractionAction->setEnabled(m_viewModel->isCommunicationInteractionEnabled());
}

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

DeviceTreeWidget *MainWindow::deviceTreeWidget() const
{
    return findChild<DeviceTreeWidget *>();
}

QString MainWindow::connectionEndpoint(const ConnectionConfig &config) const
{
    if (config.type == ConnectionType::Serial)
    {
        return config.serialPort;
    }
    return QString("%1:%2").arg(config.tcpAddress).arg(config.tcpPort);
}
ConnectionInfo MainWindow::buildConnectionInfo(const ConnectionConfig &config, bool connected) const
{
    ConnectionInfo connectionInfo;
    connectionInfo.alias = config.alias;
    connectionInfo.type = (config.type == ConnectionType::Serial) ? QStringLiteral("Serial") : QStringLiteral("TCP");
    connectionInfo.port = connectionEndpoint(config);
    connectionInfo.isConnected = connected;
    connectionInfo.baudRate = config.baudRate;
    connectionInfo.dataBits = config.dataBits;
    connectionInfo.stopBits = config.stopBits;
    connectionInfo.parity = config.parity;
    connectionInfo.flowControl = config.flowControl;
    connectionInfo.tcpAddress = config.tcpAddress;
    connectionInfo.tcpPort = config.tcpPort;
    return connectionInfo;
}
bool MainWindow::openConnectionInternal(const QString &connectionAlias, const ConnectionConfig &config, bool addConnectionNode)
{
    DeviceTreeWidget *deviceTree = deviceTreeWidget();
    const QString alias = connectionAlias.isEmpty() ? config.alias : connectionAlias;
    const QString endpoint = connectionEndpoint(config);
    if (deviceTree && deviceTree->isPortConnected(endpoint))
    {
        QMessageBox::warning(this, "Error", QString("Connection %1 is already in use").arg(endpoint));
        return false;
    }
    bool opened = false;
    if (config.type == ConnectionType::Serial)
    {
        if (!m_serialController)
        {
            QMessageBox::warning(this, "Warning", "Serial controller is not initialized");
            return false;
        }
        SerialConfig serialConfig;
        serialConfig.portName = config.serialPort;
        serialConfig.baudRate = config.baudRate;
        serialConfig.dataBits = static_cast<QSerialPort::DataBits>(config.dataBits);
        serialConfig.stopBits = static_cast<QSerialPort::StopBits>(config.stopBits);
        serialConfig.parity = static_cast<QSerialPort::Parity>(config.parity);
        serialConfig.flowControl = static_cast<QSerialPort::FlowControl>(config.flowControl);
        opened = m_serialController->openPort(serialConfig);
    }
    else
    {
        if (!m_tcpController)
        {
            QMessageBox::warning(this, "Warning", "TCP controller is not initialized");
            return false;
        }
        TcpConfig tcpConfig;
        tcpConfig.hostAddress = config.tcpAddress;
        tcpConfig.port = static_cast<quint16>(config.tcpPort);
        opened = m_tcpController->openConnection(tcpConfig);
    }
    if (!opened)
    {
        const QString errorText = (config.type == ConnectionType::Serial)
                                      ? QString("Failed to open serial port: %1").arg(endpoint)
                                      : QString("Failed to open TCP connection: %1").arg(endpoint);
        QMessageBox::critical(this, "Error", errorText);
        if (m_operationLogWidget)
        {
            m_operationLogWidget->addError(errorText);
        }
        return false;
    }
    if (deviceTree)
    {
        if (addConnectionNode)
        {
            deviceTree->addConnection(buildConnectionInfo(config, true));
        }
        else
        {
            deviceTree->setConnectionStatus(alias, true);
        }
    }
    m_viewModel->upsertConnectionConfig(config);
    m_viewModel->setCurrentConnectionAlias(alias);
    m_viewModel->setConnectionSelected(true);
    m_viewModel->setConnected(true);
    updateCommunicationDialogState();
    if (m_communicationDialog)
    {
        m_communicationDialog->clearAllMessages();
    }
    if (m_operationLogWidget)
    {
        m_operationLogWidget->addSuccess(QString("Connection opened: %1 (%2)")
                                             .arg(alias)
                                             .arg(endpoint));
    }
    LOG_INFO(QString("Connection opened: %1, Endpoint: %2")
                 .arg(alias)
                 .arg(endpoint));
    return true;
}
void MainWindow::closeConnectionInternal(const QString &connectionAlias)
{
    const QString alias = connectionAlias.isEmpty() ? m_viewModel->currentConnectionAlias() : connectionAlias;
    if (m_serialController && m_serialController->isOpen())
    {
        m_serialController->closePort();
    }
    if (m_tcpController && m_tcpController->isOpen())
    {
        m_tcpController->closeConnection();
    }
    m_viewModel->setCurrentConnectionAlias(alias);
    m_viewModel->setConnectionSelected(!alias.isEmpty());
    m_viewModel->setConnected(false);
    DeviceTreeWidget *deviceTree = deviceTreeWidget();
    if (deviceTree && !alias.isEmpty())
    {
        deviceTree->setConnectionStatus(alias, false);
    }
    updateToolBarButtons();
    updateCommunicationDialogState();
}
QStringList MainWindow::collectMeterAddresses() const
{
    QStringList addresses;
    DeviceTreeWidget *deviceTree = deviceTreeWidget();
    if (!deviceTree)
    {
        return addresses;
    }

    QTreeWidget *tree = deviceTree->getTreeWidget();
    for (int i = 0; i < tree->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem *connectionItem = tree->topLevelItem(i);
        if (!connectionItem)
        {
            continue;
        }

        for (int j = 0; j < connectionItem->childCount(); ++j)
        {
            QTreeWidgetItem *meterItem = connectionItem->child(j);
            if (!meterItem)
            {
                continue;
            }

            const QString address = meterItem->data(0, Qt::UserRole + 2).toString();
            if (!address.isEmpty())
            {
                addresses.append(address);
            }
        }
    }

    addresses.removeDuplicates();
    return addresses;
}

QStringList MainWindow::collectMeterAddressesForConnection(const QString &connectionAlias) const
{
    QStringList addresses;
    DeviceTreeWidget *deviceTree = deviceTreeWidget();
    if (!deviceTree)
    {
        return addresses;
    }

    QTreeWidget *tree = deviceTree->getTreeWidget();
    for (int i = 0; i < tree->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem *connectionItem = tree->topLevelItem(i);
        if (!connectionItem || connectionItem->data(0, Qt::UserRole).toString() != connectionAlias)
        {
            continue;
        }

        for (int j = 0; j < connectionItem->childCount(); ++j)
        {
            QTreeWidgetItem *meterItem = connectionItem->child(j);
            if (!meterItem)
            {
                continue;
            }

            const QString address = meterItem->data(0, Qt::UserRole + 2).toString();
            if (!address.isEmpty())
            {
                addresses.append(address);
            }
        }
        break;
    }

    addresses.removeDuplicates();
    return addresses;
}

MeterInfo MainWindow::findMeterInfo(const QString &connectionAlias, const QString &meterAlias) const
{
    MeterInfo meterInfo;
    DeviceTreeWidget *deviceTree = deviceTreeWidget();
    if (!deviceTree)
    {
        return meterInfo;
    }

    QTreeWidget *tree = deviceTree->getTreeWidget();
    for (int i = 0; i < tree->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem *connectionItem = tree->topLevelItem(i);
        if (!connectionItem || connectionItem->data(0, Qt::UserRole).toString() != connectionAlias)
        {
            continue;
        }

        for (int j = 0; j < connectionItem->childCount(); ++j)
        {
            QTreeWidgetItem *meterItem = connectionItem->child(j);
            if (!meterItem || meterItem->data(0, Qt::UserRole).toString() != meterAlias)
            {
                continue;
            }

            meterInfo.alias = meterItem->data(0, Qt::UserRole).toString();
            meterInfo.address = meterItem->data(0, Qt::UserRole + 2).toString();
            meterInfo.operatorCode = meterItem->data(0, Qt::UserRole + 3).toString();
            meterInfo.password = meterItem->data(0, Qt::UserRole + 4).toString();
            meterInfo.includeLeadingFE = meterItem->data(0, Qt::UserRole + 5).toBool();
            return meterInfo;
        }
    }

    return meterInfo;
}

void MainWindow::attachMeterPersistence(MeterModel *meterModel)
{
    if (!meterModel || !m_meterDataStore || !m_meterDataStore->isReady())
    {
        return;
    }

    connect(meterModel, &MeterModel::dataChanged, this, [this, meterModel](const QString &diCode, const QVariant &value)
            {
                if (m_meterDataStore->saveMeterData(meterModel))
                {
                    LOG_INFO(QString("保存当前数据: 电表地址=%1, DI=%2, 值=%3")
                                 .arg(meterModel->getMeterAddress(), diCode, value.toString()));
                } });
    connect(meterModel, &MeterModel::dataBatchChanged, this, [this, meterModel]()
            {
                if (m_meterDataStore->saveMeterData(meterModel))
                {
                    LOG_INFO(QString("当前电表数据已批量保存到数据库: 电表地址=%1")
                                 .arg(meterModel->getMeterAddress()));
                } });
    connect(meterModel, &MeterModel::addressChanged, this, [this, meterModel](const QString &newAddress)
            {
                if (m_meterDataStore->saveMeterData(meterModel))
                {
                    LOG_INFO(QString("电表地址更改后已保存到数据库: 电表地址=%1")
                                 .arg(newAddress));
                } });
}

MeterModel *MainWindow::createMeterModel(const MeterInfo &meterInfo)
{
    if (meterInfo.address.isEmpty())
    {
        return nullptr;
    }

    MeterModel *meterModel = m_meterModelsByAddress.value(meterInfo.address, nullptr);
    if (!meterModel)
    {
        meterModel = new MeterModel(this);
        meterModel->setMeterAddress(meterInfo.address);
        meterModel->setOperatorCode(meterInfo.operatorCode);
        meterModel->setPassword(meterInfo.password);
        meterModel->setIncludePrefixFE(meterInfo.includeLeadingFE);

        if (m_meterDataStore && m_meterDataStore->isReady())
        {
            if (!m_meterDataStore->loadMeterData(meterModel))
            {
                m_meterDataStore->saveMeterData(meterModel);
            }
        }

        attachMeterPersistence(meterModel);
        m_meterModelsByAddress.insert(meterInfo.address, meterModel);
    }

    meterModel->setMeterAddress(meterInfo.address);
    meterModel->setOperatorCode(meterInfo.operatorCode);
    meterModel->setPassword(meterInfo.password);
    meterModel->setIncludePrefixFE(meterInfo.includeLeadingFE);

    m_protocolController->registerMeterModel(meterInfo.address, meterModel);
    return meterModel;
}

void MainWindow::applyCurrentMeter(const MeterInfo &meterInfo)
{
    MeterModel *meterModel = createMeterModel(meterInfo);
    m_currentMeterModel = meterModel;
    m_realTimePhaseIndex = 0;

    if (m_currentMeterModel)
    {
        m_protocolController->setMeterModel(m_currentMeterModel);
    }

    if (m_dataTableWidget)
    {
        m_dataTableWidget->setMeterModel(m_currentMeterModel);
        m_dataTableWidget->setMeterInfo(meterInfo.alias, meterInfo.address);
    }

    if (m_communicationDialog)
    {
        m_communicationDialog->setMeterModel(m_currentMeterModel);
    }

    m_viewModel->setCurrentMeterAddress(meterInfo.address);
    updateConnectionStatus(m_viewModel->isConnected());
    updateRealTimeDataDisplay(false);
}

void MainWindow::clearCurrentMeter()
{
    m_currentMeterModel = nullptr;
    m_realTimePhaseIndex = 0;
    m_protocolController->setMeterModel(nullptr);

    if (m_dataTableWidget)
    {
        m_dataTableWidget->setMeterModel(nullptr);
        m_dataTableWidget->setMeterInfo(QString(), QString());
    }

    if (m_communicationDialog)
    {
        m_communicationDialog->setMeterModel(nullptr);
    }

    m_viewModel->setCurrentMeterAddress(QString());
    updateConnectionStatus(m_viewModel->isConnected());
    updateRealTimeDataDisplay(false);
}

void MainWindow::refreshMeterPresence()
{
    const QStringList meterAddresses = collectMeterAddresses();
    const bool hasMeter = !meterAddresses.isEmpty();

    m_protocolController->setKnownMeterAddresses(meterAddresses);
    m_viewModel->setHasMeter(hasMeter);
    if (!hasMeter)
    {
        clearCurrentMeter();
    }
    else if (!m_viewModel->currentMeterAddress().isEmpty() &&
             !meterAddresses.contains(m_viewModel->currentMeterAddress()))
    {
        clearCurrentMeter();
    }
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

    constexpr int phaseCount = static_cast<int>(sizeof(phaseInfos) / sizeof(phaseInfos[0]));
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

    m_realTimeDataLabel->setText(QString("正向有功总电能: %1 | 反向有功总电能: %2 | %3电压: %4 | %3电流: %5 | %3相角: %6")
                                     .arg(forwardEnergy, 0, 'f', 2)
                                     .arg(reverseEnergy, 0, 'f', 2)
                                     .arg(QString::fromUtf8(phaseInfos[m_realTimePhaseIndex].name))
                                     .arg(phaseVoltage, 0, 'f', 1)
                                     .arg(phaseCurrent, 0, 'f', 3)
                                     .arg(phaseAngle, 0, 'f', 1));
}

void MainWindow::onCommunicationInteraction()
{
    // 如果窗口已存在，则显示并激活
    if (m_communicationDialog)
    {
        m_communicationDialog->show();
        m_communicationDialog->raise();
        m_communicationDialog->activateWindow();
        return;
    }

    //
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

void MainWindow::onLoggerLogEntryAdded(LogLevel level, const QDateTime &timestamp, const QString &message)
{
    if (!m_operationLogWidget)
    {
        return;
    }

    // 根据日志级别添加到操作日志
    switch (level)
    {
    case LogLevel::Debug:
        // 调试日志不显示在操作日志中
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

void MainWindow::onConnectionSelected(const QString &connectionAlias)
{
    m_viewModel->setCurrentConnectionAlias(connectionAlias);
    m_viewModel->setConnectionSelected(true);
    clearCurrentMeter();
}

void MainWindow::onMeterSelected(const QString &meterAlias)
{
    Q_UNUSED(meterAlias);
    // TODO: 处理电表选中事件
}

void MainWindow::onMeterInfoSelected(const MeterInfo &meterInfo)
{
    if (meterInfo.address.isEmpty())
    {
        clearCurrentMeter();
        return;
    }

    applyCurrentMeter(meterInfo);
}

void MainWindow::onOpenConnectionRequested(const QString &connectionAlias)
{
    if (m_viewModel->isConnected())
    {
        QMessageBox::information(this, "提示", "已经有连接打开，请先关闭现有连接");
        return;
    }
    if (!m_viewModel->hasConnectionConfig(connectionAlias))
    {
        QMessageBox::warning(this, "错误", "未找到连接配置");
        return;
    }
    const ConnectionConfig config = m_viewModel->connectionConfig(connectionAlias);
    openConnectionInternal(connectionAlias, config, false);
}

void MainWindow::onCloseConnectionRequested(const QString &connectionAlias)
{
    if (!m_viewModel->isConnected())
    {
        QMessageBox::information(this, "提示", "连接已经关闭");
        return;
    }
    closeConnectionInternal(connectionAlias);
}

void MainWindow::onDeleteConnectionRequested(const QString &connectionAlias)
{
    QMessageBox::StandardButton reply = QMessageBox::question(this, "确认删除",
                                                              QString("确定要删除连接 \"%1\" 吗？").arg(connectionAlias),
                                                              QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes)
    {
        const QStringList connectionMeterAddresses = collectMeterAddressesForConnection(connectionAlias);

        if (m_viewModel->isConnected())
        {
            closeConnectionInternal(connectionAlias);
        }

        // 从设备树中删除连接节点
        DeviceTreeWidget *deviceTree = deviceTreeWidget();
        if (deviceTree)
        {
            deviceTree->deleteConnection(connectionAlias);
        }

        for (const QString &address : connectionMeterAddresses)
        {
            MeterModel *meterModel = m_meterModelsByAddress.take(address);
            m_protocolController->unregisterMeterModel(address);
            if (meterModel)
            {
                meterModel->deleteLater();
            }
        }

        m_viewModel->removeConnectionConfig(connectionAlias);
        refreshMeterPresence();

        LOG_INFO(QString("Connection deleted: %1").arg(connectionAlias));
        QMessageBox::information(this, "成功", "连接已删除");
    }
}

void MainWindow::onEditConnectionRequested(const QString &connectionAlias)
{
    if (!m_viewModel->hasConnectionConfig(connectionAlias))
    {
        QMessageBox::warning(this, "错误", "未找到连接配置");
        return;
    }

    if (m_viewModel->isConnected() && m_viewModel->currentConnectionAlias() == connectionAlias)
    {
        QMessageBox::information(this, "提示", "请先关闭当前连接，再编辑连接配置");
        return;
    }

    DeviceTreeWidget *deviceTree = deviceTreeWidget();
    ConnectionConfig originalConfig = m_viewModel->connectionConfig(connectionAlias);

    NewConnectionDialog dialog(this);
    dialog.setEditMode(true);
    dialog.setConnectionConfig(originalConfig);

    if (deviceTree)
    {
        QStringList connectedPorts;
        for (int i = 0; i < deviceTree->getTreeWidget()->topLevelItemCount(); ++i)
        {
            QTreeWidgetItem *item = deviceTree->getTreeWidget()->topLevelItem(i);
            if (!item)
            {
                continue;
            }

            const ItemType itemType = static_cast<ItemType>(item->data(0, Qt::UserRole + 1).toInt());
            if (itemType != ItemType::Connection)
            {
                continue;
            }

            const QString alias = item->data(0, Qt::UserRole).toString();
            const bool isConnected = item->data(0, Qt::UserRole + 2).toBool();
            if (isConnected && alias != connectionAlias)
            {
                connectedPorts.append(item->data(0, Qt::UserRole + 3).toString());
            }
        }
        dialog.setConnectedPorts(connectedPorts);
        dialog.setConnectionConfig(originalConfig);
    }

    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    const ConnectionConfig updatedConfig = dialog.getConnectionConfig();
    if (updatedConfig.alias != connectionAlias && m_viewModel->hasConnectionConfig(updatedConfig.alias))
    {
        QMessageBox::warning(this, "错误", QString("连接别名 %1 已存在").arg(updatedConfig.alias));
        return;
    }

    if (updatedConfig.type == ConnectionType::Serial && deviceTree && deviceTree->isPortConnected(updatedConfig.serialPort))
    {
        QMessageBox::warning(this, "错误", QString("串口 %1 已被连接，请使用其他串口").arg(updatedConfig.serialPort));
        return;
    }

    if (updatedConfig.alias != connectionAlias)
    {
        m_viewModel->removeConnectionConfig(connectionAlias);
    }
    m_viewModel->upsertConnectionConfig(updatedConfig);

    if (deviceTree)
    {
        ConnectionInfo connectionInfo;
        connectionInfo.alias = updatedConfig.alias;
        connectionInfo.type = (updatedConfig.type == ConnectionType::Serial) ? "串口" : "TCP";
        connectionInfo.port = (updatedConfig.type == ConnectionType::Serial)
                                  ? updatedConfig.serialPort
                                  : QString("%1:%2").arg(updatedConfig.tcpAddress).arg(updatedConfig.tcpPort);
        connectionInfo.isConnected = false;
        connectionInfo.baudRate = updatedConfig.baudRate;
        connectionInfo.dataBits = updatedConfig.dataBits;
        connectionInfo.stopBits = updatedConfig.stopBits;
        connectionInfo.parity = updatedConfig.parity;
        connectionInfo.flowControl = updatedConfig.flowControl;
        connectionInfo.tcpAddress = updatedConfig.tcpAddress;
        connectionInfo.tcpPort = updatedConfig.tcpPort;
        deviceTree->updateConnection(connectionAlias, connectionInfo);
    }

    m_viewModel->setCurrentConnectionAlias(updatedConfig.alias);
    m_viewModel->setConnectionSelected(true);

    LOG_INFO(QString("Connection updated: %1 -> %2")
                 .arg(connectionAlias, updatedConfig.alias));
    QMessageBox::information(this, "成功", "连接配置已更新");
}

void MainWindow::onDeleteMeterRequested(const QString &connectionAlias, const QString &meterAlias)
{
    QMessageBox::StandardButton reply = QMessageBox::question(this, "确认删除",
                                                              QString("确定要删除电表 \"%1\" 吗？").arg(meterAlias),
                                                              QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes)
    {
        const MeterInfo meterInfo = findMeterInfo(connectionAlias, meterAlias);

        DeviceTreeWidget *deviceTree = deviceTreeWidget();
        if (deviceTree)
        {
            deviceTree->deleteMeter(connectionAlias, meterAlias);
            refreshMeterPresence();
        }

        if (!meterInfo.address.isEmpty())
        {
            MeterModel *meterModel = m_meterModelsByAddress.take(meterInfo.address);
            m_protocolController->unregisterMeterModel(meterInfo.address);
            if (meterModel)
            {
                meterModel->deleteLater();
            }
        }

        LOG_INFO(QString("Meter deleted: %1").arg(meterAlias));
    }
}

void MainWindow::onNewMeterRequested(const QString &connectionAlias)
{
    // 检查连接是否已打开
    if (!m_viewModel->isConnected())
    {
        QMessageBox::warning(this, "提示", "请先打开连接");
        return;
    }

    DeviceTreeWidget *deviceTree = deviceTreeWidget();
    if (!deviceTree)
    {
        QMessageBox::warning(this, "提示", "未找到设备树");
        return;
    }

    NewMeterDialog dialog(this);

    // 设置当前电表配置
    MeterConfig currentConfig;
    currentConfig.alias = "电表别名";
    currentConfig.address = "000000000000";
    currentConfig.operatorCode = "00000000";
    currentConfig.password = "00000000";
    currentConfig.includePrefixFE = false;
    dialog.setMeterConfig(currentConfig);

    if (dialog.exec() == QDialog::Accepted)
    {
        MeterConfig config = dialog.getMeterConfig();

        // 检查通信地址是否已存在
        if (deviceTree->isMeterAddressExists(config.address))
        {
            QMessageBox::warning(this, "错误", QString("通信地址 %1 已存在，请使用不同的地址").arg(config.address));
            return;
        }

        // 创建 MeterInfo 结构体
        MeterInfo meterInfo;
        meterInfo.alias = config.alias;
        meterInfo.address = config.address;
        meterInfo.operatorCode = config.operatorCode;
        meterInfo.password = config.password;
        meterInfo.includeLeadingFE = config.includePrefixFE;

        // 添加电表节点到设备树
        if (deviceTree)
        {
            deviceTree->addMeter(connectionAlias, meterInfo);
        }

        refreshMeterPresence();
        applyCurrentMeter(meterInfo);

        QStatusBar *statusBar = this->statusBar();
        QList<QLabel *> labels = statusBar->findChildren<QLabel *>();
        if (labels.size() >= 2)
        {
            labels[1]->setText("地址: " + config.address);
        }

        LOG_INFO(QString("Meter added: Alias=%1, Address=%2, OperatorCode=%3, IncludePrefixFE=%4")
                     .arg(config.alias)
                     .arg(config.address)
                     .arg(config.operatorCode)
                     .arg(config.includePrefixFE ? "true" : "false"));

        QMessageBox::information(this, "成功", "电表已添加");
    }
}
