#include "DLT645FrameFormatter.h"

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
constexpr int kDefaultRateCount = 5;
constexpr int kMaxRateCount = kDefaultRateCount;
constexpr int kHarmonicOrderCount = 21;

bool isHistoryItem(const QString &diCode);
QStringList fieldNamesForDecodedValue(const QString &diCode);
QString decodedValueWithFieldDescriptions(const QString &diCode, const QString &decodedValue);

QString formatByteHex(quint8 value)
{
    return QStringLiteral("0x%1").arg(value, 2, 16, QChar('0')).toUpper();
}

QString upperHexByte(int value)
{
    return QStringLiteral("%1").arg(value, 2, 16, QChar('0')).toUpper();
}

bool isHarmonicBlockCode(const QString &diCode, const QString &familyPrefix)
{
    if (diCode.size() != 10 || !diCode.startsWith(familyPrefix) || !diCode.endsWith(QStringLiteral("FF")))
    {
        return false;
    }

    bool ok = false;
    const int phaseIndex = diCode.mid(6, 2).toInt(&ok, 16);
    return ok && phaseIndex >= 1 && phaseIndex <= 3;
}

QStringList harmonicPayloadOrderCodes(const QString &blockCode)
{
    const QString prefix = blockCode.left(8);
    QStringList codes;
    codes.reserve(kHarmonicOrderCount);
    for (int harmonicOrder = kHarmonicOrderCount; harmonicOrder >= 1; --harmonicOrder)
    {
        codes.append(prefix + upperHexByte(harmonicOrder));
    }
    return codes;
}

