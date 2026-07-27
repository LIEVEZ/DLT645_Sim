#ifndef STATUSWORDBITCATALOG_H
#define STATUSWORDBITCATALOG_H

#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QtGlobal>

/**
 * @brief 位标志参数中的一个位字段（支持单 bit 或多 bit 编码）
 */
struct StatusWordBitField
{
    int bitOffset = 0;   // 起始位（Bit0 为最低位）
    int bitWidth = 1;    // 位宽
    QString name;        // 字段含义
    QMap<int, QString> meanings; // 取值 -> 说明
    bool reserved = false;
};

/**
 * @brief 位标志参数的完整定义（状态字 / 特征字等）
 */
struct StatusWordDefinition
{
    int index = 0;       // 状态字 1..7；其他位标志参数为 0
    QString diCode;      // 可选，精确 DI
    QString title;
    QString subtitle;
    QStringList notes;
    QVector<StatusWordBitField> fields;
    int byteWidth = 2;   // 1=AA，2=AA,AA
};

/**
 * @brief 电表位标志参数目录（运行状态字、组合方式特征字等）
 *
 * 供术语解释、双击详情对照等界面复用，避免多处硬编码漂移。
 */
namespace StatusWordBitCatalog
{
bool isStatusWordCode(const QString &diCode);
bool hasBitDefinition(const QString &diCode);
bool hasBitDefinitionForDescription(const QString &description);
int statusWordIndexFromCode(const QString &diCode);
int statusWordIndexFromDescription(const QString &description);
StatusWordDefinition definitionForIndex(int index);
StatusWordDefinition definitionForCode(const QString &diCode);
StatusWordDefinition definitionForDescription(const QString &description);

QString bitRangeLabel(const StatusWordBitField &field);
int extractFieldValue(quint16 word, const StatusWordBitField &field);
quint16 applyFieldValue(quint16 word, const StatusWordBitField &field, int fieldValue);
QString meaningText(const StatusWordBitField &field, int fieldValue);
QString meaningsSummary(const StatusWordBitField &field);
QString formatWordHex(quint16 word, int byteWidth = 2);
QString formatProtocolBytes(quint16 word, int byteWidth = 2);
QString htmlBitDefinition(const StatusWordDefinition &definition);
QString htmlBitDefinitionForDescription(const QString &description);
QString describeErrorInfoByte(quint8 errorCode);
}

#endif // STATUSWORDBITCATALOG_H
