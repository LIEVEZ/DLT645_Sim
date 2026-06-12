#include "BatchNewMeterDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegExp>
#include <QRegExpValidator>
#include <QSpinBox>
#include <QValidator>
#include <QVBoxLayout>

BatchNewMeterDialog::BatchNewMeterDialog(QWidget *parent)
    : QDialog(parent),
      m_aliasPrefixEdit(new QLineEdit(this)),
      m_aliasStartNumberSpinBox(new QSpinBox(this)),
      m_startAddressEdit(new QLineEdit(this)),
      m_startAddressHintLabel(new QLabel(this)),
      m_countSpinBox(new QSpinBox(this)),
      m_operatorCodeEdit(new QLineEdit(this)),
      m_operatorCodeHintLabel(new QLabel(this)),
      m_passwordEdit(new QLineEdit(this)),
      m_passwordHintLabel(new QLabel(this)),
      m_phaseTypeComboBox(new QComboBox(this)),
      m_includePrefixFECheckBox(new QCheckBox(QStringLiteral("包含前导FE"), this)),
      m_previewLabel(new QLabel(this)),
      m_okButton(new QPushButton(QStringLiteral("确定"), this)),
      m_cancelButton(new QPushButton(QStringLiteral("取消"), this))
{
    setupUI();
    setBatchMeterConfig(m_config);
}

void BatchNewMeterDialog::setupUI()
{
    setWindowTitle(QStringLiteral("批量新建电表"));
    setFixedSize(460, 380);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);

    QFormLayout *formLayout = new QFormLayout();
    formLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    formLayout->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
    formLayout->setHorizontalSpacing(12);
    formLayout->setVerticalSpacing(10);

    m_aliasPrefixEdit->setMaxLength(24);
    formLayout->addRow(QStringLiteral("别名前缀:"), m_aliasPrefixEdit);

    m_aliasStartNumberSpinBox->setRange(0, 999999);
    formLayout->addRow(QStringLiteral("别名起始序号:"), m_aliasStartNumberSpinBox);

    QHBoxLayout *addressLayout = new QHBoxLayout();
    m_startAddressEdit->setMaxLength(12);
    QRegExp digitRegExp("\\d*");
    QValidator *digitValidator = new QRegExpValidator(digitRegExp, this);
    m_startAddressEdit->setValidator(digitValidator);
    addressLayout->addWidget(m_startAddressEdit, 1);
    addressLayout->addWidget(m_startAddressHintLabel);
    formLayout->addRow(QStringLiteral("起始通讯地址:"), addressLayout);

    m_countSpinBox->setRange(1, 1000);
    formLayout->addRow(QStringLiteral("新增数量:"), m_countSpinBox);

    QHBoxLayout *operatorCodeLayout = new QHBoxLayout();
    m_operatorCodeEdit->setMaxLength(8);
    m_operatorCodeEdit->setValidator(digitValidator);
    operatorCodeLayout->addWidget(m_operatorCodeEdit, 1);
    operatorCodeLayout->addWidget(m_operatorCodeHintLabel);
    formLayout->addRow(QStringLiteral("操作者代码:"), operatorCodeLayout);

    QHBoxLayout *passwordLayout = new QHBoxLayout();
    m_passwordEdit->setMaxLength(8);
    m_passwordEdit->setValidator(digitValidator);
    passwordLayout->addWidget(m_passwordEdit, 1);
    passwordLayout->addWidget(m_passwordHintLabel);
    formLayout->addRow(QStringLiteral("电能表密码:"), passwordLayout);

    m_phaseTypeComboBox->addItem(meterPhaseTypeToDisplayName(MeterPhaseType::ThreePhase), static_cast<int>(MeterPhaseType::ThreePhase));
    m_phaseTypeComboBox->addItem(meterPhaseTypeToDisplayName(MeterPhaseType::SinglePhase), static_cast<int>(MeterPhaseType::SinglePhase));
    formLayout->addRow(QStringLiteral("电表类型:"), m_phaseTypeComboBox);

    formLayout->addRow(QString(), m_includePrefixFECheckBox);
    mainLayout->addLayout(formLayout);

    m_previewLabel->setWordWrap(true);
    m_previewLabel->setMinimumHeight(48);
    mainLayout->addWidget(m_previewLabel);
    mainLayout->addStretch();

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    m_okButton->setMinimumWidth(100);
    m_cancelButton->setMinimumWidth(100);
    buttonLayout->addWidget(m_okButton);
    buttonLayout->addWidget(m_cancelButton);
    mainLayout->addLayout(buttonLayout);

    connect(m_okButton, &QPushButton::clicked, this, &BatchNewMeterDialog::onOkClicked);
    connect(m_cancelButton, &QPushButton::clicked, this, &BatchNewMeterDialog::reject);
    connect(m_aliasPrefixEdit, &QLineEdit::textChanged, this, &BatchNewMeterDialog::updateHintsAndPreview);
    connect(m_aliasStartNumberSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &BatchNewMeterDialog::updateHintsAndPreview);
    connect(m_startAddressEdit, &QLineEdit::textChanged, this, &BatchNewMeterDialog::updateHintsAndPreview);
    connect(m_countSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &BatchNewMeterDialog::updateHintsAndPreview);
    connect(m_operatorCodeEdit, &QLineEdit::textChanged, this, &BatchNewMeterDialog::updateHintsAndPreview);
    connect(m_passwordEdit, &QLineEdit::textChanged, this, &BatchNewMeterDialog::updateHintsAndPreview);
}

