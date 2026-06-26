#include "FrameParseResultWidget.h"

#include "Utils/UiTheme.h"

#include <QFont>
#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QLayoutItem>
#include <QScrollArea>
#include <QScrollBar>
#include <QStyle>
#include <QPalette>
#include <QVariant>
#include <QVBoxLayout>

namespace
{
QFont monospaceFont(int pointSize)
{
    return QFont(QStringLiteral("Consolas"), pointSize);
}

void applyBackgroundPalette(QWidget *widget, const QColor &backgroundColor)
{
    if (!widget)
    {
        return;
    }

    QPalette palette = widget->palette();
    palette.setColor(QPalette::Window, backgroundColor);
    palette.setColor(QPalette::Base, backgroundColor);
    widget->setPalette(palette);
    widget->setAutoFillBackground(true);
}
}

FrameParseResultWidget::FrameParseResultWidget(QWidget *parent)
    : QWidget(parent),
      m_noticeLabel(nullptr),
      m_scrollArea(nullptr),
      m_contentWidget(nullptr),
      m_sectionsHost(nullptr),
      m_sectionsLayout(nullptr),
      m_darkTheme(true)
{
    setObjectName(QStringLiteral("frameParseResultWidget"));
    setAttribute(Qt::WA_StyledBackground, true);
    setAutoFillBackground(true);

    QVBoxLayout *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setObjectName(QStringLiteral("frameParseScroll"));
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_contentWidget = new QWidget(m_scrollArea);
    m_contentWidget->setObjectName(QStringLiteral("frameParseContent"));
    m_contentWidget->setAttribute(Qt::WA_StyledBackground, true);
    m_contentWidget->setAutoFillBackground(true);

    QVBoxLayout *contentLayout = new QVBoxLayout(m_contentWidget);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(12);

    m_noticeLabel = new QLabel(m_contentWidget);
    m_noticeLabel->setObjectName(QStringLiteral("frameParseBanner"));
    m_noticeLabel->setWordWrap(true);
    m_noticeLabel->setVisible(false);
    contentLayout->addWidget(m_noticeLabel);

    m_sectionsHost = new QWidget(m_contentWidget);
    m_sectionsHost->setObjectName(QStringLiteral("frameParseSections"));
    m_sectionsHost->setAttribute(Qt::WA_StyledBackground, true);
    m_sectionsHost->setAutoFillBackground(true);

    m_sectionsLayout = new QVBoxLayout(m_sectionsHost);
    m_sectionsLayout->setContentsMargins(0, 0, 0, 0);
    m_sectionsLayout->setSpacing(14);
    contentLayout->addWidget(m_sectionsHost);

    m_scrollArea->setWidget(m_contentWidget);
    rootLayout->addWidget(m_scrollArea);

    applyCurrentTheme();
}

void FrameParseResultWidget::clearResult()
{
    m_result = FrameParseDisplayResult();
    rebuildSections();
}

void FrameParseResultWidget::setParseResult(const FrameParseDisplayResult &result)
{
    m_result = result;
    rebuildSections();
}

void FrameParseResultWidget::applyDarkTheme()
{
    m_darkTheme = true;
    applyCurrentTheme();
}

void FrameParseResultWidget::applyLightTheme()
{
    m_darkTheme = false;
    applyCurrentTheme();
}

