#include "NewMeterDialog.h"
#include <QGroupBox>
#include <QGridLayout>
#include <QMessageBox>
#include <QRegExp>
#include <QStyle>

NewMeterDialog::NewMeterDialog(QWidget *parent)
    : QDialog(parent), m_aliasEdit(new QLineEdit(this)), m_addressEdit(new QLineEdit(this)), m_addressHintLabel(new QLabel("(0/12位)", this)), m_operatorCodeEdit(new QLineEdit(this)), m_operatorCodeHintLabel(new QLabel("(0/8位)", this)), m_passwordEdit(new QLineEdit(this)), m_passwordHintLabel(new QLabel("(0/8位)", this)), m_includePrefixFECheckBox(new QCheckBox("包含前导FE", this)), m_eventRecordPresentCheckBox(new QCheckBox("事件记录存在", this)), m_phaseTypeComboBox(new QComboBox(this)), m_okButton(new QPushButton("确定", this)), m_cancelButton(new QPushButton("取消", this))
{
    setupUI();
}

NewMeterDialog::~NewMeterDialog()
{
}

void NewMeterDialog::setupUI()
{
    setWindowTitle("新建电表");
    setMinimumWidth(400);
    setMinimumHeight(340);
    setFixedSize(400, 340);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(15);

    // 电表别名
    QHBoxLayout *aliasLayout = new QHBoxLayout();
    aliasLayout->addWidget(new QLabel("电表别名:"));
    m_aliasEdit->setPlaceholderText("电表别名");
    m_aliasEdit->setMaxLength(32);
    aliasLayout->addWidget(m_aliasEdit);
    mainLayout->addLayout(aliasLayout);

    // 通讯地址
    QHBoxLayout *addressLayout = new QHBoxLayout();
    addressLayout->addWidget(new QLabel("通讯地址:"));
    m_addressEdit->setPlaceholderText("000000000000");
    m_addressEdit->setMaxLength(12);
    m_addressEdit->setEchoMode(QLineEdit::Normal);
    // 仅允许数字输入
    QRegExp digitRegExp("\\d*");
    QValidator *digitValidator = new QRegExpValidator(digitRegExp, this);
    m_addressEdit->setValidator(digitValidator);
    addressLayout->addWidget(m_addressEdit);
    addressLayout->addWidget(m_addressHintLabel);
    mainLayout->addLayout(addressLayout);

    // 操作者代码
    QHBoxLayout *operatorCodeLayout = new QHBoxLayout();
    operatorCodeLayout->addWidget(new QLabel("操作者代码:"));
    m_operatorCodeEdit->setPlaceholderText("00000000");
    m_operatorCodeEdit->setMaxLength(8);
    m_operatorCodeEdit->setEchoMode(QLineEdit::Normal);
    m_operatorCodeEdit->setValidator(digitValidator);
    operatorCodeLayout->addWidget(m_operatorCodeEdit);
    operatorCodeLayout->addWidget(m_operatorCodeHintLabel);
    mainLayout->addLayout(operatorCodeLayout);

    // 电能表密码
    QHBoxLayout *passwordLayout = new QHBoxLayout();
    passwordLayout->addWidget(new QLabel("电能表密码:"));
    m_passwordEdit->setPlaceholderText("00000002");
    m_passwordEdit->setMaxLength(8);
    m_passwordEdit->setEchoMode(QLineEdit::Normal);
    m_passwordEdit->setValidator(digitValidator);
    passwordLayout->addWidget(m_passwordEdit);
    passwordLayout->addWidget(m_passwordHintLabel);
    mainLayout->addLayout(passwordLayout);

    QHBoxLayout *phaseTypeLayout = new QHBoxLayout();
    phaseTypeLayout->addWidget(new QLabel("电表类型:"));
    m_phaseTypeComboBox->addItem(meterPhaseTypeToDisplayName(MeterPhaseType::ThreePhase), static_cast<int>(MeterPhaseType::ThreePhase));
    m_phaseTypeComboBox->addItem(meterPhaseTypeToDisplayName(MeterPhaseType::SinglePhase), static_cast<int>(MeterPhaseType::SinglePhase));
    phaseTypeLayout->addWidget(m_phaseTypeComboBox);
    mainLayout->addLayout(phaseTypeLayout);

    // 包含前导FE
    m_includePrefixFECheckBox->setChecked(true);
    mainLayout->addWidget(m_includePrefixFECheckBox);

    // 事件记录存在
    m_eventRecordPresentCheckBox->setChecked(true);
    mainLayout->addWidget(m_eventRecordPresentCheckBox);

    // 按钮栏
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    m_okButton->setMinimumWidth(100);
    connect(m_okButton, &QPushButton::clicked, this, &NewMeterDialog::onOkClicked);
    buttonLayout->addWidget(m_okButton);

    m_cancelButton->setMinimumWidth(100);
    connect(m_cancelButton, &QPushButton::clicked, this, &NewMeterDialog::onCancelClicked);
    buttonLayout->addWidget(m_cancelButton);

    mainLayout->addLayout(buttonLayout);

    // 连接文本变化信号
    connect(m_addressEdit, &QLineEdit::textChanged, this, &NewMeterDialog::onAddressTextChanged);
    connect(m_operatorCodeEdit, &QLineEdit::textChanged, this, &NewMeterDialog::onOperatorCodeTextChanged);
    connect(m_passwordEdit, &QLineEdit::textChanged, this, &NewMeterDialog::onPasswordTextChanged);
}

