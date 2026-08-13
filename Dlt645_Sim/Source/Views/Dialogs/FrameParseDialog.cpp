#include "FrameParseDialog.h"

#include "Models/MeterModel.h"
#include "Utils/DLT645FrameFormatter.h"
#include "Utils/UiTheme.h"
#include "Views/Common/FrameParseResultWidget.h"

#include <QFont>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRegularExpression>
#include <QTextEdit>
#include <QVBoxLayout>

FrameParseDialog::FrameParseDialog(QWidget *parent)
    : QDialog(parent),
      m_meterModel(nullptr),
      m_catalogMeterModel(new MeterModel(this)),
      m_sendInputEdit(nullptr),
      m_receiveInputEdit(nullptr),
      m_sendResultWidget(nullptr),
      m_receiveResultWidget(nullptr),
      m_sendStatusLabel(nullptr),
      m_receiveStatusLabel(nullptr),
      m_sendClearButton(nullptr),
      m_receiveClearButton(nullptr)
{
    setWindowFlags(windowFlags() | Qt::WindowMinimizeButtonHint);
    setupUI();
    applyDarkTheme();

    setWindowTitle(QString::fromUtf8("DLT645 帧解析"));
    setMinimumSize(960, 600);
    resize(1200, 720);
}

FrameParseDialog::~FrameParseDialog()
{
}

void FrameParseDialog::setMeterModel(MeterModel *meterModel)
{
    m_meterModel = meterModel;
}

void FrameParseDialog::setFrameHex(const QString &frameHex)
{
    const QString normalized = frameHex.trimmed().toUpper();
    m_sendInputEdit->setPlainText(normalized);
}

namespace
{
// 帧解析对话框 QSS 模板。占位符由全局调色板（Utils/UiTheme）替换。
QString frameParseStyleTemplate()
{
    return QStringLiteral(R"(
        QDialog {
            background-color: {{windowBg}};
        }

        QGroupBox {
            color: {{text}};
            border: 1px solid {{border}};
            border-radius: 8px;
            margin-top: 10px;
            padding-top: 10px;
        }

        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 4px;
            color: {{accentTitle}};
        }

        QLabel {
            color: {{text}};
            font-family: "Microsoft YaHei UI";
        }

        QTextEdit {
            background-color: {{panelBg}};
            color: {{text}};
            border: 1px solid {{border}};
            border-radius: 4px;
            padding: 8px;
        }

        QTextEdit:focus {
            border: 1px solid {{accent}};
        }

        {{buttonBlock}}
    )");
}
} // namespace

void FrameParseDialog::applyDarkTheme()
{
    setStyleSheet(ui::applyThemeTokens(frameParseStyleTemplate(), ui::darkPalette()));

    if (m_sendResultWidget)
    {
        m_sendResultWidget->applyDarkTheme();
    }
    if (m_receiveResultWidget)
    {
        m_receiveResultWidget->applyDarkTheme();
    }
}

void FrameParseDialog::applyLightTheme()
{
    setStyleSheet(ui::applyThemeTokens(frameParseStyleTemplate(), ui::lightPalette()));

    if (m_sendResultWidget)
    {
        m_sendResultWidget->applyLightTheme();
    }
    if (m_receiveResultWidget)
    {
        m_receiveResultWidget->applyLightTheme();
    }
}

void FrameParseDialog::onSendInputTextChanged()
{
    parseInput(m_sendInputEdit, m_sendStatusLabel, m_sendResultWidget);
}

void FrameParseDialog::onReceiveInputTextChanged()
{
    parseInput(m_receiveInputEdit, m_receiveStatusLabel, m_receiveResultWidget);
}

void FrameParseDialog::onSendClearClicked()
{
    clearPanel(m_sendInputEdit, m_sendStatusLabel, m_sendResultWidget);
}

void FrameParseDialog::onReceiveClearClicked()
{
    clearPanel(m_receiveInputEdit, m_receiveStatusLabel, m_receiveResultWidget);
}

void FrameParseDialog::setupUI()
{
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(12);

    QGroupBox *sendPanel = createParsePanel(QString::fromUtf8("发送解析"),
                                             &m_sendInputEdit,
                                             &m_sendResultWidget,
                                             &m_sendStatusLabel,
                                             &m_sendClearButton);
    QGroupBox *receivePanel = createParsePanel(QString::fromUtf8("接收解析"),
                                                &m_receiveInputEdit,
                                                &m_receiveResultWidget,
                                                &m_receiveStatusLabel,
                                                &m_receiveClearButton);
    mainLayout->addWidget(sendPanel, 1);
    mainLayout->addWidget(receivePanel, 1);

    connect(m_sendInputEdit, &QTextEdit::textChanged,
            this, &FrameParseDialog::onSendInputTextChanged);
    connect(m_receiveInputEdit, &QTextEdit::textChanged,
            this, &FrameParseDialog::onReceiveInputTextChanged);
    connect(m_sendClearButton, &QPushButton::clicked,
            this, &FrameParseDialog::onSendClearClicked);
    connect(m_receiveClearButton, &QPushButton::clicked,
            this, &FrameParseDialog::onReceiveClearClicked);
}

