#ifndef CATEGORYCATALOG_H
#define CATEGORYCATALOG_H

#include <QList>
#include <QString>
#include <QStringList>

class MeterModel;

/**
 * @brief 数据分类目录（事件记录 / 参变量）的统一定义
 *
 * 事件分类（0x03 族）与参变量分类（0x04 族）的名称、DI 前缀映射、结算日工具、
 * 以及按分类过滤 DI 码的辅助函数集中在此，供设备树、数据表、主站查询、
 * 查询帧等多处复用，避免在各处重复维护同一份定义。
 */
namespace CategoryCatalog
{
    /** "根/叶" 形式分类字符串拆分后的结果。 */
    struct CategorySelection
    {
        QString rootCategory;
        QString leafCategory;
    };

    /** 事件分类名称与其 DI 前缀集合的映射项。 */
    struct EventCategoryDefinition
    {
        QString name;
        QStringList prefixes;
    };

    /** 参变量分类名称与其 DI 前缀集合的映射项。 */
    struct ParameterCategoryDefinition
    {
        QString name;
        QStringList prefixes;
    };

    /** 将 "根/叶" 形式的分类字符串拆分为根分类与叶分类；无分隔符时叶分类为空。 */
    CategorySelection parseCategorySelection(const QString &category);

    /** 从结算日分类名（如 "当前"、"上3结算日"）解析结算日序号，无法识别返回 -1。 */
    int settlementDayFromCategoryName(const QString &categoryName);

    /** 生成某根分类下的结算日子分类列表（当前 + 上1~12结算日）。 */
    QList<QString> settlementDayCategories(const QString &rootCategory);

    /** 事件分类名称列表（按标准 A.4 家族顺序）。 */
    QStringList eventCategoryNames();

    /** 事件分类名称到 DI 前缀的映射（按标准 A.4 家族顺序）。 */
    QList<EventCategoryDefinition> eventCategoryDefinitions();

    /** 参变量分类名称列表（按标准 A.5 分类顺序）。 */
    QStringList parameterCategoryNames();

    /** 参变量分类名称到 DI 前缀的映射（按标准 A.5 分类顺序）。 */
    QList<ParameterCategoryDefinition> parameterCategoryDefinitions();

    /** 按前缀集合过滤 DI 码；前缀为空时原样返回全部。 */
    QList<QString> filterCodesByPrefixes(const QList<QString> &allCodes, const QStringList &prefixes);

    /**
     * @brief 按事件分类名过滤 DI 码。
     * 先按前缀匹配；若无命中且提供了 meterModel，则回退到按数据项描述包含分类名匹配。
     */
    QList<QString> filterEventCodesByCategory(const MeterModel *meterModel, const QList<QString> &allCodes, const QString &categoryName);

    /** 按参变量分类名过滤 DI 码（仅前缀匹配，无命中前缀时返回全部）。 */
    QList<QString> filterParameterCodesByCategory(const QList<QString> &allCodes, const QString &categoryName);
}

#endif // CATEGORYCATALOG_H
