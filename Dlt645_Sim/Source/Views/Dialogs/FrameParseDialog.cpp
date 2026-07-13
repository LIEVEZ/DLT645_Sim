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
      m_inputEdit(nullptr),
      m_resultWidget(nullptr),
      m_statusLabel(nullptr),
      m_clearButton(nullptr)
{
    setWindowFlags(windowFlags() | Qt::WindowMinimizeButtonHint);
    setupUI();
    applyDarkTheme();

    setWindowTitle(QString::fromUtf8("DLT645 帧解析"));
    resize(860, 640);
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
    m_inputEdit->setPlainText(normalized);
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

    if (m_resultWidget)
    {
        m_resultWidget->applyDarkTheme();
    }
}

void FrameParseDialog::applyLightTheme()
{
    setStyleSheet(ui::applyThemeTokens(frameParseStyleTemplate(), ui::lightPalette()));

    if (m_resultWidget)
    {
        m_resultWidget->applyLightTheme();
    }
}

void FrameParseDialog::onInputTextChanged()
{
    if (m_inputEdit->toPlainText().trimmed().isEmpty())
    {
        m_statusLabel->clear();
        m_resultWidget->clearResult();
        return;
    }

    parseCurrentInput();
}

void FrameParseDialog::onClearClicked()
{
    m_inputEdit->clear();
    m_resultWidget->clearResult();
    m_statusLabel->clear();
    m_inputEdit->setFocus();
}

void FrameParseDialog::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(10);

    QGroupBox *inputGroup = new QGroupBox(QString::fromUtf8("解析输入"), this);
    QVBoxLayout *inputLayout = new QVBoxLayout(inputGroup);
    inputLayout->setSpacing(10);

    QLabel *inputHintLabel = new QLabel(QString::fromUtf8("HEX 输入"), inputGroup);
    inputLayout->addWidget(inputHintLabel);

    m_inputEdit = new QTextEdit(inputGroup);
    m_inputEdit->setPlaceholderText(QString::fromUtf8("支持带空格 HEX、紧凑 HEX、换行或制表符分隔的 HEX 帧"));
    m_inputEdit->setFont(QFont(QStringLiteral("Microsoft YaHei UI"), 10));
    m_inputEdit->setAcceptRichText(false);
    m_inputEdit->setFixedHeight(52);
    inputLayout->addWidget(m_inputEdit, 1);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    m_clearButton = new QPushButton(QString::fromUtf8("清空"), inputGroup);
    buttonLayout->addWidget(m_clearButton);
    buttonLayout->addStretch();
    inputLayout->addLayout(buttonLayout);

    m_statusLabel = new QLabel(inputGroup);
    m_statusLabel->setWordWrap(true);
    inputLayout->addWidget(m_statusLabel);

    inputGroup->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    mainLayout->addWidget(inputGroup, 1);

    QGroupBox *resultGroup = new QGroupBox(QString::fromUtf8("解析结果"), this);
    QVBoxLayout *resultLayout = new QVBoxLayout(resultGroup);

    m_resultWidget = new FrameParseResultWidget(resultGroup);
    resultLayout->addWidget(m_resultWidget);

    mainLayout->addWidget(resultGroup, 2);

    connect(m_inputEdit, &QTextEdit::textChanged, this, &FrameParseDialog::onInputTextChanged);
    connect(m_clearButton, &QPushButton::clicked, this, &FrameParseDialog::onClearClicked);
}

bool FrameParseDialog::parseCurrentInput()
{
    QString errorMessage;
    const QString normalizedHex = normalizedHexInput(&errorMessage);
    if (!errorMessage.isEmpty())
    {
        m_statusLabel->setText(errorMessage);
        m_resultWidget->clearResult();
        return false;
    }

    const QByteArray frameBytes = QByteArray::fromHex(normalizedHex.toLatin1());
    m_resultWidget->setParseResult(DLT645FrameFormatter::buildParseDisplayResult(frameBytes, activeMeterModel()));
    m_statusLabel->setText(QString::fromUtf8("自动解析完成。"));
    return true;
}

QString FrameParseDialog::normalizedHexInput(QString *errorMessage) const
{
    if (errorMessage)
    {
        errorMessage->clear();
    }

    const QString rawInput = m_inputEdit->toPlainText();
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
