#include "FrameParseDialog.h"

#include "Models/MeterModel.h"
#include "Utils/DLT645FrameFormatter.h"
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

void FrameParseDialog::applyDarkTheme()
{
    setStyleSheet(R"(
        QDialog {
            background-color: #2B2B2B;
        }

        QGroupBox {
            color: #FFFFFF;
            border: 1px solid #555555;
            border-radius: 4px;
            margin-top: 10px;
            padding-top: 10px;
        }

        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 4px;
        }

        QLabel {
            color: #FFFFFF;
            font-family: "Microsoft YaHei";
        }

        QTextEdit {
            background-color: #3C3F41;
            color: #FFFFFF;
            border: 1px solid #555555;
            border-radius: 3px;
            padding: 8px;
        }

        QTextEdit:focus {
            border: 1px solid #094771;
        }

        QPushButton {
            background-color: #3C3F41;
            color: #FFFFFF;
            border: 1px solid #555555;
            border-radius: 3px;
            padding: 6px 14px;
        }

        QPushButton:hover {
            background-color: #4E5254;
        }
    )");

    if (m_resultWidget)
    {
        m_resultWidget->applyDarkTheme();
    }
}

void FrameParseDialog::applyLightTheme()
{
    setStyleSheet(R"(
        QDialog {
            background-color: #FFFFFF;
        }

        QGroupBox {
            color: #000000;
            border: 1px solid #CCCCCC;
            border-radius: 4px;
            margin-top: 10px;
            padding-top: 10px;
        }

        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 4px;
        }

        QLabel {
            color: #000000;
            font-family: "Microsoft YaHei";
        }

        QTextEdit {
            background-color: #FFFFFF;
            color: #000000;
            border: 1px solid #CCCCCC;
            border-radius: 3px;
            padding: 8px;
        }

        QTextEdit:focus {
            border: 1px solid #0078D7;
        }

        QPushButton {
            background-color: #F0F0F0;
            color: #000000;
            border: 1px solid #CCCCCC;
            border-radius: 3px;
            padding: 6px 14px;
        }

        QPushButton:hover {
            background-color: #E0E0E0;
        }
    )");

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
    m_inputEdit->setFont(QFont(QStringLiteral("Consolas"), 10));
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