QWidget *FrameParseResultWidget::createSectionWidget(const FrameParseSection &section) const
{
    QWidget *sectionWidget = new QWidget(m_sectionsHost);
    sectionWidget->setObjectName(QStringLiteral("frameParseSection"));
    sectionWidget->setAttribute(Qt::WA_StyledBackground, true);

    QVBoxLayout *sectionLayout = new QVBoxLayout(sectionWidget);
    sectionLayout->setContentsMargins(0, 0, 0, 0);
    sectionLayout->setSpacing(8);

    const bool showSectionTitle = !section.title.isEmpty()
                                  && !(section.cards.size() == 1 && section.cards.first().title == section.title);
    if (showSectionTitle)
    {
        QLabel *titleLabel = new QLabel(section.title, sectionWidget);
        titleLabel->setObjectName(QStringLiteral("frameParseSectionTitle"));
        titleLabel->setWordWrap(true);
        sectionLayout->addWidget(titleLabel);
    }

    if (section.cards.isEmpty())
    {
        QLabel *placeholderLabel = new QLabel(section.placeholderText.isEmpty() ? QString::fromUtf8("暂无内容。") : section.placeholderText, sectionWidget);
        placeholderLabel->setObjectName(QStringLiteral("frameParsePlaceholder"));
        placeholderLabel->setWordWrap(true);
        sectionLayout->addWidget(placeholderLabel);
        return sectionWidget;
    }

    QWidget *gridHost = new QWidget(sectionWidget);
    gridHost->setObjectName(QStringLiteral("frameParseCardGrid"));
    gridHost->setAttribute(Qt::WA_StyledBackground, true);

    QGridLayout *gridLayout = new QGridLayout(gridHost);
    gridLayout->setContentsMargins(0, 0, 0, 0);
    gridLayout->setHorizontalSpacing(10);
    gridLayout->setVerticalSpacing(10);
    gridLayout->setColumnStretch(0, 1);
    gridLayout->setColumnStretch(1, 1);

    for (int index = 0; index < section.cards.size(); ++index)
    {
        QWidget *cardWidget = createCardWidget(section.cards.at(index));
        const int row = index / 2;
        const int column = index % 2;
        gridLayout->addWidget(cardWidget, row, column);
    }

    sectionLayout->addWidget(gridHost);
    return sectionWidget;
}

QWidget *FrameParseResultWidget::createCardWidget(const FrameParseCard &card) const
{
    QFrame *cardFrame = new QFrame(m_sectionsHost);
    cardFrame->setObjectName(QStringLiteral("frameParseCard"));
    cardFrame->setFrameShape(QFrame::StyledPanel);
    cardFrame->setAttribute(Qt::WA_StyledBackground, true);
    cardFrame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    QVBoxLayout *cardLayout = new QVBoxLayout(cardFrame);
    cardLayout->setContentsMargins(12, 10, 12, 10);
    cardLayout->setSpacing(6);

    QLabel *titleLabel = new QLabel(card.title, cardFrame);
    titleLabel->setObjectName(QStringLiteral("frameParseCardTitle"));
    titleLabel->setWordWrap(true);
    cardLayout->addWidget(titleLabel);

    QLabel *valueLabel = new QLabel(card.value.isEmpty() ? QString::fromUtf8("无") : card.value, cardFrame);
    valueLabel->setObjectName(QStringLiteral("frameParseCardValue"));
    valueLabel->setWordWrap(true);
    valueLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    if (card.valueMonospace)
    {
        valueLabel->setFont(monospaceFont(10));
    }
    cardLayout->addWidget(valueLabel);

    if (!card.detail.isEmpty())
    {
        QLabel *detailLabel = new QLabel(card.detail, cardFrame);
        detailLabel->setObjectName(QStringLiteral("frameParseCardDetail"));
        detailLabel->setWordWrap(true);
        detailLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        if (card.detailMonospace)
        {
            detailLabel->setFont(monospaceFont(9));
        }
        cardLayout->addWidget(detailLabel);
    }

    return cardFrame;
}

void FrameParseResultWidget::rebuildSections()
{
    clearSectionWidgets();
    updateBanner();

    for (const FrameParseSection &section : m_result.sections)
    {
        m_sectionsLayout->addWidget(createSectionWidget(section));
    }
    m_sectionsLayout->addStretch();

    if (m_scrollArea && m_scrollArea->verticalScrollBar())
    {
        m_scrollArea->verticalScrollBar()->setValue(0);
    }
}