QGroupBox *FrameParseDialog::createParsePanel(const QString &title,
                                               QTextEdit **inputEdit,
                                               FrameParseResultWidget **resultWidget,
                                               QLabel **statusLabel,
                                               QPushButton **clearButton)
{
    QGroupBox *panel = new QGroupBox(title, this);
    QVBoxLayout *panelLayout = new QVBoxLayout(panel);
    panelLayout->setSpacing(10);

    QLabel *inputHintLabel = new QLabel(QString::fromUtf8("HEX 输入"), panel);
    panelLayout->addWidget(inputHintLabel);

    *inputEdit = new QTextEdit(panel);
    (*inputEdit)->setPlaceholderText(
        QString::fromUtf8("支持带空格 HEX、紧凑 HEX、换行或制表符分隔的 HEX 帧"));
    (*inputEdit)->setFont(QFont(QStringLiteral("Microsoft YaHei UI"), 10));
    (*inputEdit)->setAcceptRichText(false);
    (*inputEdit)->setFixedHeight(64);
    panelLayout->addWidget(*inputEdit);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    *clearButton = new QPushButton(QString::fromUtf8("清空"), panel);
    buttonLayout->addWidget(*clearButton);
    buttonLayout->addStretch();
    panelLayout->addLayout(buttonLayout);

    *statusLabel = new QLabel(panel);
    (*statusLabel)->setWordWrap(true);
    (*statusLabel)->setMinimumHeight((*statusLabel)->fontMetrics().lineSpacing());
    panelLayout->addWidget(*statusLabel);

    QLabel *resultHintLabel = new QLabel(QString::fromUtf8("解析结果"), panel);
    panelLayout->addWidget(resultHintLabel);

    *resultWidget = new FrameParseResultWidget(panel);
    panelLayout->addWidget(*resultWidget, 1);

    return panel;
}

bool FrameParseDialog::parseInput(QTextEdit *inputEdit,
                                  QLabel *statusLabel,
                                  FrameParseResultWidget *resultWidget)
{
    if (inputEdit->toPlainText().trimmed().isEmpty())
    {
        statusLabel->clear();
        resultWidget->clearResult();
        return false;
    }

    QString errorMessage;
    const QString normalizedHex = normalizedHexInput(inputEdit, &errorMessage);
    if (!errorMessage.isEmpty())
    {
        statusLabel->setText(errorMessage);
        resultWidget->clearResult();
        return false;
    }

    const QByteArray frameBytes = QByteArray::fromHex(normalizedHex.toLatin1());
    resultWidget->setParseResult(
        DLT645FrameFormatter::buildParseDisplayResult(frameBytes, activeMeterModel()));
    statusLabel->setText(QString::fromUtf8("自动解析完成。"));
    return true;
}

void FrameParseDialog::clearPanel(QTextEdit *inputEdit,
                                  QLabel *statusLabel,
                                  FrameParseResultWidget *resultWidget)
{
    inputEdit->clear();
    resultWidget->clearResult();
    statusLabel->clear();
    inputEdit->setFocus();
}

QString FrameParseDialog::normalizedHexInput(const QTextEdit *inputEdit,
                                             QString *errorMessage) const
{
    if (errorMessage)
    {
        errorMessage->clear();
    }

    const QString rawInput = inputEdit->toPlainText();
    if (rawInput.trimmed().isEmpty())
    {
        return QString();
    }

    QString normalized = rawInput.toUpper();
    normalized.remove(QRegularExpression(QStringLiteral("\\s+")));

    if (normalized.isEmpty())
    {
        return QString();
    }

    if (normalized.size() % 2 != 0)
    {
        if (errorMessage)
        {
            *errorMessage = QString::fromUtf8("HEX 字符数量必须为偶数。");
        }
        return QString();
    }

    if (normalized.contains(QRegularExpression(QStringLiteral("[^0-9A-F]"))))
    {
        if (errorMessage)
        {
            *errorMessage = QString::fromUtf8("输入包含非十六进制字符，请检查后重试。");
        }
        return QString();
    }

    if (normalized.size() < 24)
    {
        if (errorMessage)
        {
            *errorMessage = QString::fromUtf8("长度不足，无法组成完整的 DLT645 帧。");
        }
        return QString();
    }

    return normalized;
}

const MeterModel *FrameParseDialog::activeMeterModel() const
{
    return m_meterModel ? m_meterModel : m_catalogMeterModel;
}
