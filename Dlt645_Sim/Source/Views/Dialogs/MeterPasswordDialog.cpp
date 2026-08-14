#include "Views/Dialogs/MeterPasswordDialog.h"

#include "Utils/AuthenticationStateFrameGenerator.h"
#include "Utils/MeterAuthenticationCommandGenerator.h"
#include "Utils/MeterPasswordGenerator.h"
#include "Utils/UiTheme.h"

#include <QCloseEvent>
#include <QClipboard>
#include <QDateTime>
#include <QDateTimeEdit>
#include <QFormLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QTabWidget>
#include <QVBoxLayout>

namespace
{
QString encryptedMeterToolStyleTemplate()
{
    return QStringLiteral(R"(
        QDialog {
            background-color: {{windowBg}};
        }

        QLabel {
            color: {{text}};
            font-family: "Microsoft YaHei UI";
        }

        QLabel#encryptedMeterToolStatus {
            color: {{warning}};
        }

        QLineEdit,
        QDateTimeEdit,
        QPlainTextEdit {
            background-color: {{panelBg}};
            color: {{text}};
            border: 1px solid {{border}};
            border-radius: 4px;
            padding: 5px;
        }

        QLineEdit:focus,
        QDateTimeEdit:focus,
        QPlainTextEdit:focus {
            border: 1px solid {{accent}};
        }

        QLineEdit#meterPasswordResult[readOnly="true"],
        QLineEdit#authenticationFrameResult[readOnly="true"],
        QLineEdit#authenticationCompactFrameResult[readOnly="true"],
        QLineEdit#meterAuthenticationOperatorCode[readOnly="true"],
        QPlainTextEdit#meterAuthenticationFrameResult[readOnly="true"],
        QPlainTextEdit#meterAuthenticationCompactFrameResult[readOnly="true"] {
            background-color: {{readonlyBg}};
            color: {{textReadonly}};
            font-weight: 600;
        }

        QTabWidget::pane {
            border: 1px solid {{border}};
            background-color: {{windowBg}};
            top: -1px;
        }

        QTabBar::tab {
            background-color: {{panelBg}};
            color: {{textMuted}};
            border: 1px solid {{border}};
            border-bottom: 0;
            border-top-left-radius: 4px;
            border-top-right-radius: 4px;
            min-width: 92px;
            padding: 6px 12px;
        }

        QTabBar::tab:selected {
            background-color: {{windowBg}};
            color: {{text}};
            border-color: {{accent}};
        }

        QRadioButton {
            color: {{text}};
            spacing: 6px;
        }

        QRadioButton::indicator {
            width: 14px;
            height: 14px;
        }

        {{buttonBlock}}
    )");
}
} // namespace

MeterPasswordDialog::MeterPasswordDialog(QWidget *parent)
    : QDialog(parent),
      m_meterAddressEdit(nullptr),
      m_keyEdit(nullptr),
      m_passwordEdit(nullptr),
      m_enableAuthenticationButton(nullptr),
      m_disableAuthenticationButton(nullptr),
      m_authenticationFrameEdit(nullptr),
      m_authenticationCompactFrameEdit(nullptr),
      m_meterAuthenticationPasswordEdit(nullptr),
      m_operatorCodeEdit(nullptr),
      m_deadlineEdit(nullptr),
      m_baselineTimeEdit(nullptr),
      m_meterAuthenticationFrameEdit(nullptr),
      m_meterAuthenticationCompactFrameEdit(nullptr),
      m_statusLabel(nullptr),
      m_generateButton(nullptr),
      m_copyPasswordButton(nullptr),
      m_generateAuthenticationFrameButton(nullptr),
      m_generateMeterAuthenticationButton(nullptr)
{
    setupUi();
    applyDarkTheme();
}

MeterPasswordDialog::~MeterPasswordDialog()
{
}

void MeterPasswordDialog::setMeterAddress(const QString &meterAddress)
{
    if (m_meterAddressEdit->text() != meterAddress)
    {
        m_meterAddressEdit->setText(meterAddress);
    }
}

void MeterPasswordDialog::applyDarkTheme()
{
    applyTheme(ui::darkPalette());
}

