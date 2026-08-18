#include "DLT645FrameFormatter.h"
#include "Models/StatusWordBitCatalog.h"
#include "DLT645BlockHandler.h"
#include "DLT645ValueFormatter.h"
#include "DLT645FrameFormatterFields.h"
#include "DLT645FrameFormatterValueDisplay.h"

#include "Models/DataItem.h"
#include "Models/MeterModel.h"
#include "Services/ProtocolParser.h"
#include "Utils/BCDHelper.h"
#include "Utils/ChecksumHelper.h"
#include "Utils/DLT645DataHelper.h"

#include <algorithm>
#include <QMap>
#include <QStringList>

namespace
{
using namespace DLT645FrameFormatterFields;
using namespace DLT645FrameFormatterValueDisplay;
constexpr int kDefaultRateCount = 5;
constexpr int kMaxRateCount = 8;
constexpr int kHarmonicOrderCount = 21;


QString formatByteHex(quint8 value)
{
    return QStringLiteral("0x%1").arg(value, 2, 16, QChar('0')).toUpper();
}


QString lineWithValue(const QString &label, const QString &value)
{
    return label + value + QStringLiteral("\n");
}

QString appendUnit(const QString &value, const QString &unit, bool withSpace = true)
{
    if (value.isEmpty())
    {
        return value;
    }

    if (unit.isEmpty() || unit == QStringLiteral("-") || unit == QStringLiteral("none"))
    {
        return value;
    }

    return withSpace ? (value + QStringLiteral(" ") + unit) : (value + unit);
}

QString hexByte(quint8 value)
{
    return QStringLiteral("%1").arg(value, 2, 16, QChar('0')).toUpper();
}

QString separatorLine(QChar ch)
{
    return QString(50, ch);
}

QString bannerLine(const QString &title, QChar ch)
{
    const int sideWidth = qMax(1, (50 - title.length()) / 2);
    QString line = QString(sideWidth, ch) + title + QString(sideWidth, ch);
    if (line.length() < 50)
    {
        line += QString(50 - line.length(), ch);
    }
    return line;
}

QString annotatedLine(const QString &hex, const QString &label, const QString &value = QString())
{
    return value.isEmpty()
               ? QStringLiteral("%1 --- %2").arg(hex, label)
               : QStringLiteral("%1 --- %2:%3").arg(hex, label, value);
}

QString bcdByteText(quint8 value)
{
    return QStringLiteral("%1").arg(value, 2, 16, QChar('0')).toUpper();
}

QString remoteControlCommandName(quint8 command)
{
    switch (command)
    {
    case 0x1A:
        return QStringLiteral("跳闸");
    case 0x1B:
        return QStringLiteral("合闸");
    case 0x1C:
        return QStringLiteral("直接合闸");
    case 0x2A:
        return QStringLiteral("报警");
    case 0x2B:
        return QStringLiteral("报警解除");
    case 0x3A:
        return QStringLiteral("保电");
    case 0x3B:
        return QStringLiteral("保电解除");
    default:
        return QStringLiteral("未知控制命令(0x%1)").arg(command, 2, 16, QChar('0')).toUpper();
    }
}

QString formatRemoteControlDeadline(const QByteArray &timeBytes)
{
    if (timeBytes.size() < 6)
    {
        return QString();
    }

    // ss mm HH dd MM yy（BCD）
    return QStringLiteral("%1年%2月%3日%4时%5分%6秒")
        .arg(bcdByteText(static_cast<quint8>(timeBytes.at(5))),
             bcdByteText(static_cast<quint8>(timeBytes.at(4))),
             bcdByteText(static_cast<quint8>(timeBytes.at(3))),
             bcdByteText(static_cast<quint8>(timeBytes.at(2))),
             bcdByteText(static_cast<quint8>(timeBytes.at(1))),
             bcdByteText(static_cast<quint8>(timeBytes.at(0))));
}

QString formatRemoteControlCommandDetail(const QByteArray &commandBytes)
{
    if (commandBytes.isEmpty())
    {
        return QStringLiteral("命令数据为空");
    }

    const quint8 command = static_cast<quint8>(commandBytes.at(0));
    const QString commandName = remoteControlCommandName(command);

    // N1 命令码，N2 保留，N3~N8 为 ss mm HH dd MM yy（BCD）。
    if (commandBytes.size() >= 8)
    {
        const QString commandTime = formatRemoteControlDeadline(commandBytes.mid(2, 6));
        if ((command == 0x1B || command == 0x1C) && !commandTime.isEmpty())
        {
            return QStringLiteral("%1, 截止时间:%2").arg(commandName, commandTime);
        }
        if (!commandTime.isEmpty())
        {
            return QStringLiteral("%1, 命令时间:%2").arg(commandName, commandTime);
        }
    }

    if (commandBytes.size() > 1)
    {
        return QStringLiteral("%1, 参数:%2")
            .arg(commandName, DLT645FrameFormatter::formatHex(commandBytes.mid(1)));
    }
    return commandName;
}

QString formatPasswordDigits(const QByteArray &passwordBytes)
{
    // DLT645 密码按低字节在前发送，展示时倒序还原真实密码
    QByteArray displayBytes = passwordBytes;
    std::reverse(displayBytes.begin(), displayBytes.end());
    return DLT645FrameFormatter::formatHex(displayBytes, false);
}

QString formatOperatorCodeDigits(const QByteArray &operatorBytes)
{
    return DLT645FrameFormatter::formatHex(operatorBytes, false);
}

QString controlCodeDetail(quint8 code)
{
    const quint8 functionCode = code & 0x1F;
    QString functionText;
    switch (functionCode)
    {
    case 0x08:
        functionText = QStringLiteral("广播校时");
        break;
    case 0x11:
        functionText = QStringLiteral("读数据");
        break;
    case 0x12:
        functionText = QStringLiteral("读后续数据");
        break;
    case 0x13:
        functionText = QStringLiteral("读通信地址");
        break;
    case 0x14:
        functionText = QStringLiteral("写数据");
        break;
    case 0x15:
        functionText = QStringLiteral("写通信地址");
        break;
    case 0x16:
        functionText = QStringLiteral("冻结命令");
        break;
    case 0x17:
        functionText = QStringLiteral("更改通信速率");
        break;
    case 0x18:
        functionText = QStringLiteral("修改密码");
        break;
    case 0x19:
        functionText = QStringLiteral("最大需量清零");
        break;
    case 0x1A:
        functionText = QStringLiteral("电表清零");
        break;
    case 0x1B:
        functionText = QStringLiteral("事件清零");
        break;
    case 0x1C:
        functionText = QStringLiteral("远程控制");
        break;
    default:
        functionText = DLT645FrameFormatter::controlCodeName(code);
        break;
    }

    QStringList parts;
    parts.append(functionText);
    if ((code & 0x80) != 0)
    {
        parts.append((code & 0x40) != 0 ? QStringLiteral("异常应答") : QStringLiteral("正常应答"));
    }
    else
    {
        parts.append(QStringLiteral("请求"));
    }
    parts.append((code & 0x20) != 0 ? QStringLiteral("有后续数据帧") : QStringLiteral("无后续数据帧"));
    return parts.join(QStringLiteral("，"));
}

QString recordIndexText(const QString &diCode)
{
    bool ok = false;
    const int index = diCode.mid(8, 2).toInt(&ok, 16);
    return ok && index > 0 ? QString::number(index) : QString();
}

QString coverEventNameForDi(const QString &diCode)
{
    if (diCode.startsWith(QStringLiteral("0x03300D")))
    {
        return QStringLiteral("开表盖");
    }
    if (diCode.startsWith(QStringLiteral("0x03300E")))
    {
        return QStringLiteral("开端钮盒");
    }

    return QString();
}

QString inferredDescriptionForDi(const QString &diCode)
{
    const QString eventName = coverEventNameForDi(diCode);
    if (!eventName.isEmpty() && isHistoryItem(diCode))
    {
        const QString index = recordIndexText(diCode);
        return index.isEmpty()
                   ? QStringLiteral("%1事件记录").arg(eventName)
                   : QStringLiteral("上%1次%2事件记录").arg(index, eventName);
    }

    return QString();
}

[[maybe_unused]] QStringList coverOpenEventFieldNames()
{
    return coverEventFieldNames(QStringLiteral("开表盖"));
}

QString coverOpenEventFormat()
{
    const QString timeFormat = QStringLiteral("AA,AA,AA,AA,AA,AA");
    const QString energyFormat = QStringLiteral("AA,AA,AA,.,AA");
    QStringList parts = {timeFormat, timeFormat};
    for (int i = 0; i < 12; ++i)
    {
        parts.append(energyFormat);
    }
    return parts.join(QStringLiteral(":"));
}

QString inferredFormatForDi(const QString &diCode)
{
    if (!coverEventNameForDi(diCode).isEmpty() && isHistoryItem(diCode))
    {
        return coverOpenEventFormat();
    }

    return QString();
}

QString inferredUnitForDi(const QString &diCode, int fieldIndex)
{
    if (!coverEventNameForDi(diCode).isEmpty() && isHistoryItem(diCode) && fieldIndex >= 2)
    {
        const int energyIndex = (fieldIndex - 2) % 6;
        return energyIndex < 2 ? QStringLiteral("kWh") : QStringLiteral("kvarh");
    }

    const bool isPhasePowerEvent = (diCode.startsWith(QStringLiteral("0x0301")) ||
                                    diCode.startsWith(QStringLiteral("0x0302")) ||
                                    diCode.startsWith(QStringLiteral("0x0303")) ||
                                    diCode.startsWith(QStringLiteral("0x0304")) ||
                                    diCode.startsWith(QStringLiteral("0x030B")) ||
                                    diCode.startsWith(QStringLiteral("0x030C")) ||
                                    diCode.startsWith(QStringLiteral("0x030D"))) &&
                                   diCode.mid(6, 2) != QStringLiteral("00") &&
                                   isHistoryItem(diCode);
    if (isPhasePowerEvent)
    {
        if (fieldIndex >= 2 && fieldIndex < 6)
        {
            const int energyIndex = fieldIndex - 2;
            return energyIndex < 2 ? QStringLiteral("kWh") : QStringLiteral("kvarh");
        }
        if (fieldIndex >= 6)
        {
            const int blockIndex = (fieldIndex - 6) % 9;
            if (blockIndex < 4)
            {
                return blockIndex < 2 ? QStringLiteral("kWh") : QStringLiteral("kvarh");
            }
            if (blockIndex == 4)
            {
                return QStringLiteral("V");
            }
            if (blockIndex == 5)
            {
                return QStringLiteral("A");
            }
            if (blockIndex == 6)
            {
                return QStringLiteral("kW");
            }
            if (blockIndex == 7)
            {
                return QStringLiteral("kvar");
            }
        }
    }

    const bool isUnbalanceEvent = (diCode.startsWith(QStringLiteral("0x030900")) ||
                                   diCode.startsWith(QStringLiteral("0x030A00"))) &&
                                  isHistoryItem(diCode);
    const bool isSequenceEvent = (diCode.startsWith(QStringLiteral("0x030700")) ||
                                  diCode.startsWith(QStringLiteral("0x030800")) ||
                                  ((diCode.startsWith(QStringLiteral("0x030E")) ||
                                    diCode.startsWith(QStringLiteral("0x030F"))) &&
                                   diCode.mid(6, 2) != QStringLiteral("00"))) &&
                                 isHistoryItem(diCode);
    if (isUnbalanceEvent || isSequenceEvent)
    {
        const int firstEnergyField = isUnbalanceEvent ? 3 : 2;
        if (fieldIndex >= firstEnergyField)
        {
            const int energyIndex = (fieldIndex - firstEnergyField) % 4;
            return energyIndex < 2 ? QStringLiteral("kWh") : QStringLiteral("kvarh");
        }
    }

    return QString();
}

QString displayDiCode(const QString &diCode)
{
    QString code = diCode;
    code.remove(QStringLiteral("0x"));
    return code.toUpper();
}


bool calculateChecksumValidity(const QByteArray &frame)
{
    const QByteArray protocolFrame = DLT645DataHelper::stripLeadingFE(frame);
    if (protocolFrame.size() < 12)
    {
        return false;
    }

    const int declaredLength = static_cast<quint8>(protocolFrame[9]);
    const int expectedSize = 12 + declaredLength;
    if (protocolFrame.size() < expectedSize)
    {
        return false;
    }

    const quint8 checksum = static_cast<quint8>(protocolFrame[10 + declaredLength]);
    const QByteArray checksumRange = protocolFrame.left(10 + declaredLength);
    return ChecksumHelper::calculateChecksum(checksumRange) == checksum;
}

FrameParseCard makeCard(const QString &title,
                        const QString &value,
                        const QString &detail = QString(),
                        bool valueMonospace = false,
                        bool detailMonospace = false)
{
    FrameParseCard card;
    card.title = title;
    card.value = value;
    card.detail = detail;
    card.valueMonospace = valueMonospace;
    card.detailMonospace = detailMonospace;
    return card;
}

void appendCard(FrameParseSection &section,
                const QString &title,
                const QString &value,
                const QString &detail = QString(),
                bool valueMonospace = false,
                bool detailMonospace = false)
{
    section.cards.append(makeCard(title, value, detail, valueMonospace, detailMonospace));
}



[[maybe_unused]] FrameParseSection buildStructureSection(const QByteArray &frame, bool checksumValid, QString *messageOut)
{
    FrameParseSection section;
    section.title = QString::fromUtf8("结构解析");

    int leadingCount = 0;
    const QByteArray protocolFrame = DLT645DataHelper::stripLeadingFE(frame, &leadingCount);

    QString parseMessage;
    if (frame.isEmpty())
    {
        parseMessage = QString::fromUtf8("空帧，无法解析。");
    }

    const bool hasStartFlag = protocolFrame.size() >= 1;
    const bool hasAddress = protocolFrame.size() >= 7;
    const bool hasSecondStartFlag = protocolFrame.size() >= 8;
    const bool hasControlCode = protocolFrame.size() >= 9;
    const bool hasLength = protocolFrame.size() >= 10;

    const QByteArray address = hasAddress ? protocolFrame.mid(1, 6) : QByteArray();
    const quint8 startFlag = hasStartFlag ? static_cast<quint8>(protocolFrame[0]) : 0;
    const quint8 secondStartFlag = hasSecondStartFlag ? static_cast<quint8>(protocolFrame[7]) : 0;
    const quint8 controlCode = hasControlCode ? static_cast<quint8>(protocolFrame[8]) : 0;
    const quint8 dataLength = hasLength ? static_cast<quint8>(protocolFrame[9]) : 0;
    const int actualDataLength = hasLength
                                        ? ProtocolParser::effectiveDataFieldLength(controlCode, dataLength)
                                        : 0;
    const int expectedSize = hasLength ? (12 + actualDataLength) : 12;
    const bool hasCompleteFrame = hasLength && protocolFrame.size() >= expectedSize;

    QByteArray displayedDataField;
    if (protocolFrame.size() > 10)
    {
        displayedDataField = hasCompleteFrame ? protocolFrame.mid(10, actualDataLength) : protocolFrame.mid(10);
    }

    QString structureText;
    structureText += lineWithValue(QString::fromUtf8("前导码："),
                                   leadingCount > 0 ? DLT645FrameFormatter::formatHex(frame.left(leadingCount)) : QString::fromUtf8("无"));
    structureText += lineWithValue(QString::fromUtf8("帧起始符："),
                                   hasStartFlag ? formatByteHex(startFlag) : QString::fromUtf8("未获取"));
    structureText += lineWithValue(QString::fromUtf8("地址域："),
                                   hasAddress ? DLT645FrameFormatter::formatHex(address) : QString::fromUtf8("未获取"));
    structureText += lineWithValue(QString::fromUtf8("电表号："),
                                   hasAddress ? ProtocolParser::bytesToAddress(address) : QString::fromUtf8("未获取"));
    structureText += lineWithValue(QString::fromUtf8("第二起始符："),
                                   hasSecondStartFlag ? formatByteHex(secondStartFlag) : QString::fromUtf8("未获取"));
    structureText += lineWithValue(QString::fromUtf8("控制码："),
                                   hasControlCode
                                       ? QString::fromUtf8("%1 (%2)").arg(formatByteHex(controlCode), DLT645FrameFormatter::controlCodeName(controlCode))
                                       : QString::fromUtf8("未获取"));
    structureText += lineWithValue(QString::fromUtf8("数据长度："),
                                   hasLength ? QString::fromUtf8("%1 (%2 字节)").arg(formatByteHex(dataLength)).arg(dataLength) : QString::fromUtf8("未获取"));
    structureText += lineWithValue(QString::fromUtf8("数据域原始值："),
                                   !displayedDataField.isEmpty() ? DLT645FrameFormatter::formatHex(displayedDataField)
                                                                 : (hasLength ? QString::fromUtf8("无") : QString::fromUtf8("未获取")));
    structureText += lineWithValue(QString::fromUtf8("数据域减0x33："),
                                   !displayedDataField.isEmpty() ? DLT645FrameFormatter::formatHex(DLT645DataHelper::subtract33(displayedDataField))
                                                                 : (hasLength ? QString::fromUtf8("无") : QString::fromUtf8("未获取")));

    if (hasCompleteFrame)
    {
        const quint8 checksum = static_cast<quint8>(protocolFrame[10 + dataLength]);
        const quint8 endFlag = static_cast<quint8>(protocolFrame[11 + dataLength]);
        structureText += lineWithValue(QString::fromUtf8("校验和："), formatByteHex(checksum));
        structureText += lineWithValue(QString::fromUtf8("校验状态："),
                                       checksumValid ? QString::fromUtf8("OK 有效") : QString::fromUtf8("X 无效"));
        structureText += lineWithValue(QString::fromUtf8("结束符："), formatByteHex(endFlag));

        if (protocolFrame.size() > expectedSize)
        {
            structureText += lineWithValue(QString::fromUtf8("尾随数据："),
                                           DLT645FrameFormatter::formatHex(protocolFrame.mid(expectedSize)));
        }
    }
    else
    {
        structureText += lineWithValue(QString::fromUtf8("校验和："), QString::fromUtf8("未获取"));
        structureText += lineWithValue(QString::fromUtf8("校验状态："), QString::fromUtf8("X 无效"));
        structureText += lineWithValue(QString::fromUtf8("结束符："), QString::fromUtf8("未获取"));

        if (parseMessage.isEmpty())
        {
            parseMessage = hasLength ? QString::fromUtf8("报文长度与声明的数据长度不一致，当前仅基于已收到字节解析。")
                                     : QString::fromUtf8("报文长度不足，尚未取得完整帧头。");
        }

        structureText += lineWithValue(QString::fromUtf8("长度异常/报文不完整提示："), parseMessage);
    }

    appendCard(section, section.title, structureText.trimmed());

    if (messageOut)
    {
        *messageOut = parseMessage;
    }

    return section;
}

FrameParseSection buildDetailedFrameSection(const QByteArray &frame,
                                            const MeterModel *meterModel,
                                            bool checksumValid,
                                            QString *messageOut)
{
    FrameParseSection section;
    section.title = QStringLiteral("帧解析明细");

    int leadingCount = 0;
    const QByteArray protocolFrame = DLT645DataHelper::stripLeadingFE(frame, &leadingCount);

    QString parseMessage;
    if (frame.isEmpty())
    {
        parseMessage = QStringLiteral("空帧，无法解析。");
    }

    const bool hasStartFlag = protocolFrame.size() >= 1;
    const bool hasAddress = protocolFrame.size() >= 7;
    const bool hasSecondStartFlag = protocolFrame.size() >= 8;
    const bool hasControlCode = protocolFrame.size() >= 9;
    const bool hasLength = protocolFrame.size() >= 10;

    const QByteArray address = hasAddress ? protocolFrame.mid(1, 6) : QByteArray();
    const quint8 startFlag = hasStartFlag ? static_cast<quint8>(protocolFrame[0]) : 0;
    const quint8 secondStartFlag = hasSecondStartFlag ? static_cast<quint8>(protocolFrame[7]) : 0;
    const quint8 controlCode = hasControlCode ? static_cast<quint8>(protocolFrame[8]) : 0;
    const quint8 dataLength = hasLength ? static_cast<quint8>(protocolFrame[9]) : 0;
    const int actualDataLength = hasLength
                                        ? ProtocolParser::effectiveDataFieldLength(controlCode, dataLength)
                                        : 0;
    const int expectedSize = hasLength ? (12 + actualDataLength) : 12;
    const bool hasCompleteFrame = hasLength && protocolFrame.size() >= expectedSize;
    const int clippedDataLength = hasLength
                                      ? qMin(actualDataLength, qMax(0, protocolFrame.size() - 12))
                                      : 0;
    const QByteArray dataField = protocolFrame.mid(10, clippedDataLength);

    QString detailText;
    if (leadingCount > 0)
    {
        detailText += annotatedLine(DLT645FrameFormatter::formatHex(frame.left(leadingCount)),
                                    QStringLiteral("前导码")) + QStringLiteral("\n");
    }
    detailText += annotatedLine(hasStartFlag ? hexByte(startFlag) : QStringLiteral("??"),
                                QStringLiteral("起始字符")) + QStringLiteral("\n");
    detailText += annotatedLine(hasAddress ? DLT645FrameFormatter::formatHex(address) : QStringLiteral("??"),
                                QStringLiteral("通信地址"),
                                hasAddress ? ProtocolParser::bytesToAddress(address) : QStringLiteral("未获取")) + QStringLiteral("\n");
    detailText += annotatedLine(hasSecondStartFlag ? hexByte(secondStartFlag) : QStringLiteral("??"),
                                QStringLiteral("第二个68")) + QStringLiteral("\n");
    detailText += annotatedLine(hasControlCode ? hexByte(controlCode) : QStringLiteral("??"),
                                QStringLiteral("控制码C"),
                                hasControlCode ? controlCodeDetail(controlCode) : QStringLiteral("未获取")) + QStringLiteral("\n");
    detailText += annotatedLine(hasLength ? hexByte(dataLength) : QStringLiteral("??"),
                                QStringLiteral("长度L"),
                                hasLength ? QString::number(dataLength) : QStringLiteral("未获取")) + QStringLiteral("\n");

    if (!dataField.isEmpty())
    {
        const QByteArray decodedDataField = DLT645DataHelper::subtract33(dataField);
        detailText += bannerLine(QStringLiteral("原始DATA"), QChar('*')) + QStringLiteral("\n");
        detailText += DLT645FrameFormatter::formatHex(dataField) + QStringLiteral("\n");
        detailText += bannerLine(QStringLiteral("-33"), QChar('*')) + QStringLiteral("\n");
        detailText += DLT645FrameFormatter::formatHex(decodedDataField) + QStringLiteral("\n");
        detailText += separatorLine(QChar('=')) + QStringLiteral("\n");

        // 检查是否为异常应答（控制码 bit6=1 表示异常）
        const bool isErrorResponse = hasControlCode && (controlCode & 0x40) != 0;
        const quint8 functionCode = hasControlCode ? static_cast<quint8>(controlCode & 0x1F) : 0x00;
        if (isErrorResponse && dataField.size() == 1)
        {
            const quint8 errorCode = static_cast<quint8>(decodedDataField[0]);
            const QString errorDescription = StatusWordBitCatalog::describeErrorInfoByte(errorCode);
            detailText += annotatedLine(hexByte(errorCode),
                                        QStringLiteral("错误信息字ERR"),
                                        errorDescription) + QStringLiteral("\n");
        }
        else if (functionCode == 0x1C)
        {
            // 远程控制：PA + 密码3字节 + 操作者代码4字节 + 命令码/参数
            if (decodedDataField.size() >= 8)
            {
                const QByteArray authorityBytes = decodedDataField.left(4);
                const QByteArray operatorBytes = decodedDataField.mid(4, 4);
                const QByteArray commandBytes = decodedDataField.mid(8);

                const quint8 permission = static_cast<quint8>(authorityBytes.at(0));
                const QString passwordText = formatPasswordDigits(authorityBytes.mid(1, 3));
                const QString operatorText = formatOperatorCodeDigits(operatorBytes);

                detailText += annotatedLine(DLT645FrameFormatter::formatHex(authorityBytes),
                                            QStringLiteral("权限:%1, 密码:%2")
                                                .arg(bcdByteText(permission), passwordText)) + QStringLiteral("\n");
                detailText += annotatedLine(DLT645FrameFormatter::formatHex(operatorBytes),
                                            QStringLiteral("操作者代码:%1").arg(operatorText)) + QStringLiteral("\n");
                if (!commandBytes.isEmpty())
                {
                    detailText += annotatedLine(DLT645FrameFormatter::formatHex(commandBytes),
                                                formatRemoteControlCommandDetail(commandBytes)) + QStringLiteral("\n");
                }
            }
            else if (!decodedDataField.isEmpty())
            {
                detailText += annotatedLine(DLT645FrameFormatter::formatHex(decodedDataField),
                                            QStringLiteral("远程控制数据"),
                                            QStringLiteral("长度不足，无法拆分权限/密码/操作者代码")) + QStringLiteral("\n");
            }
        }
        else if (functionCode == 0x13 && (controlCode & 0x80) != 0)
        {
            // 读通信地址应答的数据域为加 0x33 的 6 字节地址，低字节在前。
            if (decodedDataField.size() == 6)
            {
                detailText += annotatedLine(DLT645FrameFormatter::formatHex(decodedDataField),
                                            QStringLiteral("地址数据 (低字节在前)"),
                                            QStringLiteral("通信地址:%1")
                                                .arg(ProtocolParser::bytesToAddress(decodedDataField))) + QStringLiteral("\n");
            }
            else
            {
                detailText += annotatedLine(DLT645FrameFormatter::formatHex(decodedDataField),
                                            QStringLiteral("地址数据"),
                                            QStringLiteral("长度异常，期望 6 字节")) + QStringLiteral("\n");
            }
        }
        else if (dataField.size() >= 4)
        {
            const bool isWriteDataRequest = functionCode == 0x14 && (controlCode & 0x80) == 0;
            const QString diCode = ProtocolParser::extractDIFromDataField(dataField);
            DataItem diItem;
            QString description;
            QString unit;
            QString format;

            if (meterModel)
            {
                diItem = meterModel->getDataItem(diCode);
                if (!diItem.diCode.isEmpty())
                {
                    description = diItem.description;
                    unit = diItem.unit;
                    format = diItem.format;
                }
            }
            if (description.isEmpty())
            {
                description = inferredDescriptionForDi(diCode);
            }
            if (format.isEmpty())
            {
                format = inferredFormatForDi(diCode);
            }

            detailText += annotatedLine(DLT645FrameFormatter::formatHex(decodedDataField.left(4)),
                                        QStringLiteral("数据标识ID"),
                                        displayDiCode(diCode) + (description.isEmpty() ? QString() : QStringLiteral(" ") + description)) + QStringLiteral("\n");

            QByteArray payload = decodedDataField.mid(4);
            if (isWriteDataRequest && decodedDataField.size() >= 12)
            {
                const QByteArray authorityBytes = decodedDataField.mid(4, 4);
                const QByteArray operatorBytes = decodedDataField.mid(8, 4);
                const quint8 permission = static_cast<quint8>(authorityBytes.at(0));
                const QString passwordText = formatPasswordDigits(authorityBytes.mid(1, 3));
                const QString operatorText = formatOperatorCodeDigits(operatorBytes);

                detailText += annotatedLine(DLT645FrameFormatter::formatHex(authorityBytes),
                                            QStringLiteral("权限:%1, 密码:%2")
                                                .arg(bcdByteText(permission), passwordText)) + QStringLiteral("\n");
                detailText += annotatedLine(DLT645FrameFormatter::formatHex(operatorBytes),
                                            QStringLiteral("操作者代码:%1").arg(operatorText)) + QStringLiteral("\n");
                payload = decodedDataField.mid(12);
            }
            if (isWriteDataRequest && !payload.isEmpty())
            {
                QString writeDescription = description;
                QString writeUnit = unit;
                QString decodedWriteValue = DLT645ValueFormatter::decodedValueForItem(
                    meterModel, diCode, payload, &writeDescription, &writeUnit);
                if (decodedWriteValue.isEmpty())
                {
                    decodedWriteValue = DLT645FrameFormatter::formatHex(payload);
                }

                detailText += annotatedLine(DLT645FrameFormatter::formatHex(payload),
                                            QStringLiteral("写入值"),
                                            appendUnit(decodedWriteValue, writeUnit)) + QStringLiteral("\n");

                QByteArray bitPayload = payload;
                if (bitPayload.size() == 1 && StatusWordBitCatalog::isStatusWordCode(diCode))
                {
                    // 写入状态字时常见只携带低字节，补零后复用 2 字节位解析。
                    bitPayload.append(static_cast<char>(0x00));
                }
                const QStringList bitLines = StatusWordBitCatalog::detailLinesForPayload(diCode, bitPayload);
                if (!bitLines.isEmpty())
                {
                    detailText += bitLines.join(QStringLiteral("\n")) + QStringLiteral("\n");
                }
            }
            else if (!payload.isEmpty())
            {
                const QString blockBreakdown = DLT645ValueFormatter::blockByteBreakdownLines(meterModel, diCode, payload);
                if (!blockBreakdown.isEmpty())
                {
                    detailText += blockBreakdown + QStringLiteral("\n");
                }
                else if (!format.isEmpty())
                {
                    const QString decodedValue = DLT645ValueFormatter::decodedValueForItem(meterModel, diCode, payload, &description, &unit);
                    const QString breakdown = DLT645ValueFormatter::byteBreakdownLines(diCode, payload, format, decodedValue, unit, description);
                    if (!breakdown.isEmpty())
                    {
                        detailText += breakdown + QStringLiteral("\n");
                    }
                }
                else
                {
                    const QList<FrameParseCard> blockCards = DLT645ValueFormatter::buildBlockDecodeCards(meterModel, diCode, payload);
                    if (!blockCards.isEmpty())
                    {
                        for (const FrameParseCard &card : blockCards)
                        {
                            detailText += annotatedLine(card.detail.section(QStringLiteral("原始HEX: "), 1, 1).trimmed(),
                                                        card.title,
                                                        card.value) + QStringLiteral("\n");
                        }
                    }
                    else
                    {
                        QString decodeDescription = description;
                        QString decodeUnit = unit;
                        const QString decodedValue = DLT645ValueFormatter::decodedValueForItem(meterModel, diCode, payload, &decodeDescription, &decodeUnit);
                        detailText += annotatedLine(DLT645FrameFormatter::formatHex(payload),
                                                    decodeDescription.isEmpty() ? QStringLiteral("数据值") : decodeDescription,
                                                    appendUnit(decodedValue.isEmpty() ? DLT645FrameFormatter::formatHex(payload) : decodedValue,
                                                               decodeUnit)) + QStringLiteral("\n");
                    }
                }
            }
        }
        else
        {
            detailText += annotatedLine(DLT645FrameFormatter::formatHex(decodedDataField),
                                        QStringLiteral("数据域"),
                                        QStringLiteral("长度不足，无法提取数据标识")) + QStringLiteral("\n");
        }

        detailText += separatorLine(QChar('=')) + QStringLiteral("\n");
    }

    if (hasCompleteFrame)
    {
        const quint8 checksum = static_cast<quint8>(protocolFrame[10 + dataLength]);
        const quint8 endFlag = static_cast<quint8>(protocolFrame[11 + dataLength]);
        detailText += annotatedLine(hexByte(checksum),
                                    QStringLiteral("帧校验HCS"),
                                    QStringLiteral("%1 (%2)").arg(hexByte(checksum), checksumValid ? QStringLiteral("正确") : QStringLiteral("错误"))) + QStringLiteral("\n");
        detailText += annotatedLine(hexByte(endFlag), QStringLiteral("结束符"));
        if (protocolFrame.size() > expectedSize)
        {
            detailText += QStringLiteral("\n") + annotatedLine(DLT645FrameFormatter::formatHex(protocolFrame.mid(expectedSize)),
                                                               QStringLiteral("尾随数据"));
        }
    }
    else
    {
        if (parseMessage.isEmpty())
        {
            parseMessage = hasLength ? QStringLiteral("报文长度与声明的数据长度不一致，当前仅基于已收到字节解析。")
                                     : QStringLiteral("报文长度不足，尚未取得完整帧头。");
        }
        detailText += annotatedLine(QStringLiteral("??"),
                                    QStringLiteral("帧校验HCS"),
                                    QStringLiteral("未获取")) + QStringLiteral("\n");
        detailText += annotatedLine(QStringLiteral("??"), QStringLiteral("结束符"), QStringLiteral("未获取")) + QStringLiteral("\n");
        detailText += QStringLiteral("提示:") + parseMessage;
    }

    appendCard(section, section.title, detailText.trimmed(), QString(), true);

    if (messageOut)
    {
        *messageOut = parseMessage;
    }

    return section;
}

[[maybe_unused]] FrameParseSection buildDataSection(const QByteArray &frame, const MeterModel *meterModel)
{
    FrameParseSection section;
    section.title = QString::fromUtf8("数据解析");

    if (frame.isEmpty())
    {
        appendCard(section, section.title, QString::fromUtf8("空帧，暂无可解析内容。"));
        return section;
    }

    const QByteArray protocolFrame = DLT645DataHelper::stripLeadingFE(frame);
    if (protocolFrame.size() < 12)
    {
        appendCard(section,
                   section.title,
                   QString::fromUtf8("无可解析数据"),
                   QString::fromUtf8("报文长度不足，未形成完整协议帧。"));
        return section;
    }

    const quint8 controlCode = static_cast<quint8>(protocolFrame[8]);
    const quint8 dataLength = static_cast<quint8>(protocolFrame[9]);
    const int actualDataLength = qMin(static_cast<int>(dataLength), qMax(0, protocolFrame.size() - 12));
    const QByteArray dataField = protocolFrame.mid(10, actualDataLength);
    const quint8 baseControlCode = controlCode & 0x7F;
    const bool isResponse = (controlCode & 0x80) != 0;

    if (dataField.isEmpty())
    {
        appendCard(section, section.title, QString::fromUtf8("当前帧无数据域。"));
        return section;
    }

    if (baseControlCode == 0x13)
    {
        QString dataText;
        if (isResponse)
        {
            const QByteArray decodedAddressBytes = DLT645DataHelper::decodeReversedPayload(dataField);
            dataText += lineWithValue(QString::fromUtf8("载荷类型："), QString::fromUtf8("地址响应"));
            dataText += lineWithValue(QString::fromUtf8("解码字节："), DLT645FrameFormatter::formatHex(decodedAddressBytes));

            if (decodedAddressBytes.size() == 6)
            {
                dataText += lineWithValue(QString::fromUtf8("解析值："), ProtocolParser::bytesToAddress(decodedAddressBytes));
            }
            else
            {
                dataText += lineWithValue(QString::fromUtf8("解析说明："), QString::fromUtf8("地址响应长度异常，未能解析为 6 字节地址。"));
            }
        }
        else
        {
            dataText += lineWithValue(QString::fromUtf8("载荷类型："), QString::fromUtf8("读地址请求"));
            dataText += lineWithValue(QString::fromUtf8("解析说明："), QString::fromUtf8("该请求帧不携带数据域，用于读取目标电表通信地址。"));
        }

        appendCard(section, section.title, dataText.trimmed());
        return section;
    }

    if (baseControlCode == 0x1C)
    {
        QString dataText;
        const QByteArray decodedDataField = DLT645DataHelper::subtract33(dataField);
        dataText += lineWithValue(QString::fromUtf8("报文类型："), QString::fromUtf8(isResponse ? "远程控制应答" : "远程控制请求"));
        dataText += lineWithValue(QString::fromUtf8("原始DATA："), DLT645FrameFormatter::formatHex(dataField));
        dataText += lineWithValue(QString::fromUtf8("-33DATA："), DLT645FrameFormatter::formatHex(decodedDataField));

        if (decodedDataField.size() >= 8)
        {
            const QByteArray authorityBytes = decodedDataField.left(4);
            const QByteArray operatorBytes = decodedDataField.mid(4, 4);
            const QByteArray commandBytes = decodedDataField.mid(8);
            const quint8 permission = static_cast<quint8>(authorityBytes.at(0));
            dataText += lineWithValue(QString::fromUtf8("权限："), bcdByteText(permission));
            dataText += lineWithValue(QString::fromUtf8("密码："), formatPasswordDigits(authorityBytes.mid(1, 3)));
            dataText += lineWithValue(QString::fromUtf8("操作者代码："), formatOperatorCodeDigits(operatorBytes));
            if (!commandBytes.isEmpty())
            {
                dataText += lineWithValue(QString::fromUtf8("控制命令："), formatRemoteControlCommandDetail(commandBytes));
                dataText += lineWithValue(QString::fromUtf8("命令字节："), DLT645FrameFormatter::formatHex(commandBytes));
            }
        }
        else if (!decodedDataField.isEmpty())
        {
            dataText += lineWithValue(QString::fromUtf8("解析说明："), QString::fromUtf8("远程控制数据长度不足，无法拆分权限/密码/操作者代码。"));
        }
        else
        {
            dataText += lineWithValue(QString::fromUtf8("解析说明："), QString::fromUtf8("远程控制应答通常无数据域。"));
        }

        appendCard(section, section.title, dataText.trimmed());
        return section;
    }

    if (baseControlCode != 0x11 && baseControlCode != 0x14)
    {
        const QString value = QString::fromUtf8("解析说明：当前控制码暂无细分展示。\n控制码：%1 (%2)\n原始数据：%3")
                                  .arg(formatByteHex(controlCode),
                                       DLT645FrameFormatter::controlCodeName(controlCode),
                                       DLT645FrameFormatter::formatHex(dataField));
        appendCard(section, section.title, value);
        return section;
    }

    QString dataText;
    QString diCode;
    QString description;
    QString unit;
    QByteArray decodedPayload;
    QString payloadLabel;
    DataItem diItem;

    // 检查是否为异常应答（控制码 bit6=1 表示异常）
    const bool isErrorResponse = isResponse && (controlCode & 0x40) != 0;
    if (isErrorResponse && dataField.size() == 1)
    {
        const QByteArray decodedDataField = DLT645DataHelper::subtract33(dataField);
        const quint8 errorCode = static_cast<quint8>(decodedDataField[0]);
        const QString errorDescription = StatusWordBitCatalog::describeErrorInfoByte(errorCode);
        dataText += lineWithValue(QString::fromUtf8("报文类型："), QString::fromUtf8("异常应答"));
        dataText += lineWithValue(QString::fromUtf8("错误信息字ERR："), formatByteHex(errorCode));
        dataText += lineWithValue(QString::fromUtf8("错误描述："), errorDescription);
        appendCard(section, section.title, dataText.trimmed());
        return section;
    }

    if (dataField.size() >= 4)
    {
        diCode = ProtocolParser::extractDIFromDataField(dataField);
        dataText += lineWithValue(QString::fromUtf8("数据标识："), diCode);

        if (meterModel)
        {
            diItem = meterModel->getDataItem(diCode);
            if (!diItem.diCode.isEmpty())
            {
                description = diItem.description;
                unit = diItem.unit;
                dataText += lineWithValue(QString::fromUtf8("数据项名称："), description);
            }
            else
            {
                dataText += lineWithValue(QString::fromUtf8("数据项名称："), QString::fromUtf8("未找到匹配数据项"));
            }
        }
    }
    else
    {
        const QString value = QString::fromUtf8("解析说明：数据域长度不足，无法提取数据标识。\n原始数据：%1")
                                  .arg(DLT645FrameFormatter::formatHex(dataField));
        appendCard(section, section.title, value);
        return section;
    }

    if (baseControlCode == 0x11)
    {
        if (isResponse)
        {
            decodedPayload = DLT645DataHelper::subtract33(dataField.mid(4));
            payloadLabel = QString::fromUtf8("应答数据");
        }
        else
        {
            decodedPayload = dataField.mid(4);
            payloadLabel = QString::fromUtf8("请求参数");
        }
    }
    else if (isResponse)
    {
        decodedPayload = dataField.mid(5);
        payloadLabel = QString::fromUtf8("写入状态");
    }
    else
    {
        const bool hasLegacyPayloadLength =
            dataField.size() > 5 && static_cast<quint8>(dataField[4]) == dataField.size() - 5;

        if (hasLegacyPayloadLength)
        {
            const int payloadLength = static_cast<quint8>(dataField[4]);
            decodedPayload = DLT645DataHelper::subtract33(dataField.mid(5, payloadLength > 0 ? payloadLength : -1));
            payloadLabel = QString::fromUtf8("写入数据");
        }
        else if (dataField.size() >= 12)
        {
            const QByteArray authorityBytes = DLT645DataHelper::subtract33(dataField.mid(4, 8));
            dataText += lineWithValue(QString::fromUtf8("权限："), DLT645FrameFormatter::formatHex(authorityBytes.left(1), false));
            dataText += lineWithValue(QString::fromUtf8("密码："), formatPasswordDigits(authorityBytes.mid(1, 3)));
            dataText += lineWithValue(QString::fromUtf8("操作代码："), DLT645FrameFormatter::formatHex(authorityBytes.mid(4, 4), false));

            decodedPayload = DLT645DataHelper::subtract33(dataField.mid(12));
            payloadLabel = QString::fromUtf8("写入数据");
        }
    }

    if (!payloadLabel.isEmpty())
    {
        dataText += lineWithValue(QString::fromUtf8("载荷类型："), payloadLabel);
    }

    if (!decodedPayload.isEmpty())
    {
        dataText += lineWithValue(QString::fromUtf8("原始DATA："), DLT645FrameFormatter::formatHex(dataField));
        dataText += lineWithValue(QString::fromUtf8("-33DATA："), DLT645FrameFormatter::formatHex(DLT645DataHelper::subtract33(dataField)));
        dataText += lineWithValue(QString::fromUtf8("解码字节："), DLT645FrameFormatter::formatHex(decodedPayload));

        const QList<FrameParseCard> blockCards = DLT645ValueFormatter::buildBlockDecodeCards(meterModel, diCode, decodedPayload);
        if (!blockCards.isEmpty())
        {
            QStringList blockLines;
            for (const FrameParseCard &card : blockCards)
            {
                QString line;
                QString explanation = card.title;
                if (!card.detail.isEmpty())
                {
                    QString compactDetail = card.detail;
                    const QString separator(1, QChar(0xFF0C));
                    compactDetail.replace(QChar(0x000A), separator);
                    explanation += separator + compactDetail;
                }
                const QString valueText = card.value.isEmpty()
                                              ? QString(1, QChar(0x65E0))
                                              : card.value;
                const QString explanationPrefix = QString(1, QChar(0xFF08))
                                                  + QChar(0x8BF4)
                                                  + QChar(0x660E)
                                                  + QChar(0xFF1A);
                line = valueText + explanationPrefix + explanation + QString(1, QChar(0xFF09));
                blockLines.append(line);
            }
            dataText += lineWithValue(QString::fromUtf8("解析值："), blockLines.join(QStringLiteral("\n")));
        }
        else
        {
            QString decodeDescription = description;
            QString decodeUnit = unit;
            const QString decodedValue = diCode.isEmpty()
                                             ? DLT645FrameFormatter::formatHex(decodedPayload)
                                             : DLT645ValueFormatter::decodedValueForItem(meterModel, diCode, decodedPayload, &decodeDescription, &decodeUnit);

            if (!decodedValue.isEmpty())
            {
                dataText += lineWithValue(QString::fromUtf8("解析值："), decodedValueWithFieldDescriptions(diCode, appendUnit(decodedValue, decodeUnit)));
                if (!decodeDescription.isEmpty() && decodeDescription != description)
                {
                    dataText += lineWithValue(QString::fromUtf8("解析说明："), decodeDescription);
                }
            }
            else
            {
                dataText += lineWithValue(QString::fromUtf8("解析说明："), QString::fromUtf8("当前数据暂未能进一步解码。"));
            }
        }
    }
    else
    {
        dataText += lineWithValue(QString::fromUtf8("解析说明："), QString::fromUtf8("该帧未携带附加数据。"));
    }

    if (meterModel && !diCode.isEmpty() && isResponse && !diItem.diCode.isEmpty() && !diItem.format.contains(':'))
    {
        const QVariant modelValue = meterModel->getDataValue(diCode);
        if (modelValue.isValid())
        {
            DataItem modelItem = diItem;
            modelItem.value = modelValue;
            const QString modelText = modelItem.getFormattedValue();
            if (!modelText.isEmpty())
            {
                dataText += lineWithValue(QString::fromUtf8("MeterModel 当前值："), appendUnit(modelText, unit));
            }
        }
    }

    appendCard(section, section.title, dataText.trimmed());
    return section;
}

QString renderDisplayResultAsText(const FrameParseDisplayResult &result)
{
    QString detail;

    if (!result.errorText.isEmpty())
    {
        detail += result.errorText + QStringLiteral("\n\n");
    }
    else if (!result.noticeText.isEmpty())
    {
        detail += result.noticeText + QStringLiteral("\n\n");
    }

    for (int sectionIndex = 0; sectionIndex < result.sections.size(); ++sectionIndex)
    {
        const FrameParseSection &section = result.sections.at(sectionIndex);
        detail += section.title + QString::fromUtf8("：\n");

        if (section.cards.isEmpty())
        {
            detail += section.placeholderText.isEmpty() ? QString::fromUtf8("无。\n") : (section.placeholderText + QStringLiteral("\n"));
        }
        else
        {
            for (const FrameParseCard &card : section.cards)
            {
                detail += lineWithValue(card.title + QString::fromUtf8("："), card.value);
                if (!card.detail.isEmpty())
                {
                    detail += card.detail + QStringLiteral("\n");
                }
            }
        }

        if (sectionIndex + 1 < result.sections.size())
        {
            detail += QStringLiteral("\n");
        }
    }

    return detail;
}
}

