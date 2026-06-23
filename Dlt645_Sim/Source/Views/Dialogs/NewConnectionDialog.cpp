#include "NewConnectionDialog.h"
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QStackedWidget>
#include <QGroupBox>
#include <QGridLayout>
#include <QRegExpValidator>
#include <QMessageBox>
#include <algorithm>

NewConnectionDialog::NewConnectionDialog(QWidget *parent)
    : QDialog(parent), m_connectionTypeCombo(new QComboBox(this)), m_aliasEdit(new QLineEdit(this)), m_configStack(new QStackedWidget(this)), m_serialPortCombo(new QComboBox(this)), m_baudRateCombo(new QComboBox(this)), m_dataBitsCombo(new QComboBox(this)), m_stopBitsCombo(new QComboBox(this)), m_parityCombo(new QComboBox(this)), m_flowControlCombo(new QComboBox(this)), m_refreshPortsButton(new QPushButton("刷新", this)), m_tcpAddressEdit(new QLineEdit(this)), m_tcpPortSpinBox(new QSpinBox(this)), m_okButton(new QPushButton("连接", this)), m_cancelButton(new QPushButton("取消", this))
{
    setupUI();
    onRefreshPorts();
}

NewConnectionDialog::~NewConnectionDialog()
{
}

void NewConnectionDialog::setupUI()
{
    setWindowTitle("新建连接");
    setMinimumWidth(450);
    setMinimumHeight(400);
    setFixedSize(450, 400);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);

    // 连接类型选择
    QHBoxLayout *typeLayout = new QHBoxLayout();
    typeLayout->addWidget(new QLabel("连接类型:"));
    m_connectionTypeCombo->addItem("串口");
    m_connectionTypeCombo->addItem("TCP");
    connect(m_connectionTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &NewConnectionDialog::onConnectionTypeChanged);
    typeLayout->addWidget(m_connectionTypeCombo);
    mainLayout->addLayout(typeLayout);

    // 连接别名
    QHBoxLayout *aliasLayout = new QHBoxLayout();
    aliasLayout->addWidget(new QLabel("连接别名:"));
    m_aliasEdit->setPlaceholderText("请输入连接别名");
    m_aliasEdit->setMaxLength(32);
    aliasLayout->addWidget(m_aliasEdit);
    mainLayout->addLayout(aliasLayout);

    // 创建配置区域切换器
    m_configStack = new QStackedWidget(this);

    // 创建并添加串口配置页面
    setupSerialConfig();

    // 创建并添加TCP配置页面
    setupTCPConfig();

    mainLayout->addWidget(m_configStack);

    // 按钮栏
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    m_okButton->setMinimumWidth(100);
    connect(m_okButton, &QPushButton::clicked, this, &NewConnectionDialog::onOkClicked);
    buttonLayout->addWidget(m_okButton);

    m_cancelButton->setMinimumWidth(100);
    connect(m_cancelButton, &QPushButton::clicked, this, &NewConnectionDialog::onCancelClicked);
    buttonLayout->addWidget(m_cancelButton);

    mainLayout->addLayout(buttonLayout);
}

void NewConnectionDialog::setupSerialConfig()
{
    QWidget *serialPage = new QWidget();
    QFormLayout *formLayout = new QFormLayout(serialPage);
    formLayout->setSpacing(15);
    formLayout->setContentsMargins(20, 20, 20, 20);

    // 串口
    QHBoxLayout *portLayout = new QHBoxLayout();
    portLayout->addWidget(m_serialPortCombo);
    m_refreshPortsButton->setMaximumWidth(80);
    connect(m_refreshPortsButton, &QPushButton::clicked, this, &NewConnectionDialog::onRefreshPorts);
    portLayout->addWidget(m_refreshPortsButton);
    formLayout->addRow("串口:", portLayout);

    // 波特率
    m_baudRateCombo->addItem("1200");
    m_baudRateCombo->addItem("2400");
    m_baudRateCombo->addItem("4800");
    m_baudRateCombo->addItem("9600");
    m_baudRateCombo->addItem("19200");
    m_baudRateCombo->addItem("38400");
    m_baudRateCombo->addItem("57600");
    m_baudRateCombo->addItem("115200");
    m_baudRateCombo->setCurrentText("2400");
    formLayout->addRow("波特率:", m_baudRateCombo);

    // 数据位
    m_dataBitsCombo->addItem("5");
    m_dataBitsCombo->addItem("6");
    m_dataBitsCombo->addItem("7");
    m_dataBitsCombo->addItem("8");
    m_dataBitsCombo->setCurrentText("8");
    formLayout->addRow("数据位:", m_dataBitsCombo);

    // 停止位
    m_stopBitsCombo->addItem("1");
    m_stopBitsCombo->addItem("1.5");
    m_stopBitsCombo->addItem("2");
    m_stopBitsCombo->setCurrentText("1");
    formLayout->addRow("停止位:", m_stopBitsCombo);

    // 校验位
    m_parityCombo->addItem("无");
    m_parityCombo->addItem("奇校验");
    m_parityCombo->addItem("偶校验");
    m_parityCombo->setCurrentText("偶校验");
    formLayout->addRow("校验位:", m_parityCombo);

    // 流控
    m_flowControlCombo->addItem("None");
    m_flowControlCombo->addItem("Hardware");
    m_flowControlCombo->addItem("Software");
    m_flowControlCombo->setCurrentText("None");
    formLayout->addRow("流控:", m_flowControlCombo);

    m_configStack->addWidget(serialPage);
}

