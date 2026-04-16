#ifndef FRAMEPARSERESULTWIDGET_H
#define FRAMEPARSERESULTWIDGET_H

#include <QWidget>

#include "Utils/DLT645FrameFormatter.h"

class QLabel;
class QScrollArea;
class QVBoxLayout;

class FrameParseResultWidget : public QWidget
{
public:
    explicit FrameParseResultWidget(QWidget *parent = nullptr);

    void clearResult();
    void setParseResult(const FrameParseDisplayResult &result);
    void applyDarkTheme();
    void applyLightTheme();

private:
    QWidget *createSectionWidget(const FrameParseSection &section) const;
    QWidget *createCardWidget(const FrameParseCard &card) const;
    void rebuildSections();
    void clearSectionWidgets();
    void applyCurrentTheme();
    void updateBanner();

    QLabel *m_noticeLabel;
    QScrollArea *m_scrollArea;
    QWidget *m_contentWidget;
    QWidget *m_sectionsHost;
    QVBoxLayout *m_sectionsLayout;
    FrameParseDisplayResult m_result;
    bool m_darkTheme;
};

#endif // FRAMEPARSERESULTWIDGET_H
