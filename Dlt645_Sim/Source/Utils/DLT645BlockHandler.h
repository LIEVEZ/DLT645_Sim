#ifndef DLT645BLOCKHANDLER_H
#define DLT645BLOCKHANDLER_H

#include <QString>
#include <QStringList>

class MeterModel;

/**
 * @brief DLT645 数据块处理器
 *
 * 负责处理各种数据块（动态块、谐波块、冻结块等）的载荷和显示顺序码
 */
namespace DLT645BlockHandler
{
    /**
     * @brief 判断是否为谐波块代码
     */
    bool isHarmonicBlockCode(const QString &diCode, const QString &familyPrefix);

    /**
     * @brief 获取谐波块的载荷顺序码
     */
    QStringList harmonicPayloadOrderCodes(const QString &blockCode);

    /**
     * @brief 获取谐波块的显示顺序码
     */
    QStringList harmonicDisplayOrderCodes(const QString &blockCode);

    /**
     * @brief 判断是否为动态需量块
     */
    bool isDynamicDemandBlock(const MeterModel *meterModel, const QString &diCode);

    /**
     * @brief 判断是否为动态电能块
     */
    bool isDynamicEnergyBlock(const MeterModel *meterModel, const QString &diCode);

    /**
     * @brief 判断是否为动态冻结电能块
     */
    bool isDynamicFreezeEnergyBlock(const MeterModel *meterModel, const QString &diCode);

    /**
     * @brief 判断是否为最近整点冻结复合块
     */
    bool isLatestHourlyFreezeCompositeBlock(const QString &diCode);

    /**
     * @brief 判断是否为费率数量相关块
     */
    bool isRateCountDependentBlock(const MeterModel *meterModel, const QString &diCode);

    /**
     * @brief 获取动态电能块的载荷顺序码
     */
    QStringList dynamicEnergyPayloadOrderCodes(const MeterModel *meterModel, const QString &diCode, int rateCountOverride = -1);

    /**
     * @brief 获取动态电能块的显示顺序码
     */
    QStringList dynamicEnergyDisplayOrderCodes(const MeterModel *meterModel, const QString &diCode, int rateCountOverride = -1);

    /**
     * @brief 获取动态需量块的载荷顺序码
     */
    QStringList dynamicDemandPayloadOrderCodes(const MeterModel *meterModel, const QString &diCode, int rateCountOverride = -1);

    /**
     * @brief 获取动态需量块的显示顺序码
     */
    QStringList dynamicDemandDisplayOrderCodes(const MeterModel *meterModel, const QString &diCode, int rateCountOverride = -1);

    /**
     * @brief 获取动态冻结电能块的载荷顺序码
     */
    QStringList dynamicFreezeEnergyPayloadOrderCodes(const MeterModel *meterModel, const QString &diCode, int rateCountOverride = -1);

    /**
     * @brief 获取动态冻结电能块的显示顺序码
     */
    QStringList dynamicFreezeEnergyDisplayOrderCodes(const MeterModel *meterModel, const QString &diCode, int rateCountOverride = -1);

    /**
     * @brief 获取最近整点冻结复合块的代码列表
     */
    QStringList latestHourlyFreezeCompositeCodes();

    /**
     * @brief 获取块的载荷顺序码
     */
    QStringList payloadOrderCodesForBlock(const MeterModel *meterModel, const QString &diCode, int rateCountOverride = -1);

    /**
     * @brief 获取块的显示顺序码
     */
    QStringList displayOrderCodesForBlock(const MeterModel *meterModel, const QString &diCode, int rateCountOverride = -1);

    /**
     * @brief 选择块的载荷顺序码（智能匹配）
     */
    QStringList selectPayloadOrderCodesForBlock(const MeterModel *meterModel,
                                                const QString &blockDiCode,
                                                const QByteArray &payload,
                                                int *rateCountOverrideOut = nullptr);
}

#endif // DLT645BLOCKHANDLER_H
