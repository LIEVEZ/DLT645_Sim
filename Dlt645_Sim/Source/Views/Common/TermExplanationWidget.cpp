#include "TermExplanationWidget.h"
#include "Models/TermDefinition.h"

#include <QVBoxLayout>
#include <QTextEdit>
#include <QLabel>
#include <QSizePolicy>

TermExplanationWidget::TermExplanationWidget(QWidget *parent)
    : QWidget(parent),
      m_titleLabel(nullptr),
      m_contentEdit(nullptr),
      m_isEmpty(true),
      m_darkTheme(true)
{
    setupUI();
    showWelcomeMessage();  // 显示欢迎提示
}

void TermExplanationWidget::setupUI()
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 5, 0, 0);  // 减小顶部边距
    mainLayout->setSpacing(2);  // 进一步缩小间距到 2px

    // 标题
    m_titleLabel = new QLabel(QString::fromUtf8("📖 术语解释"), this);
    m_titleLabel->setStyleSheet("font-weight: bold; font-size: 13px;");
    m_titleLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    m_titleLabel->setFixedHeight(22);
    mainLayout->addWidget(m_titleLabel);

    // 内容区域
    m_contentEdit = new QTextEdit(this);
    m_contentEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_contentEdit->setReadOnly(true);
    m_contentEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_contentEdit->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_contentEdit->setMinimumHeight(120);

    // 减少文档边距
    m_contentEdit->document()->setDocumentMargin(2);

    mainLayout->addWidget(m_contentEdit, 1);

    setLayout(mainLayout);
}

void TermExplanationWidget::showTerm(const TermDefinition &term)
{
    if (term.term.isEmpty()) {
        showWelcomeMessage();
        return;
    }

    // 生成 HTML 内容
    QString html = term.toHtml();
    m_contentEdit->setHtml(html);

    // 显示组件
    m_isEmpty = false;
}

void TermExplanationWidget::clear()
{
    showWelcomeMessage();
}

void TermExplanationWidget::showWelcomeMessage()
{
    QString html = QString::fromUtf8(
        "<p style='color:#888888; font-size:12px; margin-top:20px; text-align:center;'>"
        "💡 在上方搜索框输入关键词（如“功率因数”、“DI”、“电能”），可查看相关术语解释。"
        "</p>"
    );
    m_contentEdit->setHtml(html);
    m_isEmpty = true;
}

bool TermExplanationWidget::isEmpty() const
{
    return m_isEmpty;
}

void TermExplanationWidget::applyDarkTheme()
{
    m_darkTheme = true;

    m_titleLabel->setStyleSheet(R"(
        QLabel {
            color: #FFFFFF;
            font-weight: bold;
            font-size: 13px;
            background-color: transparent;
        }
    )");

    m_contentEdit->setStyleSheet(R"(
        QTextEdit {
            background-color: #2B2B2B;
            color: #CCCCCC;
            border: 1px solid #555555;
            border-radius: 3px;
            padding: 6px;
            font-family: "Microsoft YaHei";
            font-size: 12px;
        }

        QScrollBar:vertical {
            background-color: #2B2B2B;
            width: 12px;
            border: none;
        }

        QScrollBar::handle:vertical {
            background-color: #5A5A5A;
            border-radius: 6px;
            min-height: 20px;
        }

        QScrollBar::handle:vertical:hover {
            background-color: #6A6A6A;
        }

        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }
    )");
}

void TermExplanationWidget::applyLightTheme()
{
    m_darkTheme = false;

    m_titleLabel->setStyleSheet(R"(
        QLabel {
            color: #000000;
            font-weight: bold;
            font-size: 13px;
            background-color: transparent;
        }
    )");

    m_contentEdit->setStyleSheet(R"(
        QTextEdit {
            background-color: #F5F5F5;
            color: #333333;
            border: 1px solid #CCCCCC;
            border-radius: 3px;
            padding: 6px;
            font-family: "Microsoft YaHei";
            font-size: 12px;
        }

        QScrollBar:vertical {
            background-color: #F5F5F5;
            width: 12px;
            border: none;
        }

        QScrollBar::handle:vertical {
            background-color: #CCCCCC;
            border-radius: 6px;
            min-height: 20px;
        }

        QScrollBar::handle:vertical:hover {
            background-color: #BBBBBB;
        }

        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }
    )");
}