void MeterPasswordDialog::applyLightTheme()
{
    applyTheme(ui::lightPalette());
}

void MeterPasswordDialog::onGenerateClicked()
{
    QString password;
    QString errorMessage;
    if (!MeterPasswordGenerator::generate(m_meterAddressEdit->text(), m_keyEdit->text(), &password, &errorMessage))
    {
        m_passwordEdit->clear();
        m_copyPasswordButton->setEnabled(false);
        m_statusLabel->setText(errorMessage);
        return;
    }

    m_passwordEdit->setText(password);
    m_meterAuthenticationPasswordEdit->setText(password);
    m_copyPasswordButton->setEnabled(true);
    m_statusLabel->clear();
}

void MeterPasswordDialog::onCopyPasswordClicked()
{
    QGuiApplication::clipboard()->setText(m_passwordEdit->text());
    m_statusLabel->setText(QString::fromUtf8("密码已复制到剪贴板。"));
}

void MeterPasswordDialog::onAuthenticationGenerateClicked()
{
    QString frame;
    QString errorMessage;
    const bool authenticationEnabled = m_enableAuthenticationButton->isChecked();
    if (!AuthenticationStateFrameGenerator::generateWriteFrame(m_meterAddressEdit->text(),
                                                                 authenticationEnabled,
                                                                 &frame,
                                                                 &errorMessage))
    {
        clearAuthenticationFrame();
        m_statusLabel->setText(errorMessage);
        return;
    }

    m_authenticationFrameEdit->setText(frame);
    m_authenticationCompactFrameEdit->setText(QString(frame).remove(QLatin1Char(' ')));
    m_statusLabel->clear();
}

void MeterPasswordDialog::onMeterAuthenticationGenerateClicked()
{
    QString frame;
    QString errorMessage;
    if (!MeterAuthenticationCommandGenerator::generate(m_meterAddressEdit->text(),
                                                        m_meterAuthenticationPasswordEdit->text(),
                                                        m_deadlineEdit->dateTime(),
                                                        m_baselineTimeEdit->dateTime(),
                                                        &frame,
                                                        &errorMessage))
    {
        clearMeterAuthenticationFrame();
        m_statusLabel->setText(errorMessage);
        return;
    }

    m_meterAuthenticationFrameEdit->setPlainText(frame);
    m_meterAuthenticationCompactFrameEdit->setPlainText(QString(frame).remove(QLatin1Char(' ')));
    m_statusLabel->clear();
}

void MeterPasswordDialog::clearGeneratedPassword()
{
    const bool clearAuthenticationPassword = m_meterAuthenticationPasswordEdit
                                             && m_meterAuthenticationPasswordEdit->text()
                                                    == m_passwordEdit->text();
    m_passwordEdit->clear();
    if (clearAuthenticationPassword)
    {
        m_meterAuthenticationPasswordEdit->clear();
    }
    m_copyPasswordButton->setEnabled(false);
    m_statusLabel->clear();
}

void MeterPasswordDialog::clearMeterAuthenticationFrame()
{
    m_meterAuthenticationFrameEdit->clear();
    m_meterAuthenticationCompactFrameEdit->clear();
    m_statusLabel->clear();
}

void MeterPasswordDialog::clearAuthenticationFrame()
{
    m_authenticationFrameEdit->clear();
    m_authenticationCompactFrameEdit->clear();
    m_statusLabel->clear();
}

void MeterPasswordDialog::closeEvent(QCloseEvent *event)
{
    emit dialogClosed();
    event->accept();
}