void NewConnectionDialog::setupTCPConfig()
{
    QWidget *tcpPage = new QWidget();
    QFormLayout *formLayout = new QFormLayout(tcpPage);
    formLayout->setSpacing(15);
    formLayout->setContentsMargins(20, 20, 20, 20);

    // IP地址
    m_tcpAddressEdit->setPlaceholderText("127.0.0.1");
    // 设置IP地址验证器
    QRegExp ipPattern("^((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.){3}(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$");
    m_tcpAddressEdit->setValidator(new QRegExpValidator(ipPattern, this));
    formLayout->addRow("服务端IP地址:", m_tcpAddressEdit);

    // 端口
    m_tcpPortSpinBox->setRange(1, 65535);
    m_tcpPortSpinBox->setValue(8080);
    formLayout->addRow("服务端端口号:", m_tcpPortSpinBox);

    m_configStack->addWidget(tcpPage);
}

void NewConnectionDialog::onRefreshPorts()
{
    m_serialPortCombo->clear();

    QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();

    // 对串口列表按端口名进行自然排序（COM1, COM3, COM10...）
    std::sort(ports.begin(), ports.end(), [](const QSerialPortInfo &a, const QSerialPortInfo &b) {
        QString nameA = a.portName();
        QString nameB = b.portName();

        // 提取数字部分进行比较
        QRegExp rxA("(\\D+)(\\d+)");
        QRegExp rxB("(\\D+)(\\d+)");
        int posA = rxA.indexIn(nameA);
        int posB = rxB.indexIn(nameB);

        if (posA >= 0 && posB >= 0)
        {
            QString prefixA = rxA.cap(1);
            QString prefixB = rxB.cap(1);
            int numA = rxA.cap(2).toInt();
            int numB = rxB.cap(2).toInt();

            if (prefixA == prefixB)
            {
                return numA < numB;  // 数字小的在前
            }
        }

        return nameA < nameB;  // 无数字时按字母排序
    });

    for (const QSerialPortInfo &port : ports)
    {
        QString portName = port.portName();
        QString displayText = portName;

        // 检查端口是否已连接，如果已连接则添加标记
        if (m_connectedPorts.contains(portName))
        {
            displayText += " (已连接)";
        }

        m_serialPortCombo->addItem(displayText, portName); // 使用userData存储原始端口名
    }

    if (m_serialPortCombo->count() > 0)
    {
        m_serialPortCombo->setCurrentIndex(0);
    }
    else
    {
        QMessageBox::information(this, "提示", "未检测到可用串口");
    }
}

void NewConnectionDialog::setConnectedPorts(const QStringList &connectedPorts)
{
    m_connectedPorts = connectedPorts;
    // 刷新端口列表以更新显示
    onRefreshPorts();
}

void NewConnectionDialog::setEditMode(bool editing)
{
    setWindowTitle(editing ? "编辑连接" : "新建连接");
    m_okButton->setText(editing ? "保存" : "连接");
}

ConnectionConfig NewConnectionDialog::getConnectionConfig() const
{
    return m_config;
}