QString DLT645FrameFormatter::formatHex(const QByteArray &data, bool withSpaces)
{
    return QString::fromLatin1(withSpaces ? data.toHex(' ') : data.toHex()).toUpper();
}

QString DLT645FrameFormatter::controlCodeName(quint8 code)
{
    switch (code)
    {
    case 0x11:
        return QString::fromUtf8("读数据");
    case 0x12:
        return QString::fromUtf8("读后续数据");
    case 0x13:
        return QString::fromUtf8("读通信地址");
    case 0x14:
        return QString::fromUtf8("写数据");
    case 0x08:
        return QString::fromUtf8("广播校时");
    case 0x15:
        return QString::fromUtf8("写通信地址");
    case 0x16:
        return QString::fromUtf8("冻结命令");
    case 0x17:
        return QString::fromUtf8("更改通信速率");
    case 0x18:
        return QString::fromUtf8("修改密码");
    case 0x19:
        return QString::fromUtf8("最大需量清零");
    case 0x1A:
        return QString::fromUtf8("电表清零");
    case 0x1B:
        return QString::fromUtf8("事件清零");
    case 0x1C:
        return QString::fromUtf8("远程控制");
    case 0x9C:
        return QString::fromUtf8("远程控制应答");
    case 0x2A:
        return QString::fromUtf8("读认证状态");
    case 0x2F:
        return QString::fromUtf8("写认证状态");
    case 0x91:
        return QString::fromUtf8("正常应答");
    case 0x92:
        return QString::fromUtf8("后续应答");
    case 0x94:
        return QString::fromUtf8("写数据应答");
    case 0xAA:
        return QString::fromUtf8("读认证状态应答");
    case 0xAF:
        return QString::fromUtf8("写认证状态应答");
    case 0xD1:
        return QString::fromUtf8("异常应答");
    case 0xD2:
        return QString::fromUtf8("不支持");
    default:
        return QString::fromUtf8("未知");
    }
}

