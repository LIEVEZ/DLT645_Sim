#ifndef TERMDEFINITION_H
#define TERMDEFINITION_H

#include <QString>
#include <QStringList>

/**
 * @brief 术语定义
 *
 * 电表领域术语的详细解释，包括定义、公式、单位、说明等。
 * 用于在数据项手册中为用户提供上下文帮助。
 */
struct TermDefinition {
    QString term;           // 术语名称（中文，如"功率因数"）
    QString englishTerm;    // 英文名称（如"Power Factor"）
    QString definition;     // 定义
    QString formula;        // 公式（支持 HTML 上下标）
    QString unit;           // 单位
    QString range;          // 取值范围
    QString note;           // 说明
    QStringList keywords;   // 关键词列表（用于搜索匹配）

    /**
     * @brief 转换为 HTML 格式（用于 QTextEdit 显示）
     * @return HTML 格式的术语详情
     */
    QString toHtml() const {
        QString html;

        // 标题（术语名 + 英文名）
        html += QString("<p style='font-size:14px; font-weight:bold; color:#4A9EFF; margin-bottom:8px;'>");
        html += term;
        if (!englishTerm.isEmpty()) {
            html += QString(" <span style='font-weight:normal; color:#888888;'>(%1)</span>").arg(englishTerm);
        }
        html += "</p>";

        // 定义
        if (!definition.isEmpty()) {
            html += QString("<p style='margin-bottom:6px;'><b>定义：</b>%1</p>").arg(definition);
        }

        // 公式
        if (!formula.isEmpty()) {
            html += QString("<p style='margin-bottom:6px; background-color:#45494A; padding:4px 8px; "
                           "border-radius:3px; font-family:Consolas,monospace;'>"
                           "<b>公式：</b>%1</p>").arg(formula);
        }

        // 单位
        if (!unit.isEmpty()) {
            html += QString("<p style='margin-bottom:6px;'><b>单位：</b>%1</p>").arg(unit);
        }

        // 范围
        if (!range.isEmpty()) {
            html += QString("<p style='margin-bottom:6px;'><b>范围：</b>%1</p>").arg(range);
        }

        // 说明
        if (!note.isEmpty()) {
            html += QString("<p style='margin-bottom:6px;'><b>说明：</b>%1</p>").arg(note);
        }

        return html;
    }

    /**
     * @brief 检查是否匹配关键词（不区分大小写）
     * @param keyword 搜索关键词
     * @return 如果匹配返回 true
     */
    bool matchesKeyword(const QString &keyword) const {
        if (keyword.isEmpty()) {
            return false;
        }

        QString lowerKeyword = keyword.toLower();

        // 精确匹配术语名或英文名
        if (term.toLower() == lowerKeyword || englishTerm.toLower() == lowerKeyword) {
            return true;
        }

        // 部分匹配术语名或英文名
        if (term.toLower().contains(lowerKeyword) || englishTerm.toLower().contains(lowerKeyword)) {
            return true;
        }

        // 匹配关键词列表
        for (const QString &kw : keywords) {
            if (kw.toLower().contains(lowerKeyword) || lowerKeyword.contains(kw.toLower())) {
                return true;
            }
        }

        return false;
    }
};

#endif // TERMDEFINITION_H