BatchMeterConfig BatchNewMeterDialog::getBatchMeterConfig() const
{
    return m_config;
}

void BatchNewMeterDialog::setBatchMeterConfig(const BatchMeterConfig &config)
{
    m_config = config;

    m_aliasPrefixEdit->setText(config.aliasPrefix);
    m_aliasStartNumberSpinBox->setValue(config.aliasStartNumber);
    m_startAddressEdit->setText(config.startAddress);
    m_countSpinBox->setValue(config.count);
    m_operatorCodeEdit->setText(config.operatorCode);
    m_passwordEdit->setText(config.password);
    m_includePrefixFECheckBox->setChecked(config.includePrefixFE);
    const int phaseTypeIndex = m_phaseTypeComboBox->findData(static_cast<int>(config.phaseType));
    m_phaseTypeComboBox->setCurrentIndex(phaseTypeIndex >= 0 ? phaseTypeIndex : 0);

    updateHintsAndPreview();
}

void BatchNewMeterDialog::onOkClicked()
{
    if (!validateInput())
    {
        return;
    }

    m_config.aliasPrefix = m_aliasPrefixEdit->text().trimmed();
    m_config.aliasStartNumber = m_aliasStartNumberSpinBox->value();
    m_config.startAddress = m_startAddressEdit->text();
    m_config.count = m_countSpinBox->value();
    m_config.operatorCode = m_operatorCodeEdit->text();
    m_config.password = m_passwordEdit->text();
    m_config.includePrefixFE = m_includePrefixFECheckBox->isChecked();
    m_config.eventRecordPresent = true;  // 默认为true
    m_config.phaseType = static_cast<MeterPhaseType>(m_phaseTypeComboBox->currentData().toInt());

    accept();
}

void BatchNewMeterDialog::updateHintsAndPreview()
{
    m_startAddressHintLabel->setText(QStringLiteral("(%1/12位)").arg(m_startAddressEdit->text().length()));
    m_operatorCodeHintLabel->setText(QStringLiteral("(%1/8位)").arg(m_operatorCodeEdit->text().length()));
    m_passwordHintLabel->setText(QStringLiteral("(%1/8位)").arg(m_passwordEdit->text().length()));

    const QString firstAlias = aliasAt(0);
    const QString firstAddress = addressAt(0);
    const QString lastAlias = aliasAt(m_countSpinBox->value() - 1);
    const QString lastAddress = addressAt(m_countSpinBox->value() - 1);
    m_previewLabel->setText(QStringLiteral("预览: %1 [%2] 至 %3 [%4]")
                                .arg(firstAlias, firstAddress, lastAlias, lastAddress));
}

bool BatchNewMeterDialog::validateInput()
{
    if (m_aliasPrefixEdit->text().trimmed().isEmpty())
    {
        QMessageBox::warning(this, QStringLiteral("验证失败"), QStringLiteral("别名前缀不能为空"));
        return false;
    }

    if (m_aliasPrefixEdit->text().trimmed().length() > 24)
    {
        QMessageBox::warning(this, QStringLiteral("验证失败"), QStringLiteral("别名前缀不能超过24个字符"));
        return false;
    }

    if (m_startAddressEdit->text().length() != 12)
    {
        QMessageBox::warning(this, QStringLiteral("验证失败"), QStringLiteral("起始通讯地址必须为12位数字"));
        return false;
    }

    if (m_operatorCodeEdit->text().length() != 8)
    {
        QMessageBox::warning(this, QStringLiteral("验证失败"), QStringLiteral("操作者代码必须为8位数字"));
        return false;
    }

    if (m_passwordEdit->text().length() != 8)
    {
        QMessageBox::warning(this, QStringLiteral("验证失败"), QStringLiteral("电能表密码必须为8位数字"));
        return false;
    }

    if (addressAt(m_countSpinBox->value() - 1).length() != 12)
    {
        QMessageBox::warning(this, QStringLiteral("验证失败"), QStringLiteral("批量地址递增后超过12位，请减少数量或调整起始地址"));
        return false;
    }

    const int lastAliasNumber = m_aliasStartNumberSpinBox->value() + m_countSpinBox->value() - 1;
    if (m_aliasPrefixEdit->text().trimmed().length() + QString::number(lastAliasNumber).length() > 32)
    {
        QMessageBox::warning(this, QStringLiteral("验证失败"), QStringLiteral("生成后的电表别名不能超过32个字符"));
        return false;
    }

    return true;
}

QString BatchNewMeterDialog::addressAt(int offset) const
{
    bool ok = false;
    const quint64 startAddress = m_startAddressEdit->text().toULongLong(&ok);
    if (!ok)
    {
        return m_startAddressEdit->text();
    }

    const quint64 address = startAddress + static_cast<quint64>(offset);
    return QString("%1").arg(address, 12, 10, QLatin1Char('0'));
}

QString BatchNewMeterDialog::aliasAt(int offset) const
{
    const int number = m_aliasStartNumberSpinBox->value() + offset;
    return QStringLiteral("%1%2").arg(m_aliasPrefixEdit->text().trimmed()).arg(number);
}