void MeterPasswordDialog::setupUi()
{
    setWindowTitle(QString::fromUtf8("加密电表工具"));
    setMinimumWidth(560);
    resize(660, 480);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    QFormLayout *addressLayout = new QFormLayout();
    addressLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    addressLayout->setHorizontalSpacing(10);
    addressLayout->setVerticalSpacing(0);

    m_meterAddressEdit = new QLineEdit(this);
    m_meterAddressEdit->setPlaceholderText(QString::fromUtf8("12 位数字表号"));
    m_meterAddressEdit->setMaxLength(12);
    m_meterAddressEdit->setValidator(
        new QRegularExpressionValidator(QRegularExpression(QStringLiteral("\\d{0,12}")), m_meterAddressEdit));
    addressLayout->addRow(QString::fromUtf8("电表地址"), m_meterAddressEdit);
    mainLayout->addLayout(addressLayout);

    QTabWidget *toolTabs = new QTabWidget(this);

    QWidget *passwordTab = new QWidget(toolTabs);
    QVBoxLayout *passwordLayout = new QVBoxLayout(passwordTab);
    passwordLayout->setContentsMargins(14, 16, 14, 14);
    passwordLayout->setSpacing(10);

    QFormLayout *passwordFormLayout = new QFormLayout();
    passwordFormLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    passwordFormLayout->setHorizontalSpacing(10);
    passwordFormLayout->setVerticalSpacing(10);

    m_keyEdit = new QLineEdit(passwordTab);
    m_keyEdit->setPlaceholderText(QString::fromUtf8("请输入密钥"));
    m_keyEdit->setEchoMode(QLineEdit::Password);
    m_keyEdit->setText(QStringLiteral("jqandd"));
    passwordFormLayout->addRow(QString::fromUtf8("密钥"), m_keyEdit);

    m_passwordEdit = new QLineEdit(passwordTab);
    m_passwordEdit->setObjectName(QStringLiteral("meterPasswordResult"));
    m_passwordEdit->setReadOnly(true);
    passwordFormLayout->addRow(QString::fromUtf8("生成密码"), m_passwordEdit);
    passwordLayout->addLayout(passwordFormLayout);

    QHBoxLayout *passwordButtonLayout = new QHBoxLayout();
    passwordButtonLayout->addStretch();
    m_generateButton = new QPushButton(QString::fromUtf8("生成密码"), passwordTab);
    m_generateButton->setDefault(true);
    passwordButtonLayout->addWidget(m_generateButton);
    m_copyPasswordButton = new QPushButton(QString::fromUtf8("复制"), passwordTab);
    m_copyPasswordButton->setEnabled(false);
    passwordButtonLayout->addWidget(m_copyPasswordButton);
    passwordLayout->addLayout(passwordButtonLayout);
    passwordLayout->addStretch();
    toolTabs->addTab(passwordTab, QString::fromUtf8("密码生成"));

    QWidget *authenticationTab = new QWidget(toolTabs);
    QVBoxLayout *authenticationLayout = new QVBoxLayout(authenticationTab);
    authenticationLayout->setContentsMargins(14, 16, 14, 14);
    authenticationLayout->setSpacing(10);

    QHBoxLayout *authenticationStateLayout = new QHBoxLayout();
    QLabel *authenticationStateLabel = new QLabel(QString::fromUtf8("认证状态"), authenticationTab);
    authenticationStateLayout->addWidget(authenticationStateLabel);
    m_enableAuthenticationButton = new QRadioButton(QString::fromUtf8("开启认证"), authenticationTab);
    m_enableAuthenticationButton->setChecked(true);
    authenticationStateLayout->addWidget(m_enableAuthenticationButton);
    m_disableAuthenticationButton = new QRadioButton(QString::fromUtf8("关闭认证"), authenticationTab);
    authenticationStateLayout->addWidget(m_disableAuthenticationButton);
    authenticationStateLayout->addStretch();
    authenticationLayout->addLayout(authenticationStateLayout);

    QFormLayout *authenticationFormLayout = new QFormLayout();
    authenticationFormLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    authenticationFormLayout->setHorizontalSpacing(10);
    authenticationFormLayout->setVerticalSpacing(10);

    m_authenticationFrameEdit = new QLineEdit(authenticationTab);
    m_authenticationFrameEdit->setObjectName(QStringLiteral("authenticationFrameResult"));
    m_authenticationFrameEdit->setReadOnly(true);
    m_authenticationFrameEdit->setPlaceholderText(QString::fromUtf8("生成后显示发送报文"));
    authenticationFormLayout->addRow(QString::fromUtf8("发送报文"), m_authenticationFrameEdit);

    m_authenticationCompactFrameEdit = new QLineEdit(authenticationTab);
    m_authenticationCompactFrameEdit->setObjectName(QStringLiteral("authenticationCompactFrameResult"));
    m_authenticationCompactFrameEdit->setReadOnly(true);
    m_authenticationCompactFrameEdit->setPlaceholderText(QString::fromUtf8("生成后显示无空格报文"));
    authenticationFormLayout->addRow(QString::fromUtf8("紧凑报文"),
                                     m_authenticationCompactFrameEdit);

    authenticationLayout->addLayout(authenticationFormLayout);

    QHBoxLayout *authenticationButtonLayout = new QHBoxLayout();
    authenticationButtonLayout->addStretch();
    m_generateAuthenticationFrameButton = new QPushButton(QString::fromUtf8("生成报文"), authenticationTab);
    authenticationButtonLayout->addWidget(m_generateAuthenticationFrameButton);
    authenticationLayout->addLayout(authenticationButtonLayout);
    authenticationLayout->addStretch();
    toolTabs->addTab(authenticationTab, QString::fromUtf8("认证状态"));

    QWidget *meterAuthenticationTab = new QWidget(toolTabs);
    QVBoxLayout *meterAuthenticationLayout = new QVBoxLayout(meterAuthenticationTab);
    meterAuthenticationLayout->setContentsMargins(14, 16, 14, 14);
    meterAuthenticationLayout->setSpacing(10);

    QFormLayout *meterAuthenticationFormLayout = new QFormLayout();
    meterAuthenticationFormLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    meterAuthenticationFormLayout->setHorizontalSpacing(10);
    meterAuthenticationFormLayout->setVerticalSpacing(10);

    m_meterAuthenticationPasswordEdit = new QLineEdit(meterAuthenticationTab);
    m_meterAuthenticationPasswordEdit->setPlaceholderText(QString::fromUtf8("02 级六位密码"));
    m_meterAuthenticationPasswordEdit->setMaxLength(6);
    m_meterAuthenticationPasswordEdit->setValidator(
        new QRegularExpressionValidator(QRegularExpression(QStringLiteral("\\d{0,6}")),
                                        m_meterAuthenticationPasswordEdit));
    meterAuthenticationFormLayout->addRow(QString::fromUtf8("02 级密码"),
                                          m_meterAuthenticationPasswordEdit);

    m_operatorCodeEdit = new QLineEdit(QStringLiteral("00000000"), meterAuthenticationTab);
    m_operatorCodeEdit->setObjectName(QStringLiteral("meterAuthenticationOperatorCode"));
    m_operatorCodeEdit->setReadOnly(true);
    meterAuthenticationFormLayout->addRow(QString::fromUtf8("操作者代码"), m_operatorCodeEdit);

    QDateTime baselineTime = QDateTime::currentDateTime();
    baselineTime.setTime(QTime(baselineTime.time().hour(), baselineTime.time().minute()));
    const QDateTime deadline = baselineTime.addDays(1);
    const QDateTime minimumDateTime(QDate(2000, 1, 1), QTime(0, 0));
    const QDateTime maximumDateTime(QDate(2099, 12, 31), QTime(23, 59));

    m_deadlineEdit = new QDateTimeEdit(deadline, meterAuthenticationTab);
    m_deadlineEdit->setCalendarPopup(true);
    m_deadlineEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd HH:mm"));
    m_deadlineEdit->setMinimumDateTime(minimumDateTime);
    m_deadlineEdit->setMaximumDateTime(maximumDateTime);
    meterAuthenticationFormLayout->addRow(QString::fromUtf8("截止时间"), m_deadlineEdit);

    m_baselineTimeEdit = new QDateTimeEdit(baselineTime, meterAuthenticationTab);
    m_baselineTimeEdit->setCalendarPopup(true);
    m_baselineTimeEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd HH:mm"));
    m_baselineTimeEdit->setMinimumDateTime(minimumDateTime);
    m_baselineTimeEdit->setMaximumDateTime(maximumDateTime);
    meterAuthenticationFormLayout->addRow(QString::fromUtf8("基准时间"), m_baselineTimeEdit);

    m_meterAuthenticationFrameEdit = new QPlainTextEdit(meterAuthenticationTab);
    m_meterAuthenticationFrameEdit->setObjectName(QStringLiteral("meterAuthenticationFrameResult"));
    m_meterAuthenticationFrameEdit->setReadOnly(true);
    m_meterAuthenticationFrameEdit->setPlaceholderText(
        QString::fromUtf8("生成后显示 40 字节认证命令（含前导码）"));
    m_meterAuthenticationFrameEdit->setMaximumHeight(70);
    meterAuthenticationFormLayout->addRow(QString::fromUtf8("认证命令"),
                                          m_meterAuthenticationFrameEdit);

    m_meterAuthenticationCompactFrameEdit = new QPlainTextEdit(meterAuthenticationTab);
    m_meterAuthenticationCompactFrameEdit->setObjectName(
        QStringLiteral("meterAuthenticationCompactFrameResult"));
    m_meterAuthenticationCompactFrameEdit->setReadOnly(true);
    m_meterAuthenticationCompactFrameEdit->setPlaceholderText(
        QString::fromUtf8("生成后显示无空格认证命令"));
    m_meterAuthenticationCompactFrameEdit->setMaximumHeight(55);
    meterAuthenticationFormLayout->addRow(QString::fromUtf8("紧凑报文"),
                                          m_meterAuthenticationCompactFrameEdit);
    meterAuthenticationLayout->addLayout(meterAuthenticationFormLayout);

    QHBoxLayout *meterAuthenticationButtonLayout = new QHBoxLayout();
    meterAuthenticationButtonLayout->addStretch();
    m_generateMeterAuthenticationButton = new QPushButton(QString::fromUtf8("生成命令"),
                                                           meterAuthenticationTab);
    meterAuthenticationButtonLayout->addWidget(m_generateMeterAuthenticationButton);
    meterAuthenticationLayout->addLayout(meterAuthenticationButtonLayout);
    meterAuthenticationLayout->addStretch();
    toolTabs->addTab(meterAuthenticationTab, QString::fromUtf8("电表认证"));

    mainLayout->addWidget(toolTabs);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setObjectName(QStringLiteral("encryptedMeterToolStatus"));
    m_statusLabel->setMinimumHeight(20);
    mainLayout->addWidget(m_statusLabel);

    connect(m_generateButton, &QPushButton::clicked, this, &MeterPasswordDialog::onGenerateClicked);
    connect(m_copyPasswordButton, &QPushButton::clicked, this, &MeterPasswordDialog::onCopyPasswordClicked);
    connect(m_generateAuthenticationFrameButton,
            &QPushButton::clicked,
            this,
            &MeterPasswordDialog::onAuthenticationGenerateClicked);
    connect(m_generateMeterAuthenticationButton,
            &QPushButton::clicked,
            this,
            &MeterPasswordDialog::onMeterAuthenticationGenerateClicked);
    connect(m_meterAddressEdit, &QLineEdit::textChanged, this, &MeterPasswordDialog::clearGeneratedPassword);
    connect(m_meterAddressEdit, &QLineEdit::textChanged, this, &MeterPasswordDialog::clearAuthenticationFrame);
    connect(m_meterAddressEdit,
            &QLineEdit::textChanged,
            this,
            &MeterPasswordDialog::clearMeterAuthenticationFrame);
    connect(m_keyEdit, &QLineEdit::textChanged, this, &MeterPasswordDialog::clearGeneratedPassword);
    connect(m_meterAuthenticationPasswordEdit,
            &QLineEdit::textChanged,
            this,
            &MeterPasswordDialog::clearMeterAuthenticationFrame);
    connect(m_deadlineEdit,
            &QDateTimeEdit::dateTimeChanged,
            this,
            &MeterPasswordDialog::clearMeterAuthenticationFrame);
    connect(m_baselineTimeEdit,
            &QDateTimeEdit::dateTimeChanged,
            this,
            &MeterPasswordDialog::clearMeterAuthenticationFrame);
    connect(m_enableAuthenticationButton,
            &QRadioButton::toggled,
            this,
            &MeterPasswordDialog::clearAuthenticationFrame);
}

void MeterPasswordDialog::applyTheme(const ui::ThemePalette &palette)
{
    setStyleSheet(ui::applyThemeTokens(encryptedMeterToolStyleTemplate(), palette));
}
