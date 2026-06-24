#ifndef TERMEXPLANATIONWIDGET_H
#define TERMEXPLANATIONWIDGET_H

#include <QWidget>

class QTextEdit;
class QLabel;
struct TermDefinition;

/**
 * @brief 术语解释面板组件
 *
 * 在数据项手册中显示术语详细解释的可复用组件。
 * 支持自动显示/隐藏、主题切换。
 */
class TermExplanationWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TermExplanationWidget(QWidget *parent = nullptr);

    /**
     * @brief 显示术语详情
     * @param term 术语定义
     */
    void showTerm(const TermDefinition &term);

    /**
     * @brief 清空内容并隐藏面板
     */
    void clear();

    /**
     * @brief 检查是否为空（无内容）
     */
    bool isEmpty() const;

    /**
     * @brief 应用深色主题
     */
    void applyDarkTheme();

    /**
     * @brief 应用浅色主题
     */
    void applyLightTheme();

private:
    void setupUI();
    void showWelcomeMessage();  // 显示欢迎提示信息

    QLabel *m_titleLabel;       // 标题（"📖 术语解释"）
    QTextEdit *m_contentEdit;   // 内容显示区域
    bool m_isEmpty;             // 是否为空
    bool m_darkTheme;           // 当前主题
};

#endif // TERMEXPLANATIONWIDGET_H