MeterConfig NewMeterDialog::getMeterConfig() const
{
    return m_config;
}

void NewMeterDialog::setMeterConfig(const MeterConfig &config)
{
    m_config = config;

    m_aliasEdit->setText(config.alias);
    m_addressEdit->setText(config.address);
    m_operatorCodeEdit->setText(config.operatorCode);
    m_passwordEdit->setText(config.password);
    m_includePrefixFECheckBox->setChecked(config.includePrefixFE);
    m_eventRecordPresentCheckBox->setChecked(config.eventRecordPresent);
    const int phaseTypeIndex = m_phaseTypeComboBox->findData(static_cast<int>(config.phaseType));
    m_phaseTypeComboBox->setCurrentIndex(phaseTypeIndex >= 0 ? phaseTypeIndex : 0);
}

void NewMeterDialog::onOkClicked()
{
    // 验证输入
    if (!validateInput())
    {
        return;
    }

    // 保存配置
    m_config.alias = m_aliasEdit->text().trimmed();
    m_config.address = m_addressEdit->text();
    m_config.operatorCode = m_operatorCodeEdit->text();
    m_config.password = m_passwordEdit->text();
    m_config.includePrefixFE = m_includePrefixFECheckBox->isChecked();
    m_config.eventRecordPresent = m_eventRecordPresentCheckBox->isChecked();
    m_config.phaseType = static_cast<MeterPhaseType>(m_phaseTypeComboBox->currentData().toInt());

    accept();
}

void NewMeterDialog::onCancelClicked()
{
    reject();
}

void NewMeterDialog::onAddressTextChanged(const QString &text)
{
    m_addressHintLabel->setText(QString("(%1/12位)").arg(text.length()));

    // 验证长度
    if (text.length() > 12)
    {
        m_addressEdit->setProperty("error", true);
        m_addressEdit->style()->unpolish(m_addressEdit);
        m_addressEdit->style()->polish(m_addressEdit);
    }
    else
    {
        m_addressEdit->setProperty("error", false);
        m_addressEdit->style()->unpolish(m_addressEdit);
        m_addressEdit->style()->polish(m_addressEdit);
    }
}

void NewMeterDialog::onOperatorCodeTextChanged(const QString &text)
{
    m_operatorCodeHintLabel->setText(QString("(%1/8位)").arg(text.length()));

    // 验证长度
    if (text.length() > 8)
    {
        m_operatorCodeEdit->setProperty("error", true);
        m_operatorCodeEdit->style()->unpolish(m_operatorCodeEdit);
        m_operatorCodeEdit->style()->polish(m_operatorCodeEdit);
    }
    else
    {
        m_operatorCodeEdit->setProperty("error", false);
        m_operatorCodeEdit->style()->unpolish(m_operatorCodeEdit);
        m_operatorCodeEdit->style()->polish(m_operatorCodeEdit);
    }
}

void NewMeterDialog::onPasswordTextChanged(const QString &text)
{
    m_passwordHintLabel->setText(QString("(%1/8位)").arg(text.length()));

    // 验证长度
    if (text.length() > 8)
    {
        m_passwordEdit->setProperty("error", true);
        m_passwordEdit->style()->unpolish(m_passwordEdit);
        m_passwordEdit->style()->polish(m_passwordEdit);
    }
    else
    {
        m_passwordEdit->setProperty("error", false);
        m_passwordEdit->style()->unpolish(m_passwordEdit);
        m_passwordEdit->style()->polish(m_passwordEdit);
    }
}

bool NewMeterDialog::validateInput()
{
    // 验证电表别名
    // if (m_aliasEdit->text().trimmed().isEmpty())
    // {
    //     QMessageBox::warning(this, "验证失败", "电表别名不能为空");
    //     return false;
    // }

    if (m_aliasEdit->text().length() > 32)
    {
        QMessageBox::warning(this, "验证失败", "电表别名不能超过32个字符");
        return false;
    }

    // 验证通讯地址
    if (m_addressEdit->text().isEmpty())
    {
        QMessageBox::warning(this, "验证失败", "通讯地址不能为空");
        return false;
    }

    if (m_addressEdit->text().length() != 12)
    {
        QMessageBox::warning(this, "验证失败", "通讯地址必须为12位数字");
        return false;
    }

    // 验证操作者代码
    if (m_operatorCodeEdit->text().isEmpty())
    {
        QMessageBox::warning(this, "验证失败", "操作者代码不能为空");
        return false;
    }

    if (m_operatorCodeEdit->text().length() != 8)
    {
        QMessageBox::warning(this, "验证失败", "操作者代码必须为8位数字");
        return false;
    }

    // 验证电能表密码
    if (m_passwordEdit->text().isEmpty())
    {
        QMessageBox::warning(this, "验证失败", "电能表密码不能为空");
        return false;
    }

    if (m_passwordEdit->text().length() != 8)
    {
        QMessageBox::warning(this, "验证失败", "电能表密码必须为8位数字");
        return false;
    }

    return true;
}
