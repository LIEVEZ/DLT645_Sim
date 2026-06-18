#ifndef DLT645VALUEFORMATTER_H
#define DLT645VALUEFORMATTER_H

#include <QString>
#include <QByteArray>
#include <QList>

class MeterModel;
struct FrameParseCard;

/**
 * @brief DLT645 值格式化器
 *
 * 负责解码和格式化 DLT645 协议数据项的值，包括：
 * - 单个数据项的解码和格式化
 * - 块数据的解码和格式化
 * - 字节分解显示
 * - 块解码卡片构建
 */
namespace DLT645ValueFormatter
{
    /**
     * @brief 解码数据项的值
     * @param meterModel 电表模型
     * @param diCode 数据标识代码
     * @param payload 载荷数据
     * @param descriptionOut 输出参数：数据项描述
     * @param unitOut 输出参数：数据项单位
     * @return 解码后的值字符串
     */
    QString decodedValueForItem(const MeterModel *meterModel,
                                const QString &diCode,
                                const QByteArray &payload,
                                QString *descriptionOut = nullptr,
                                QString *unitOut = nullptr);

    /**
     * @brief 获取块数据项的显示值
     * @param meterModel 电表模型
     * @param diCode 数据标识代码
     * @param payload 载荷数据
     * @param descriptionOut 输出参数：数据项描述
     * @param unitOut 输出参数：数据项单位
     * @return 格式化后的显示值
     */
    QString blockDisplayValueForItem(const MeterModel *meterModel,
                                     const QString &diCode,
                                     const QByteArray &payload,
                                     QString *descriptionOut = nullptr,
                                     QString *unitOut = nullptr);

    /**
     * @brief 生成字节分解行（单个数据项）
     * @param diCode 数据标识代码
     * @param payload 载荷数据
     * @param format 格式串
     * @param fallbackDecodedValue 回退解码值
     * @param unit 单位
     * @param fallbackFieldName 回退字段名
     * @return 字节分解文本
     */
    QString byteBreakdownLines(const QString &diCode,
                               const QByteArray &payload,
                               const QString &format,
                               const QString &fallbackDecodedValue,
                               const QString &unit,
                               const QString &fallbackFieldName = QString());

    /**
     * @brief 生成字节分解行（块数据）
     * @param meterModel 电表模型
     * @param blockDiCode 块数据标识代码
     * @param payload 载荷数据
     * @return 字节分解文本，失败返回空字符串
     */
    QString blockByteBreakdownLines(const MeterModel *meterModel,
                                    const QString &blockDiCode,
                                    const QByteArray &payload);

    /**
     * @brief 构建块解码卡片列表
     * @param meterModel 电表模型
     * @param blockDiCode 块数据标识代码
     * @param payload 载荷数据
     * @return 解码卡片列表，失败返回空列表
     */
    QList<FrameParseCard> buildBlockDecodeCards(const MeterModel *meterModel,
                                                const QString &blockDiCode,
                                                const QByteArray &payload);
}

#endif // DLT645VALUEFORMATTER_H