QStringList harmonicDisplayOrderCodes(const QString &blockCode)
{
    const QString prefix = blockCode.left(8);
    QStringList codes;
    codes.reserve(kHarmonicOrderCount);
    for (int harmonicOrder = 1; harmonicOrder <= kHarmonicOrderCount; ++harmonicOrder)
    {
        codes.append(prefix + upperHexByte(harmonicOrder));
    }
    return codes;
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

QString controlCodeDetail(quint8 code)
{
    const quint8 functionCode = code & 0x1F;
    QString functionText;
    switch (functionCode)
    {
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
        functionText = QStringLiteral("广播校时");
        break;
    case 0x16:
        functionText = QStringLiteral("写设备地址");
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
    parts.append((code & 0x20) != 0 ? QStringLiteral("有后续数据") : QStringLiteral("无后续数据"));
    return parts.join(QStringLiteral("，"));
}

int bcdByteToInt(quint8 value, bool *ok = nullptr)
{
    const int high = (value >> 4) & 0x0F;
    const int low = value & 0x0F;
    const bool valid = high <= 9 && low <= 9;
    if (ok)
    {
        *ok = valid;
    }
    return high * 10 + low;
}

QString displayBcdDigits(const QByteArray &bytes, bool reverseBytes)
{
    QByteArray normalized = bytes;
    if (reverseBytes)
    {
        std::reverse(normalized.begin(), normalized.end());
    }

    QString digits;
    digits.reserve(normalized.size() * 2);
    for (int i = 0; i < normalized.size(); ++i)
    {
        const quint8 byte = static_cast<quint8>(normalized[i]);
        digits += QStringLiteral("%1").arg(byte, 2, 16, QChar('0')).toUpper();
    }
    return digits;
}

QString displayDateTimeSeconds(const QByteArray &bytes)
{
    if (bytes.size() != 6)
    {
        return QString();
    }

    bool ok = true;
    const int second = bcdByteToInt(static_cast<quint8>(bytes[0]), &ok);
    const int minute = ok ? bcdByteToInt(static_cast<quint8>(bytes[1]), &ok) : 0;
    const int hour = ok ? bcdByteToInt(static_cast<quint8>(bytes[2]), &ok) : 0;
    const int day = ok ? bcdByteToInt(static_cast<quint8>(bytes[3]), &ok) : 0;
    const int month = ok ? bcdByteToInt(static_cast<quint8>(bytes[4]), &ok) : 0;
    const int year = ok ? bcdByteToInt(static_cast<quint8>(bytes[5]), &ok) : 0;
    if (!ok)
    {
        return QString();
    }

    return QStringLiteral("%1年%2月%3日 %4时%5分%6秒")
        .arg(year, 2, 10, QChar('0'))
        .arg(month, 2, 10, QChar('0'))
        .arg(day, 2, 10, QChar('0'))
        .arg(hour, 2, 10, QChar('0'))
        .arg(minute, 2, 10, QChar('0'))
        .arg(second, 2, 10, QChar('0'));
}

QString displayDateTimeMinutes(const QByteArray &bytes)
{
    if (bytes.size() != 5)
    {
        return QString();
    }

    bool ok = true;
    const int minute = bcdByteToInt(static_cast<quint8>(bytes[0]), &ok);
    const int hour = ok ? bcdByteToInt(static_cast<quint8>(bytes[1]), &ok) : 0;
    const int day = ok ? bcdByteToInt(static_cast<quint8>(bytes[2]), &ok) : 0;
    const int month = ok ? bcdByteToInt(static_cast<quint8>(bytes[3]), &ok) : 0;
    const int year = ok ? bcdByteToInt(static_cast<quint8>(bytes[4]), &ok) : 0;
    if (!ok)
    {
        return QString();
    }

    return QStringLiteral("%1年%2月%3日 %4时%5分")
        .arg(year, 2, 10, QChar('0'))
        .arg(month, 2, 10, QChar('0'))
        .arg(day, 2, 10, QChar('0'))
        .arg(hour, 2, 10, QChar('0'))
        .arg(minute, 2, 10, QChar('0'));
}

QString displayDecimalFromBcd(const QByteArray &bytes, int decimalDigits)
{
    if (bytes.isEmpty() || !BCDHelper::isValidBCD(bytes))
    {
        return QString();
    }

    QString digits = displayBcdDigits(bytes, true);
    if (decimalDigits > 0)
    {
        if (digits.length() <= decimalDigits)
        {
            digits = digits.rightJustified(decimalDigits + 1, QChar('0'));
        }
        digits.insert(digits.length() - decimalDigits, QChar('.'));
    }
    return digits;
}

QString displayValueForFormatPart(const QByteArray &bytes, const QString &formatPart)
{
    const QString trimmedFormat = formatPart.trimmed();
    if (trimmedFormat == QStringLiteral("AA,AA,AA,AA,AA,AA"))
    {
        const QString dateTime = displayDateTimeSeconds(bytes);
        if (!dateTime.isEmpty())
        {
            return dateTime;
        }
    }
    if (trimmedFormat == QStringLiteral("AA,AA,AA,AA,AA"))
    {
        const QString dateTime = displayDateTimeMinutes(bytes);
        if (!dateTime.isEmpty())
        {
            return dateTime;
        }
    }
    if (trimmedFormat == QStringLiteral("AA,AA,AA,.,AA"))
    {
        return displayDecimalFromBcd(bytes, 2);
    }
    if (trimmedFormat == QStringLiteral("AA,.,AAAA") ||
        trimmedFormat == QStringLiteral("AA,.,AAA") ||
        trimmedFormat == QStringLiteral("AA,.,AA") ||
        trimmedFormat == QStringLiteral("AA,.,A"))
    {
        QString decimalPart = trimmedFormat.mid(trimmedFormat.indexOf(QChar('.')) + 1);
        decimalPart.remove(QChar(','));
        const int decimalDigits = decimalPart.length();
        return displayDecimalFromBcd(bytes, qMax(0, decimalDigits));
    }
    if (trimmedFormat == QStringLiteral("AA,AA,AA,AA"))
    {
        return displayBcdDigits(bytes, true);
    }

    return displayDecimalFromBcd(bytes, 0);
}

QString recordIndexText(const QString &diCode)
{
    bool ok = false;
    const int index = diCode.mid(8, 2).toInt(&ok, 16);
    return ok && index > 0 ? QString::number(index) : QString();
}

QString inferredDescriptionForDi(const QString &diCode)
{
    if (diCode.startsWith(QStringLiteral("0x03300D")) && isHistoryItem(diCode))
    {
        const QString index = recordIndexText(diCode);
        return index.isEmpty()
                   ? QStringLiteral("开表盖事件记录")
                   : QStringLiteral("上%1次开表盖事件记录").arg(index);
    }

    return QString();
}

QStringList coverOpenEventFieldNames()
{
    const QStringList energyTypes = {
        QStringLiteral("正向有功总电能"),
        QStringLiteral("反向有功总电能"),
        QStringLiteral("第一象限无功总电能"),
        QStringLiteral("第二象限无功总电能"),
        QStringLiteral("第三象限无功总电能"),
        QStringLiteral("第四象限无功总电能")};

    QStringList names = {QStringLiteral("发生时刻"), QStringLiteral("结束时刻")};
    for (const QString &energyType : energyTypes)
    {
        names.append(QStringLiteral("开表盖前%1").arg(energyType));
    }
    for (const QString &energyType : energyTypes)
    {
        names.append(QStringLiteral("开表盖后%1").arg(energyType));
    }
    return names;
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
    if (diCode.startsWith(QStringLiteral("0x03300D")) && isHistoryItem(diCode))
    {
        return coverOpenEventFormat();
    }

    return QString();
}

QString inferredUnitForDi(const QString &diCode, int fieldIndex)
{
    if (diCode.startsWith(QStringLiteral("0x03300D")) && isHistoryItem(diCode) && fieldIndex >= 2)
    {
        const int energyIndex = (fieldIndex - 2) % 6;
        return energyIndex < 2 ? QStringLiteral("kWh") : QStringLiteral("kvarh");
    }

    return QString();
}

QString displayDiCode(const QString &diCode)
{
    QString code = diCode;
    code.remove(QStringLiteral("0x"));
    return code.toUpper();
}

QString byteBreakdownLines(const QString &diCode,
                           const QByteArray &payload,
                           const QString &format,
                           const QString &fallbackDecodedValue,
                           const QString &unit)
{
    const QStringList fieldNames = fieldNamesForDecodedValue(diCode);
    const QStringList formatParts = format.split(QStringLiteral(":"), Qt::SkipEmptyParts);
    if (formatParts.isEmpty())
    {
        return decodedValueWithFieldDescriptions(diCode, appendUnit(fallbackDecodedValue, unit));
    }

    QStringList lines;
    int offset = 0;
    for (int i = 0; i < formatParts.size(); ++i)
    {
        const int size = DLT645DataHelper::bytesRequiredForFormat(formatParts.at(i));
        if (size <= 0 || offset + size > payload.size())
        {
            return decodedValueWithFieldDescriptions(diCode, appendUnit(fallbackDecodedValue, unit));
        }

        const QByteArray fieldBytes = payload.mid(offset, size);
        const QString fieldName = i < fieldNames.size()
                                      ? fieldNames.at(i)
                                      : (formatParts.size() == 1 ? QStringLiteral("数据值") : QStringLiteral("字段%1").arg(i + 1));
        QString fieldValue = displayValueForFormatPart(fieldBytes, formatParts.at(i));
        if (fieldValue.isEmpty())
        {
            fieldValue = DLT645FrameFormatter::formatHex(fieldBytes);
        }

        const QString fieldUnit = !inferredUnitForDi(diCode, i).isEmpty()
                                      ? inferredUnitForDi(diCode, i)
                                      : unit;
        lines.append(QStringLiteral("%1 --- %2:%3")
                         .arg(DLT645FrameFormatter::formatHex(fieldBytes),
                              fieldName,
                              appendUnit(fieldValue, fieldUnit)));
        offset += size;
    }

    if (offset < payload.size())
    {
        lines.append(annotatedLine(DLT645FrameFormatter::formatHex(payload.mid(offset)),
                                   QStringLiteral("未匹配剩余数据")));
    }

    return lines.join(QStringLiteral("\n"));
}

QString phaseEventName(const QString &diCode)
{
    if (diCode.startsWith(QStringLiteral("0x0301")))
    {
        return QStringLiteral("失压");
    }
    if (diCode.startsWith(QStringLiteral("0x0302")))
    {
        return QStringLiteral("欠压");
    }
    if (diCode.startsWith(QStringLiteral("0x0303")))
    {
        return QStringLiteral("过压");
    }
    if (diCode.startsWith(QStringLiteral("0x0304")))
    {
        return QStringLiteral("断相");
    }

    return QStringLiteral("事件");
}

QStringList phaseVoltageEventFieldNames(const QString &eventName)
{
    const QString prefix = eventName + QStringLiteral("期间");
    const QStringList energyTypes = {
        QStringLiteral("正向有功"),
        QStringLiteral("反向有功"),
        QStringLiteral("组合无功1"),
        QStringLiteral("组合无功2")};
    const QStringList phases = {QStringLiteral("A相"), QStringLiteral("B相"), QStringLiteral("C相")};

    QStringList names = {QStringLiteral("发生时刻"), QStringLiteral("结束时刻")};
    for (const QString &energyType : energyTypes)
    {
        names.append(QStringLiteral("%1%2总电能增量").arg(prefix, energyType));
    }

    for (const QString &phase : phases)
    {
        for (const QString &energyType : energyTypes)
        {
            names.append(QStringLiteral("%1%2%3电能增量").arg(prefix, phase, energyType));
        }
        names.append(QStringLiteral("%1时刻%2电压").arg(eventName, phase));
        names.append(QStringLiteral("%1时刻%2电流").arg(eventName, phase));
        names.append(QStringLiteral("%1时刻%2有功功率").arg(eventName, phase));
        names.append(QStringLiteral("%1时刻%2无功功率").arg(eventName, phase));
        names.append(QStringLiteral("%1时刻%2功率因数").arg(eventName, phase));
    }

    names.append(QStringLiteral("%1总安时数").arg(prefix));
    for (const QString &phase : phases)
    {
        names.append(QStringLiteral("%1%2安时数").arg(prefix, phase));
    }

    return names;
}

QStringList totalAndPhaseEnergyNames(const QString &prefix, const QStringList &energyTypes, const QString &suffix)
{
    QStringList names;
    for (const QString &energyType : energyTypes)
    {
        names.append(QStringLiteral("%1%2总电能%3").arg(prefix, energyType, suffix));
    }

    const QStringList phases = {QStringLiteral("A相"), QStringLiteral("B相"), QStringLiteral("C相")};
    for (const QString &phase : phases)
    {
        for (const QString &energyType : energyTypes)
        {
            names.append(QStringLiteral("%1%2%3电能%4").arg(prefix, phase, energyType, suffix));
        }
    }

    return names;
}

QStringList sequenceEventFieldNames(const QString &eventName)
{
    const QStringList energyTypes = {
        QStringLiteral("正向有功"),
        QStringLiteral("反向有功"),
        QStringLiteral("组合无功1"),
        QStringLiteral("组合无功2")};
    QStringList names = {QStringLiteral("发生时刻"), QStringLiteral("结束时刻")};
    names.append(totalAndPhaseEnergyNames(eventName + QStringLiteral("期间"), energyTypes, QStringLiteral("增量")));
    return names;
}

QStringList unbalanceEventFieldNames(const QString &eventName)
{
    QStringList names = {QStringLiteral("发生时刻"),
                         QStringLiteral("结束时刻"),
                         QStringLiteral("最大不平衡率")};
    names.append(sequenceEventFieldNames(eventName).mid(2));
    return names;
}

QStringList meterClearFieldNames()
{
    const QStringList energyTypes = {
        QStringLiteral("正向有功"),
        QStringLiteral("反向有功"),
        QStringLiteral("第一象限无功"),
        QStringLiteral("第二象限无功"),
        QStringLiteral("第三象限无功"),
        QStringLiteral("第四象限无功")};
    QStringList names = {QStringLiteral("发生时刻"), QStringLiteral("操作者代码")};
    names.append(totalAndPhaseEnergyNames(QStringLiteral("电表清零前"), energyTypes, QString()));
    return names;
}

QStringList demandClearFieldNames()
{
    const QStringList demandTypes = {
        QStringLiteral("正向有功"),
        QStringLiteral("反向有功"),
        QStringLiteral("第一象限无功"),
        QStringLiteral("第二象限无功"),
        QStringLiteral("第三象限无功"),
        QStringLiteral("第四象限无功")};
    const QStringList phases = {QStringLiteral("A相"), QStringLiteral("B相"), QStringLiteral("C相")};

    QStringList baseNames;
    for (const QString &demandType : demandTypes)
    {
        baseNames.append(QStringLiteral("需量清零前%1总最大需量").arg(demandType));
    }
    for (const QString &phase : phases)
    {
        for (const QString &demandType : demandTypes)
        {
            baseNames.append(QStringLiteral("需量清零前%1%2最大需量").arg(phase, demandType));
        }
    }

    QStringList names = {QStringLiteral("发生时刻"), QStringLiteral("操作者代码")};
    for (const QString &baseName : baseNames)
    {
        names.append(baseName);
        names.append(baseName + QStringLiteral("发生时间"));
    }

    return names;
}

QStringList voltageComplianceFieldNames(const QString &phaseName)
{
    const QString prefix = phaseName.isEmpty() ? QStringLiteral("电压") : phaseName + QStringLiteral("电压");
    return {prefix + QStringLiteral("监测时间"),
            prefix + QStringLiteral("合格率"),
            prefix + QStringLiteral("超限率"),
            prefix + QStringLiteral("超上限时间"),
            prefix + QStringLiteral("超下限时间"),
            phaseName + QStringLiteral("最高电压"),
            phaseName + QStringLiteral("最高电压出现时间"),
            phaseName + QStringLiteral("最低电压"),
            phaseName + QStringLiteral("最低电压出现时间")};
}

QStringList scheduleTableProgramFieldNames()
{
    QStringList names = {QStringLiteral("发生时刻"), QStringLiteral("操作者代码")};
    for (int setIndex = 1; setIndex <= 2; ++setIndex)
    {
        for (int tableIndex = 1; tableIndex <= 8; ++tableIndex)
        {
            names.append(QStringLiteral("时段表编程前第%1套第%2日时段表数据").arg(setIndex).arg(tableIndex));
        }
    }

    return names;
}

QStringList yearZoneProgramFieldNames()
{
    QStringList names = {QStringLiteral("发生时刻"), QStringLiteral("操作者代码")};
    for (int setIndex = 1; setIndex <= 2; ++setIndex)
    {
        for (int zoneIndex = 1; zoneIndex <= 14; ++zoneIndex)
        {
            names.append(QStringLiteral("时区表编程前第%1套第%2时区表数据").arg(setIndex).arg(zoneIndex));
        }
    }

    return names;
}

bool isHistoryItem(const QString &diCode)
{
    return diCode.size() >= 10 && diCode.mid(8, 2) != QStringLiteral("00");
}

QStringList fieldNamesForDecodedValue(const QString &diCode)
{
    const bool isPhaseVoltageFamily = diCode.startsWith(QStringLiteral("0x0301")) ||
                                      diCode.startsWith(QStringLiteral("0x0302")) ||
                                      diCode.startsWith(QStringLiteral("0x0303")) ||
                                      diCode.startsWith(QStringLiteral("0x0304"));
    const QString thirdByte = diCode.size() >= 8 ? diCode.mid(6, 2) : QString();

    if (isPhaseVoltageFamily && thirdByte != QStringLiteral("00") && isHistoryItem(diCode))
    {
        return phaseVoltageEventFieldNames(phaseEventName(diCode));
    }

    if (((isPhaseVoltageFamily && thirdByte == QStringLiteral("00")) ||
         diCode == QStringLiteral("0x03050000") ||
         diCode == QStringLiteral("0x03060000") ||
         diCode == QStringLiteral("0x03070000") ||
         diCode == QStringLiteral("0x03080000") ||
         diCode == QStringLiteral("0x03090000")) &&
        (!isHistoryItem(diCode) || isPhaseVoltageFamily))
    {
        return {QStringLiteral("总次数"), QStringLiteral("总累计时间")};
    }

    if (diCode.startsWith(QStringLiteral("0x030500")) && isHistoryItem(diCode))
    {
        return {QStringLiteral("全失压开始时间"),
                QStringLiteral("全失压时电流"),
                QStringLiteral("全失压结束时间")};
    }

    if ((diCode.startsWith(QStringLiteral("0x030600")) ||
         diCode.startsWith(QStringLiteral("0x031100"))) &&
        isHistoryItem(diCode))
    {
        return {QStringLiteral("事件开始时间"), QStringLiteral("事件结束时间")};
    }

    if ((diCode.startsWith(QStringLiteral("0x030700")) ||
         diCode.startsWith(QStringLiteral("0x030800"))) &&
        isHistoryItem(diCode))
    {
        const QString eventName = diCode.startsWith(QStringLiteral("0x030700"))
                                      ? QStringLiteral("电压逆相序")
                                      : QStringLiteral("电流逆相序");
        return sequenceEventFieldNames(eventName);
    }

    if (diCode.startsWith(QStringLiteral("0x030900")) && isHistoryItem(diCode))
    {
        return unbalanceEventFieldNames(QStringLiteral("电压不平衡"));
    }

    if (diCode.startsWith(QStringLiteral("0x0310")))
    {
        QString phaseName;
        if (diCode.startsWith(QStringLiteral("0x031001")))
        {
            phaseName = QStringLiteral("A相");
        }
        else if (diCode.startsWith(QStringLiteral("0x031002")))
        {
            phaseName = QStringLiteral("B相");
        }
        else if (diCode.startsWith(QStringLiteral("0x031003")))
        {
            phaseName = QStringLiteral("C相");
        }
        return voltageComplianceFieldNames(phaseName);
    }

    if (diCode == QStringLiteral("0x03120000"))
    {
        return {QStringLiteral("正向有功需量超限总次数"),
                QStringLiteral("反向有功需量超限总次数"),
                QStringLiteral("第一象限无功需量超限总次数"),
                QStringLiteral("第二象限无功需量超限总次数"),
                QStringLiteral("第三象限无功需量超限总次数"),
                QStringLiteral("第四象限无功需量超限总次数")};
    }

    if (diCode.startsWith(QStringLiteral("0x0312")) && isHistoryItem(diCode))
    {
        return {QStringLiteral("发生时刻"),
                QStringLiteral("结束时刻"),
                QStringLiteral("需量超限期间最大需量"),
                QStringLiteral("需量超限期间最大需量发生时间")};
    }

    if (diCode.startsWith(QStringLiteral("0x03300D")) && isHistoryItem(diCode))
    {
        return coverOpenEventFieldNames();
    }

    if (diCode.startsWith(QStringLiteral("0x033000")) && isHistoryItem(diCode))
    {
        QStringList names = {QStringLiteral("发生时刻"), QStringLiteral("操作者代码")};
        for (int i = 1; i <= 10; ++i)
        {
            names.append(QStringLiteral("编程数据标识%1").arg(i));
        }
        names.append(QStringLiteral("扩展数据标识1"));
        names.append(QStringLiteral("扩展数据标识2"));
        return names;
    }

    if (diCode.startsWith(QStringLiteral("0x033001")) && isHistoryItem(diCode))
    {
        return meterClearFieldNames();
    }

    if (diCode.startsWith(QStringLiteral("0x033002")) && isHistoryItem(diCode))
    {
        return demandClearFieldNames();
    }

    if (diCode.startsWith(QStringLiteral("0x033003")) && isHistoryItem(diCode))
    {
        return {QStringLiteral("发生时刻"),
                QStringLiteral("操作者代码"),
                QStringLiteral("事件清零数据标识码")};
    }

    if (diCode.startsWith(QStringLiteral("0x033004")) && isHistoryItem(diCode))
    {
        return {QStringLiteral("操作者代码"),
                QStringLiteral("校时前时间"),
                QStringLiteral("校时后时间")};
    }

    if (diCode.startsWith(QStringLiteral("0x033005")) && isHistoryItem(diCode))
    {
        return scheduleTableProgramFieldNames();
    }

    if (diCode.startsWith(QStringLiteral("0x033006")) && isHistoryItem(diCode))
    {
        return yearZoneProgramFieldNames();
    }

    return {};
}

QString decodedValueWithFieldDescriptions(const QString &diCode, const QString &decodedValue)
{
    const QStringList fieldNames = fieldNamesForDecodedValue(diCode);
    if (fieldNames.isEmpty() || !decodedValue.contains(QStringLiteral(" : ")))
    {
        return decodedValue;
    }

    const QStringList values = decodedValue.split(QStringLiteral(" : "));
    QStringList lines;
    lines.reserve(values.size());
    for (int i = 0; i < values.size(); ++i)
    {
        const QString fieldName = i < fieldNames.size()
                                      ? fieldNames.at(i)
                                      : QStringLiteral("字段%1").arg(i + 1);
        lines.append(QStringLiteral("%1：%2").arg(fieldName, values.at(i)));
    }

    return lines.join(QStringLiteral("\n"));
}

[[maybe_unused]] QString decodedValueWithByteBreakdown(const QString &diCode,
                                                       const QByteArray &payload,
                                                       const QString &format,
                                                       const QString &decodedValue,
                                                       const QString &unit)
{
    const QStringList fieldNames = fieldNamesForDecodedValue(diCode);
    const QStringList formatParts = format.split(QStringLiteral(":"), Qt::SkipEmptyParts);
    if (fieldNames.isEmpty() || formatParts.size() <= 1 || !decodedValue.contains(QStringLiteral(" : ")))
    {
        return decodedValueWithFieldDescriptions(diCode, appendUnit(decodedValue, unit));
    }

    const QStringList values = decodedValue.split(QStringLiteral(" : "));
    QStringList lines;
    int offset = 0;
    for (int i = 0; i < formatParts.size(); ++i)
    {
        const int size = DLT645DataHelper::bytesRequiredForFormat(formatParts.at(i));
        if (size <= 0 || offset + size > payload.size())
        {
            return decodedValueWithFieldDescriptions(diCode, appendUnit(decodedValue, unit));
        }

        const QString fieldName = i < fieldNames.size()
                                      ? fieldNames.at(i)
                                      : QStringLiteral("字段%1").arg(i + 1);
        const QString value = i < values.size() ? values.at(i) : QString();
        lines.append(QStringLiteral("%1 --- %2：%3")
                         .arg(DLT645FrameFormatter::formatHex(payload.mid(offset, size)),
                              fieldName,
                              appendUnit(value, unit)));
        offset += size;
    }

    if (offset < payload.size())
    {
        lines.append(QStringLiteral("%1 --- %2")
                         .arg(DLT645FrameFormatter::formatHex(payload.mid(offset)),
                              QStringLiteral("未匹配剩余数据")));
    }

    return lines.join(QStringLiteral("\n"));
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

bool hasModelItem(const MeterModel *meterModel, const QString &diCode)
{
    return meterModel && !meterModel->getDataItem(diCode).diCode.isEmpty();
}

int currentRateCount(const MeterModel *meterModel)
{
    if (!meterModel)
    {
        return kDefaultRateCount;
    }

    bool ok = false;
    int rateCount = meterModel->getDataValue(QStringLiteral("0x04000204")).toInt(&ok);
    if (!ok)
    {
        rateCount = meterModel->getDataItem(QStringLiteral("0x04000204")).value.toInt(&ok);
    }

    return qBound(1, ok ? rateCount : kDefaultRateCount, kMaxRateCount);
}

int effectiveRateCount(const MeterModel *meterModel, int rateCountOverride)
{
    return rateCountOverride > 0
               ? qBound(1, rateCountOverride, kMaxRateCount)
               : currentRateCount(meterModel);
}

bool isDynamicDemandBlock(const MeterModel *meterModel, const QString &diCode)
{
    if (!meterModel || !diCode.endsWith(QStringLiteral("FF00")))
    {
        return false;
    }

    const QString prefix = diCode.left(6);
    const DataItem totalItem = meterModel->getDataItem(prefix + QStringLiteral("0000"));
    return !totalItem.diCode.isEmpty() &&
           hasModelItem(meterModel, prefix + QStringLiteral("0001")) &&
           hasModelItem(meterModel, prefix + QStringLiteral("0100")) &&
           hasModelItem(meterModel, prefix + QStringLiteral("0101"));
}

bool isDynamicEnergyBlock(const MeterModel *meterModel, const QString &diCode)
{
    if (!meterModel || !diCode.endsWith(QStringLiteral("FF00")))
    {
        return false;
    }

    const QString prefix = diCode.left(6);
    const DataItem totalItem = meterModel->getDataItem(prefix + QStringLiteral("0000"));
    return !totalItem.diCode.isEmpty() &&
           totalItem.unit.contains(QChar('h'), Qt::CaseInsensitive) &&
           hasModelItem(meterModel, prefix + QStringLiteral("0100")) &&
           !hasModelItem(meterModel, prefix + QStringLiteral("0001"));
}

bool isDynamicFreezeEnergyBlock(const MeterModel *meterModel, const QString &diCode)
{
    if (!meterModel || diCode.size() != 10 || !diCode.startsWith(QStringLiteral("0x05")) || !diCode.endsWith(QStringLiteral("01")))
    {
        return false;
    }

    bool ok = false;
    const int familyValue = diCode.mid(6, 2).toInt(&ok, 16);
    if (!ok || familyValue < 0x01 || familyValue > 0x08)
    {
        return false;
    }

    const QString totalCode = diCode.left(8) + QStringLiteral("00");
    const DataItem totalItem = meterModel->getDataItem(totalCode);
    return !totalItem.diCode.isEmpty() && totalItem.unit.contains(QChar('h'), Qt::CaseInsensitive);
}

bool isLatestHourlyFreezeCompositeBlock(const QString &diCode)
{
    return diCode == QStringLiteral("0x0504FF01");
}

QStringList dynamicEnergyPayloadOrderCodes(const MeterModel *meterModel, const QString &diCode, int rateCountOverride = -1)
{
    const QString prefix = diCode.left(6);
    const int rateCount = effectiveRateCount(meterModel, rateCountOverride);

    QStringList codes;
    codes.reserve(rateCount + 1);
    for (int rateIndex = rateCount; rateIndex >= 1; --rateIndex)
    {
        codes.append(prefix + upperHexByte(rateIndex) + QStringLiteral("00"));
    }
    codes.append(prefix + QStringLiteral("0000"));
    return codes;
}

QStringList dynamicEnergyDisplayOrderCodes(const MeterModel *meterModel, const QString &diCode, int rateCountOverride = -1)
{
    const QString prefix = diCode.left(6);
    const int rateCount = effectiveRateCount(meterModel, rateCountOverride);

    QStringList codes;
    codes.reserve(rateCount + 1);
    codes.append(prefix + QStringLiteral("0000"));
    for (int rateIndex = 1; rateIndex <= rateCount; ++rateIndex)
    {
        codes.append(prefix + upperHexByte(rateIndex) + QStringLiteral("00"));
    }
    return codes;
}

QStringList dynamicDemandPayloadOrderCodes(const MeterModel *meterModel, const QString &diCode, int rateCountOverride = -1)
{
    const QString prefix = diCode.left(6);
    const int rateCount = effectiveRateCount(meterModel, rateCountOverride);

    QStringList codes;
    codes.reserve((rateCount + 1) * 2);
    for (int rateIndex = rateCount; rateIndex >= 1; --rateIndex)
    {
        const QString rateHex = upperHexByte(rateIndex);
        codes.append(prefix + rateHex + QStringLiteral("01"));
        codes.append(prefix + rateHex + QStringLiteral("00"));
    }
    codes.append(prefix + QStringLiteral("0001"));
    codes.append(prefix + QStringLiteral("0000"));
    return codes;
}

QStringList dynamicDemandDisplayOrderCodes(const MeterModel *meterModel, const QString &diCode, int rateCountOverride = -1)
{
    const QString prefix = diCode.left(6);
    const int rateCount = effectiveRateCount(meterModel, rateCountOverride);

    QStringList codes;
    codes.reserve((rateCount + 1) * 2);
    codes.append(prefix + QStringLiteral("0000"));
    codes.append(prefix + QStringLiteral("0001"));
    for (int rateIndex = 1; rateIndex <= rateCount; ++rateIndex)
    {
        const QString rateHex = upperHexByte(rateIndex);
        codes.append(prefix + rateHex + QStringLiteral("00"));
        codes.append(prefix + rateHex + QStringLiteral("01"));
    }
    return codes;
}

QStringList dynamicFreezeEnergyPayloadOrderCodes(const MeterModel *meterModel, const QString &diCode, int rateCountOverride = -1)
{
    const QString prefix = diCode.left(8);
    const int rateCount = effectiveRateCount(meterModel, rateCountOverride);

    QStringList codes;
    codes.reserve(rateCount + 1);
    for (int rateIndex = rateCount; rateIndex >= 1; --rateIndex)
    {
        codes.append(prefix + upperHexByte(rateIndex + 1));
    }
    codes.append(prefix + QStringLiteral("00"));
    return codes;
}

QStringList dynamicFreezeEnergyDisplayOrderCodes(const MeterModel *meterModel, const QString &diCode, int rateCountOverride = -1)
{
    const QString prefix = diCode.left(8);
    const int rateCount = effectiveRateCount(meterModel, rateCountOverride);

    QStringList codes;
    codes.reserve(rateCount + 1);
    codes.append(prefix + QStringLiteral("00"));
    for (int rateIndex = 1; rateIndex <= rateCount; ++rateIndex)
    {
        codes.append(prefix + upperHexByte(rateIndex + 1));
    }
    return codes;
}

QStringList latestHourlyFreezeCompositeCodes()
{
    return {
        QStringLiteral("0x05040001"),
        QStringLiteral("0x05040100"),
        QStringLiteral("0x05040200")};
}

QStringList payloadOrderCodesForBlock(const MeterModel *meterModel, const QString &diCode, int rateCountOverride = -1)
{
    if (isDynamicEnergyBlock(meterModel, diCode))
    {
        return dynamicEnergyPayloadOrderCodes(meterModel, diCode, rateCountOverride);
    }

    if (isDynamicFreezeEnergyBlock(meterModel, diCode))
    {
        return dynamicFreezeEnergyPayloadOrderCodes(meterModel, diCode, rateCountOverride);
    }

    if (isDynamicDemandBlock(meterModel, diCode))
    {
        return dynamicDemandPayloadOrderCodes(meterModel, diCode, rateCountOverride);
    }

    if (isHarmonicBlockCode(diCode, QStringLiteral("0x020A")) ||
        isHarmonicBlockCode(diCode, QStringLiteral("0x020B")))
    {
        return harmonicPayloadOrderCodes(diCode);
    }

    if (diCode == QStringLiteral("0x0201FF00"))
    {
        return {QStringLiteral("0x02010300"), QStringLiteral("0x02010200"), QStringLiteral("0x02010100")};
    }

    if (diCode == QStringLiteral("0x0202FF00"))
    {
        return {QStringLiteral("0x02020300"), QStringLiteral("0x02020200"), QStringLiteral("0x02020100")};
    }

    if (diCode == QStringLiteral("0x0203FF00"))
    {
        return {QStringLiteral("0x02030300"), QStringLiteral("0x02030200"), QStringLiteral("0x02030100"), QStringLiteral("0x02030000")};
    }

    if (diCode == QStringLiteral("0x0204FF00"))
    {
        return {QStringLiteral("0x02040300"), QStringLiteral("0x02040200"), QStringLiteral("0x02040100"), QStringLiteral("0x02040000")};
    }

    if (diCode == QStringLiteral("0x0205FF00"))
    {
        return {QStringLiteral("0x02050300"), QStringLiteral("0x02050200"), QStringLiteral("0x02050100"), QStringLiteral("0x02050000")};
    }

    if (diCode == QStringLiteral("0x0206FF00"))
    {
        return {QStringLiteral("0x02060300"), QStringLiteral("0x02060200"), QStringLiteral("0x02060100"), QStringLiteral("0x02060000")};
    }

    if (diCode == QStringLiteral("0x0207FF00"))
    {
        return {QStringLiteral("0x02070300"), QStringLiteral("0x02070200"), QStringLiteral("0x02070100")};
    }

    if (diCode == QStringLiteral("0x0208FF00"))
    {
        return {QStringLiteral("0x02080300"), QStringLiteral("0x02080200"), QStringLiteral("0x02080100")};
    }

    if (diCode == QStringLiteral("0x0209FF00"))
    {
        return {QStringLiteral("0x02090300"), QStringLiteral("0x02090200"), QStringLiteral("0x02090100")};
    }

    if (diCode == QStringLiteral("0x230100FF"))
    {
        return {
            QStringLiteral("0x23010008"),
            QStringLiteral("0x23010007"),
            QStringLiteral("0x23010006"),
            QStringLiteral("0x23010005"),
            QStringLiteral("0x23010004"),
            QStringLiteral("0x23010003"),
            QStringLiteral("0x23010002"),
            QStringLiteral("0x23010001")};
    }

    if (isLatestHourlyFreezeCompositeBlock(diCode))
    {
        return latestHourlyFreezeCompositeCodes();
    }

    return {};
}

QStringList displayOrderCodesForBlock(const MeterModel *meterModel, const QString &diCode, int rateCountOverride = -1)
{
    if (isDynamicEnergyBlock(meterModel, diCode))
    {
        return dynamicEnergyDisplayOrderCodes(meterModel, diCode, rateCountOverride);
    }

    if (isDynamicFreezeEnergyBlock(meterModel, diCode))
    {
        return dynamicFreezeEnergyDisplayOrderCodes(meterModel, diCode, rateCountOverride);
    }

    if (isDynamicDemandBlock(meterModel, diCode))
    {
        return dynamicDemandDisplayOrderCodes(meterModel, diCode, rateCountOverride);
    }

    if (isHarmonicBlockCode(diCode, QStringLiteral("0x020A")) ||
        isHarmonicBlockCode(diCode, QStringLiteral("0x020B")))
    {
        return harmonicDisplayOrderCodes(diCode);
    }

    if (diCode == QStringLiteral("0x0201FF00"))
    {
        return {QStringLiteral("0x02010100"), QStringLiteral("0x02010200"), QStringLiteral("0x02010300")};
    }

    if (diCode == QStringLiteral("0x0202FF00"))
    {
        return {QStringLiteral("0x02020100"), QStringLiteral("0x02020200"), QStringLiteral("0x02020300")};
    }

    if (diCode == QStringLiteral("0x0203FF00"))
    {
        return {QStringLiteral("0x02030000"), QStringLiteral("0x02030100"), QStringLiteral("0x02030200"), QStringLiteral("0x02030300")};
    }

    if (diCode == QStringLiteral("0x0204FF00"))
    {
        return {QStringLiteral("0x02040000"), QStringLiteral("0x02040100"), QStringLiteral("0x02040200"), QStringLiteral("0x02040300")};
    }

    if (diCode == QStringLiteral("0x0205FF00"))
    {
        return {QStringLiteral("0x02050000"), QStringLiteral("0x02050100"), QStringLiteral("0x02050200"), QStringLiteral("0x02050300")};
    }

    if (diCode == QStringLiteral("0x0206FF00"))
    {
        return {QStringLiteral("0x02060000"), QStringLiteral("0x02060100"), QStringLiteral("0x02060200"), QStringLiteral("0x02060300")};
    }

    if (diCode == QStringLiteral("0x0207FF00"))
    {
        return {QStringLiteral("0x02070100"), QStringLiteral("0x02070200"), QStringLiteral("0x02070300")};
    }

    if (diCode == QStringLiteral("0x0208FF00"))
    {
        return {QStringLiteral("0x02080100"), QStringLiteral("0x02080200"), QStringLiteral("0x02080300")};
    }

    if (diCode == QStringLiteral("0x0209FF00"))
    {
        return {QStringLiteral("0x02090100"), QStringLiteral("0x02090200"), QStringLiteral("0x02090300")};
    }

    if (diCode == QStringLiteral("0x230100FF"))
    {
        return {
            QStringLiteral("0x23010001"),
            QStringLiteral("0x23010002"),
            QStringLiteral("0x23010003"),
            QStringLiteral("0x23010004"),
            QStringLiteral("0x23010005"),
            QStringLiteral("0x23010006"),
            QStringLiteral("0x23010007"),
            QStringLiteral("0x23010008")};
    }

    if (isLatestHourlyFreezeCompositeBlock(diCode))
    {
        return latestHourlyFreezeCompositeCodes();
    }

    return {};
}

QString decodedValueForItem(const MeterModel *meterModel,
                            const QString &diCode,
                            const QByteArray &payload,
                            QString *descriptionOut,
                            QString *unitOut)
{
    QString decoded = DLT645DataHelper::decodePayloadForItem(meterModel, diCode, payload, descriptionOut, unitOut);

    if (!decoded.isEmpty() && decoded != BCDHelper::bytesToHexString(payload))
    {
        return decoded;
    }

    const DataItem item = meterModel ? meterModel->getDataItem(diCode) : DataItem();
    if (!item.diCode.isEmpty() && BCDHelper::isValidBCD(payload))
    {
        const QString rawDigits = BCDHelper::bytesToHexString(payload, QString());
        return BCDHelper::bcdStringToDecimal(rawDigits, item.decimalPlaces);
    }

    return decoded;
}

QString blockDisplayValueForItem(const MeterModel *meterModel,
                                 const QString &diCode,
                                 const QByteArray &payload,
                                 QString *descriptionOut,
                                 QString *unitOut)
{
    const DataItem item = meterModel ? meterModel->getDataItem(diCode) : DataItem();
    if (!item.diCode.isEmpty())
    {
        if (descriptionOut)
        {
            *descriptionOut = item.description;
        }
        if (unitOut)
        {
            *unitOut = item.unit;
        }
    }

    return decodedValueForItem(meterModel, diCode, payload, descriptionOut, unitOut);
}

bool isRateCountDependentBlock(const MeterModel *meterModel, const QString &diCode)
{
    return isDynamicEnergyBlock(meterModel, diCode) ||
           isDynamicFreezeEnergyBlock(meterModel, diCode) ||
           isDynamicDemandBlock(meterModel, diCode);
}

int payloadSizeForCodes(const MeterModel *meterModel, const QStringList &codes)
{
    if (!meterModel || codes.isEmpty())
    {
        return -1;
    }

    int totalSize = 0;
    for (const QString &code : codes)
    {
        const DataItem item = meterModel->getDataItem(code);
        if (item.diCode.isEmpty())
        {
            return -1;
        }

        const int size = DLT645DataHelper::bytesRequiredForFormat(item.format);
        if (size <= 0)
        {
            return -1;
        }

        totalSize += size;
    }

    return totalSize;
}

QStringList selectPayloadOrderCodesForBlock(const MeterModel *meterModel,
                                            const QString &blockDiCode,
                                            const QByteArray &payload,
                                            int *rateCountOverrideOut)
{
    if (rateCountOverrideOut)
    {
        *rateCountOverrideOut = -1;
    }

    QStringList payloadCodes = payloadOrderCodesForBlock(meterModel, blockDiCode);
    if (payloadSizeForCodes(meterModel, payloadCodes) == payload.size())
    {
        return payloadCodes;
    }

    if (!isRateCountDependentBlock(meterModel, blockDiCode))
    {
        return payloadCodes;
    }

    for (int rateCount = kMaxRateCount; rateCount >= 1; --rateCount)
    {
        const QStringList candidateCodes = payloadOrderCodesForBlock(meterModel, blockDiCode, rateCount);
        if (payloadSizeForCodes(meterModel, candidateCodes) == payload.size())
        {
            if (rateCountOverrideOut)
            {
                *rateCountOverrideOut = rateCount;
            }
            return candidateCodes;
        }
    }

    return payloadCodes;
}

QList<FrameParseCard> buildBlockDecodeCards(const MeterModel *meterModel, const QString &blockDiCode, const QByteArray &payload)
{
    if (!meterModel || blockDiCode.isEmpty() || payload.isEmpty())
    {
        return {};
    }

    int rateCountOverride = -1;
    const QStringList payloadCodes = selectPayloadOrderCodesForBlock(meterModel, blockDiCode, payload, &rateCountOverride);
    if (payloadCodes.isEmpty())
    {
        return {};
    }

    QMap<QString, QString> payloadHexByCode;
    QMap<QString, QString> valuesByCode;
    QMap<QString, QString> descriptionsByCode;
    QMap<QString, QString> unitsByCode;

    int offset = 0;
    for (const QString &code : payloadCodes)
    {
        const DataItem item = meterModel->getDataItem(code);
        if (item.diCode.isEmpty())
        {
            return {};
        }

        const int size = DLT645DataHelper::bytesRequiredForFormat(item.format);
        if (size <= 0 || offset + size > payload.size())
        {
            return {};
        }

        const QByteArray partPayload = payload.mid(offset, size);
        QByteArray displayBytes = partPayload;
        std::reverse(displayBytes.begin(), displayBytes.end());

        QString description;
        QString unit;
        const QString value = blockDisplayValueForItem(meterModel, code, partPayload, &description, &unit);

        payloadHexByCode.insert(code, DLT645FrameFormatter::formatHex(displayBytes));
        valuesByCode.insert(code, value);
        descriptionsByCode.insert(code, description.isEmpty() ? item.description : description);
        unitsByCode.insert(code, unit.isEmpty() ? item.unit : unit);
        offset += size;
    }

    if (offset != payload.size())
    {
        return {};
    }

    const QStringList displayCodes = displayOrderCodesForBlock(meterModel, blockDiCode, rateCountOverride);
    QList<FrameParseCard> cards;
    for (const QString &code : displayCodes)
    {
        if (!valuesByCode.contains(code))
        {
            continue;
        }

        const QString payloadHex = payloadHexByCode.value(code);
        const QString description = descriptionsByCode.value(code, code);
        const QString value = appendUnit(valuesByCode.value(code), unitsByCode.value(code), false);
        const QString detail = QString::fromUtf8("DI: %1\n原始HEX: %2").arg(code, payloadHex);
        cards.append(makeCard(description, value, detail));
    }

    return cards;
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
    const int expectedSize = hasLength ? (12 + dataLength) : 12;
    const bool hasCompleteFrame = hasLength && protocolFrame.size() >= expectedSize;

    QByteArray displayedDataField;
    if (protocolFrame.size() > 10)
    {
        displayedDataField = hasCompleteFrame ? protocolFrame.mid(10, dataLength) : protocolFrame.mid(10);
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
    const int expectedSize = hasLength ? (12 + dataLength) : 12;
    const bool hasCompleteFrame = hasLength && protocolFrame.size() >= expectedSize;
    const int actualDataLength = hasLength ? qMin(static_cast<int>(dataLength), qMax(0, protocolFrame.size() - 12)) : 0;
    const QByteArray dataField = protocolFrame.mid(10, actualDataLength);

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

        if (dataField.size() >= 4)
        {
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

            const QByteArray payload = decodedDataField.mid(4);
            if (!payload.isEmpty())
            {
                if (!format.isEmpty())
                {
                    const QString decodedValue = decodedValueForItem(meterModel, diCode, payload, &description, &unit);
                    const QString breakdown = byteBreakdownLines(diCode, payload, format, decodedValue, unit);
                    if (!breakdown.isEmpty())
                    {
                        detailText += breakdown + QStringLiteral("\n");
                    }
                }
                else
                {
                    const QList<FrameParseCard> blockCards = buildBlockDecodeCards(meterModel, diCode, payload);
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
                        const QString decodedValue = decodedValueForItem(meterModel, diCode, payload, &decodeDescription, &decodeUnit);
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
            decodedPayload = DLT645DataHelper::decodeReversedPayload(dataField.mid(4));
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
            decodedPayload = DLT645DataHelper::decodeReversedPayload(dataField.mid(5, payloadLength > 0 ? payloadLength : -1));
            payloadLabel = QString::fromUtf8("写入数据");
        }
        else if (dataField.size() >= 12)
        {
            const QByteArray authorityBytes = DLT645DataHelper::subtract33(dataField.mid(4, 8));
            dataText += lineWithValue(QString::fromUtf8("权限："), DLT645FrameFormatter::formatHex(authorityBytes.left(1), false));
            dataText += lineWithValue(QString::fromUtf8("密码："), DLT645FrameFormatter::formatHex(authorityBytes.mid(1, 3), false));
            dataText += lineWithValue(QString::fromUtf8("操作代码："), DLT645FrameFormatter::formatHex(authorityBytes.mid(4, 4), false));

            decodedPayload = DLT645DataHelper::decodeReversedPayload(dataField.mid(12));
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

        const QList<FrameParseCard> blockCards = buildBlockDecodeCards(meterModel, diCode, decodedPayload);
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
                                             : decodedValueForItem(meterModel, diCode, decodedPayload, &decodeDescription, &decodeUnit);

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
    case 0x15:
        return QString::fromUtf8("广播校时");
    case 0x16:
        return QString::fromUtf8("写设备地址");
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
    case 0x91:
        return QString::fromUtf8("正常应答");
    case 0x92:
        return QString::fromUtf8("后续应答");
    case 0x94:
        return QString::fromUtf8("写数据应答");
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
            const quint8 dataLength = static_cast<quint8>(protocolFrame[9]);
            const int expectedSize = 12 + dataLength;
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