QString DLT645FrameFormatter::buildFrameStructureText(const QByteArray &frame)
{
    const FrameParseDisplayResult result = buildParseDisplayResult(frame, nullptr);

    FrameParseDisplayResult structureOnly;
    structureOnly.checksumValid = result.checksumValid;
    structureOnly.errorText = result.errorText;
    structureOnly.noticeText = result.noticeText;
    if (!result.sections.isEmpty())
    {
        structureOnly.sections.append(result.sections.first());
    }

    return renderDisplayResultAsText(structureOnly);
}

FrameParseDisplayResult DLT645FrameFormatter::buildParseDisplayResult(const QByteArray &frame, const MeterModel *meterModel)
{
    return buildParseDisplayResult(frame, meterModel, calculateChecksumValidity(frame));
}

FrameParseDisplayResult DLT645FrameFormatter::buildParseDisplayResult(const QByteArray &frame, const MeterModel *meterModel, bool checksumValid)
{
    FrameParseDisplayResult result;
    result.checksumValid = checksumValid;

    QString structureMessage;
    result.sections.append(buildDetailedFrameSection(frame, meterModel, checksumValid, &structureMessage));

    if (frame.isEmpty())
    {
        result.errorText = QString::fromUtf8("空帧，无法解析。");
    }
    else
    {
        const QByteArray protocolFrame = DLT645DataHelper::stripLeadingFE(frame);
        if (protocolFrame.size() < 12)
        {
            result.errorText = QString::fromUtf8("报文长度不足，无法完成完整帧解析。");
        }
        else
        {
            const quint8 controlCode = static_cast<quint8>(protocolFrame[8]);
            const quint8 dataLength = static_cast<quint8>(protocolFrame[9]);
            const int expectedSize = 12 + ProtocolParser::effectiveDataFieldLength(controlCode, dataLength);
            if (protocolFrame.size() < expectedSize)
            {
                result.noticeText = structureMessage;
            }
            else if (!checksumValid)
            {
                result.noticeText = QString::fromUtf8("校验和无效，请核对原始帧。");
            }
        }
    }

    return result;
}

QString DLT645FrameFormatter::buildParseText(const QByteArray &frame, const MeterModel *meterModel)
{
    return renderDisplayResultAsText(buildParseDisplayResult(frame, meterModel));
}

QString DLT645FrameFormatter::buildParseText(const QByteArray &frame, const MeterModel *meterModel, bool checksumValid)
{
    return renderDisplayResultAsText(buildParseDisplayResult(frame, meterModel, checksumValid));
}