void FrameParseResultWidget::clearSectionWidgets()
{
    while (QLayoutItem *item = m_sectionsLayout->takeAt(0))
    {
        if (QWidget *widget = item->widget())
        {
            widget->deleteLater();
        }
        delete item;
    }
}

void FrameParseResultWidget::applyCurrentTheme()
{
    const ui::ThemePalette p = m_darkTheme ? ui::darkPalette() : ui::lightPalette();
    const QColor backgroundColor(p.windowBg);

    // 报文解析结果区 QSS 模板。banner 的 notice/error、卡片标题等语义色取自全局调色板。
    const QString styleTemplate = QStringLiteral(R"(
        QWidget#frameParseResultWidget,
        QWidget#frameParseContent,
        QWidget#frameParseSections,
        QScrollArea#frameParseScroll,
        QWidget#frameParseSection,
        QWidget#frameParseCardGrid {
            background-color: {{windowBg}};
        }

        QWidget#frameParseSection,
        QWidget#frameParseCardGrid {
            background: transparent;
        }

        QScrollArea#frameParseScroll {
            border: none;
        }

        QLabel#frameParseBanner {
            border-radius: 8px;
            padding: 8px 10px;
            border: 1px solid {{border}};
            background-color: {{panelBg}};
            color: {{text}};
            font-family: "Microsoft YaHei";
        }

        QLabel#frameParseBanner[state="notice"] {
            background-color: {{noticeBg}};
            border-color: {{noticeBorder}};
            color: {{noticeText}};
        }

        QLabel#frameParseBanner[state="error"] {
            background-color: {{errorBg}};
            border-color: {{errorBorder}};
            color: {{errorText}};
        }

        QLabel#frameParseSectionTitle {
            color: {{accentTitle}};
            font-family: "Microsoft YaHei";
            font-size: 14px;
            font-weight: 600;
            padding: 2px 0 4px 2px;
        }

        QFrame#frameParseCard {
            background-color: {{panelBg}};
            border: 1px solid {{border}};
            border-radius: 8px;
        }

        QLabel#frameParseCardTitle {
            color: {{cardTitle}};
            font-family: "Microsoft YaHei";
            font-size: 12px;
            font-weight: 600;
        }

        QLabel#frameParseCardValue {
            color: {{text}};
            font-family: "Microsoft YaHei";
            font-size: 13px;
        }

        QLabel#frameParseCardDetail,
        QLabel#frameParsePlaceholder {
            color: {{cardDetail}};
            font-family: "Microsoft YaHei";
            font-size: 11px;
        }
    )");

    setStyleSheet(ui::applyThemeTokens(styleTemplate, p));

    applyBackgroundPalette(this, backgroundColor);
    applyBackgroundPalette(m_scrollArea, backgroundColor);
    applyBackgroundPalette(m_contentWidget, backgroundColor);
    applyBackgroundPalette(m_sectionsHost, backgroundColor);

    if (m_scrollArea && m_scrollArea->viewport())
    {
        applyBackgroundPalette(m_scrollArea->viewport(), backgroundColor);
        m_scrollArea->viewport()->setStyleSheet(
            QStringLiteral("background-color: %1;").arg(backgroundColor.name()));
    }

    updateBanner();
}

void FrameParseResultWidget::updateBanner()
{
    if (!m_noticeLabel)
    {
        return;
    }

    QString state;
    QString text;
    if (!m_result.errorText.isEmpty())
    {
        state = QStringLiteral("error");
        text = m_result.errorText;
    }
    else if (!m_result.noticeText.isEmpty())
    {
        state = QStringLiteral("notice");
        text = m_result.noticeText;
    }

    m_noticeLabel->setProperty("state", QVariant(state));
    m_noticeLabel->setText(text);
    m_noticeLabel->setVisible(!text.isEmpty());
    style()->unpolish(m_noticeLabel);
    style()->polish(m_noticeLabel);
    m_noticeLabel->update();
}