void NewConnectionDialog::setConnectionConfig(const ConnectionConfig &config)
{
    m_config = config;
    m_aliasEdit->setText(config.alias);

    // 设置连接类型
    m_connectionTypeCombo->setCurrentIndex(config.type == ConnectionType::Serial ? 0 : 1);

    // 设置串口配置
    if (config.type == ConnectionType::Serial)
    {
        int portIndex = m_serialPortCombo->findData(config.serialPort);
        if (portIndex < 0)
        {
            portIndex = m_serialPortCombo->findText(config.serialPort);
        }
        if (portIndex >= 0)
        {
            m_serialPortCombo->setCurrentIndex(portIndex);
        }

        m_baudRateCombo->setCurrentText(QString::number(config.baudRate));
        m_dataBitsCombo->setCurrentText(QString::number(config.dataBits));
        m_stopBitsCombo->setCurrentText(QString::number(config.stopBits));

        switch (config.parity)
        {
        case 0:
            m_parityCombo->setCurrentText("无");
            break;
        case 1:
            m_parityCombo->setCurrentText("奇校验");
            break;
        case 2:
            m_parityCombo->setCurrentText("偶校验");
            break;
        }

        // 设置流控
        switch (config.flowControl)
        {
        case 0:
            m_flowControlCombo->setCurrentText("None");
            break;
        case 1:
            m_flowControlCombo->setCurrentText("Hardware");
            break;
        case 2:
            m_flowControlCombo->setCurrentText("Software");
            break;
        }
    }
    else
    {
        // 设置TCP配置
        m_tcpAddressEdit->setText(config.tcpAddress);
        m_tcpPortSpinBox->setValue(config.tcpPort);
    }
}

void NewConnectionDialog::onConnectionTypeChanged(int index)
{
    m_config.type = (index == 0) ? ConnectionType::Serial : ConnectionType::TCP;
    updateUI();
}

void NewConnectionDialog::onOkClicked()
{
    // 验证输入
    if (!validateInput())
    {
        return;
    }

    // 保存连接别名
    m_config.alias = m_aliasEdit->text().trimmed();

    // 如果别名为空，使用默认别名（别名+当前选择的端口）
    if (m_config.alias.isEmpty())
    {
        if (m_config.type == ConnectionType::Serial)
        {
            // 使用userData获取原始端口名（去除"(已连接)"标记）
            QString port = m_serialPortCombo->currentData().toString();
            if (port.isEmpty())
            {
                port = m_serialPortCombo->currentText();
            }
            m_config.alias = "连接_" + port;
        }
        else
        {
            m_config.alias = "连接_" + m_tcpAddressEdit->text() + ":" + QString::number(m_tcpPortSpinBox->value());
        }
    }

    // 保存配置
    if (m_config.type == ConnectionType::Serial)
    {
        // 使用userData获取原始端口名（去除"(已连接)"标记）
        m_config.serialPort = m_serialPortCombo->currentData().toString();
        if (m_config.serialPort.isEmpty())
        {
            m_config.serialPort = m_serialPortCombo->currentText();
        }
        m_config.baudRate = m_baudRateCombo->currentText().toInt();
        m_config.dataBits = m_dataBitsCombo->currentText().toInt();
        m_config.stopBits = m_stopBitsCombo->currentText().toInt();

        QString parityText = m_parityCombo->currentText();
        if (parityText == "无")
        {
            m_config.parity = 0;
        }
        else if (parityText == "奇校验")
        {
            m_config.parity = 1;
        }
        else
        {
            m_config.parity = 2;
        }

        // 保存流控
        QString flowText = m_flowControlCombo->currentText();
        if (flowText == "None")
        {
            m_config.flowControl = 0;
        }
        else if (flowText == "Hardware")
        {
            m_config.flowControl = 1;
        }
        else
        {
            m_config.flowControl = 2;
        }
    }
    else
    {
        m_config.tcpAddress = m_tcpAddressEdit->text();
        m_config.tcpPort = m_tcpPortSpinBox->value();
    }

    accept();
}

void NewConnectionDialog::onCancelClicked()
{
    reject();
}

void NewConnectionDialog::updateUI()
{
    // 根据连接类型切换配置页面
    if (m_config.type == ConnectionType::Serial)
    {
        m_configStack->setCurrentIndex(0); // 串口配置页面
    }
    else
    {
        m_configStack->setCurrentIndex(1); // TCP配置页面
    }
}

bool NewConnectionDialog::validateInput()
{
    // 验证连接别名
    QString alias = m_aliasEdit->text().trimmed();
    if (alias.isEmpty())
    {
        // 别名为空时，将使用默认别名，不需要验证
        return true;
    }

    if (alias.length() > 32)
    {
        QMessageBox::warning(this, "验证失败", "连接别名不能超过32个字符");
        return false;
    }

    // 验证TCP配置
    if (m_config.type == ConnectionType::TCP)
    {
        QString ip = m_tcpAddressEdit->text().trimmed();
        if (ip.isEmpty())
        {
            QMessageBox::warning(this, "验证失败", "IP地址不能为空");
            return false;
        }

        QRegExp ipPattern("^((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.){3}(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$");
        if (!ipPattern.exactMatch(ip))
        {
            QMessageBox::warning(this, "验证失败", "IP地址格式不正确");
            return false;
        }
    }

    return true;
}
