#include "MeterModelPrivate.h"

using namespace MeterModelPrivate;

void MeterModel::initializeParameterData()
{
    // 参变量 (0x04xxxxxx)
    const QDateTime now = QDateTime::currentDateTime();
    const int configuredYearZoneCount = 2;
    const int configuredDayScheduleTableCount = 2;
    const int configuredDayScheduleCount = 8;
    const int configuredRateCount = 8;  // 修改为8，支持费率1-8
    const int configuredHolidayCount = 12;
    const int configuredStepCount = 3;
    const int configuredAutoScreenCount = 6;
    const int configuredKeyScreenCount = 4;
    constexpr int defaultStatusWord3 = 0x0050; // 主电源，继电器及继电器命令状态为断

    m_dataItems["0x04000101"] = DataItem("0x04000101", "当前日期(含星期代码)", compactDateWithWeek(now.date()), "-", "AA,AA,AA,AA", true, 0);
    m_dataItems["0x04000102"] = DataItem("0x04000102", "当前时间", now.time().toString(QStringLiteral("HHmmss")), "-", "AA,AA,AA", true, 0);
    m_dataItems["0x04000103"] = DataItem("0x04000103", "最大需量周期", 15, "分钟", "AA", true, 0);
    m_dataItems["0x04000104"] = DataItem("0x04000104", "滑差时间", 1, "分钟", "AA", true, 0);
    m_dataItems["0x04000105"] = DataItem("0x04000105", "校表脉冲宽度", 100, "毫秒", "AA,AA", true, 0);
    m_dataItems["0x04000106"] = DataItem("0x04000106", "两套时区表切换时间", compactDateTimeToMinutes(now.addDays(30)), "-", "AA,AA,AA,AA,AA", true, 0);
    m_dataItems["0x04000107"] = DataItem("0x04000107", "两套日时段表切换时间", compactDateTimeToMinutes(now.addDays(60)), "-", "AA,AA,AA,AA,AA", true, 0);
    m_dataItems["0x04000108"] = DataItem("0x04000108", "两套费率电价切换时间", compactDateTimeToMinutes(now.addDays(90)), "-", "AA,AA,AA,AA,AA", true, 0);
    m_dataItems["0x04000109"] = DataItem("0x04000109", "两套梯度切换时间", compactDateTimeToMinutes(now.addDays(120)), "-", "AA,AA,AA,AA,AA", true, 0);
    m_dataItems["0x04000201"] = DataItem("0x04000201", "年时区数p", configuredYearZoneCount, "个", "AA", true, 0);
    m_dataItems["0x04000202"] = DataItem("0x04000202", "日时段表数q", configuredDayScheduleTableCount, "个", "AA", true, 0);
    m_dataItems["0x04000203"] = DataItem("0x04000203", "日时段数m", configuredDayScheduleCount, "个", "AA", true, 0);
    m_dataItems["0x04000204"] = DataItem("0x04000204", "费率数k", configuredRateCount, "个", "AA", true, 0);
    m_dataItems["0x04000205"] = DataItem("0x04000205", "公共假日数n", configuredHolidayCount, "个", "AA,AA", true, 0);
    m_dataItems["0x04000206"] = DataItem("0x04000206", "谐波分析次数", 1, "次", "AA", true, 0);
    m_dataItems["0x04000207"] = DataItem("0x04000207", "梯度数", configuredStepCount, "个", "AA", true, 0);

    m_dataItems["0x04000301"] = DataItem("0x04000301", "自动循环显示屏数", configuredAutoScreenCount, "个", "AA", true, 0);
    m_dataItems["0x04000302"] = DataItem("0x04000302", "每屏显示时间", 5, "秒", "AA", true, 0);
    m_dataItems["0x04000303"] = DataItem("0x04000303", "显示电能小数位数", 2, "位", "AA", true, 0);
    m_dataItems["0x04000304"] = DataItem("0x04000304", "显示功率(最大需量)小数位数", 4, "位", "AA", true, 0);
    m_dataItems["0x04000305"] = DataItem("0x04000305", "按键循环显示屏数", configuredKeyScreenCount, "个", "AA", true, 0);
    m_dataItems["0x04000306"] = DataItem("0x04000306", "电流互感器变比", QStringLiteral("000200"), "-", "AA,AA,AA", true, 0);
    m_dataItems["0x04000307"] = DataItem("0x04000307", "电压互感器变比", QStringLiteral("000001"), "-", "AA,AA,AA", true, 0);

    m_dataItems["0x04000401"] = DataItem("0x04000401", "通信地址", m_meterAddress, "-", "AA,AA,AA,AA,AA,AA", true, 0);
    m_dataItems["0x04000402"] = DataItem("0x04000402", "表号", QStringLiteral("000000000001"), "-", "AA,AA,AA,AA,AA,AA", true, 0);
    m_dataItems["0x04000403"] = DataItem("0x04000403", "资产管理编码(ASCII码)", QStringLiteral("20260429000000000000000000000001"), "-", repeatedByteFormat(32), true, 0);
    m_dataItems["0x04000404"] = DataItem("0x04000404", "额定电压(ASCII码)", QStringLiteral("000000220000"), "-", repeatedByteFormat(6), true, 0);
    m_dataItems["0x04000405"] = DataItem("0x04000405", "额定电流/基本电流(ASCII码)", QStringLiteral("000000050000"), "-", repeatedByteFormat(6), true, 0);
    m_dataItems["0x04000406"] = DataItem("0x04000406", "最大电流(ASCII码)", QStringLiteral("000000600000"), "-", repeatedByteFormat(6), true, 0);
    m_dataItems["0x04000407"] = DataItem("0x04000407", "有功准确度等级(ASCII码)", QStringLiteral("0001"), "-", repeatedByteFormat(4), true, 0);
    m_dataItems["0x04000408"] = DataItem("0x04000408", "无功准确度等级(ASCII码)", QStringLiteral("0002"), "-", repeatedByteFormat(4), true, 0);
    m_dataItems["0x04000409"] = DataItem("0x04000409", "电表有功常数", QStringLiteral("1600"), "imp/kWh", "AA,AA", true, 0);
    m_dataItems["0x0400040A"] = DataItem("0x0400040A", "电表无功常数", QStringLiteral("1600"), "imp/kvarh", "AA,AA", true, 0);
    m_dataItems["0x0400040B"] = DataItem("0x0400040B", "电表型号(ASCII码)", QStringLiteral("DTZ0000001"), "-", repeatedByteFormat(10), true, 0);
    m_dataItems["0x0400040C"] = DataItem("0x0400040C", "生产日期(ASCII码)", QStringLiteral("2026042900"), "-", repeatedByteFormat(10), true, 0);
    m_dataItems["0x0400040D"] = DataItem("0x0400040D", "协议版本号(ASCII码)", QStringLiteral("6452007000000000"), "-", repeatedByteFormat(16), true, 0);
    m_dataItems["0x0400040E"] = DataItem("0x0400040E", "客户编号", QStringLiteral("000001234567"), "-", "AA,AA,AA,AA,AA,AA", true, 0);

    QVariantList statusWordBlock;
    for (int i = 1; i <= 7; ++i)
    {
        const QString diCode = QStringLiteral("0x040005%1").arg(i, 2, 16, QChar('0')).toUpper().replace(QStringLiteral("0X"), QStringLiteral("0x"));
        const int value = (i == 1) ? 1 : (i == 3 ? defaultStatusWord3 : 0);
        m_dataItems[diCode] = DataItem(diCode, QStringLiteral("电表运行状态字%1").arg(i), value, "-", "AA,AA", true, 0);
        statusWordBlock.append(value);
    }
    m_dataItems["0x040005FF"] = DataItem("0x040005FF", "电表运行状态字数据块", statusWordBlock, "-", repeatedFormatParts("AA,AA", 7), true, 0);

    m_dataItems["0x04000601"] = DataItem("0x04000601", "有功组合方式特征字", 1, "-", "AA", true, 0);
    m_dataItems["0x04000602"] = DataItem("0x04000602", "无功组合方式1特征字", 1, "-", "AA", true, 0);
    m_dataItems["0x04000603"] = DataItem("0x04000603", "无功组合方式2特征字", 1, "-", "AA", true, 0);

    m_dataItems["0x04000701"] = DataItem("0x04000701", "调制型红外光口通信速率特征字", 8, "-", "AA", true, 0);
    m_dataItems["0x04000702"] = DataItem("0x04000702", "接触式红外光口通信速率特征字", 8, "-", "AA", true, 0);
    m_dataItems["0x04000703"] = DataItem("0x04000703", "通信口1通信速率特征字", 8, "-", "AA", true, 0);
    m_dataItems["0x04000704"] = DataItem("0x04000704", "通信口2通信速率特征字", 8, "-", "AA", true, 0);
    m_dataItems["0x04000705"] = DataItem("0x04000705", "通信口3通信速率特征字", 8, "-", "AA", true, 0);

    m_dataItems["0x04000801"] = DataItem("0x04000801", "周休日特征字", 96, "-", "AA", true, 0);
    m_dataItems["0x04000802"] = DataItem("0x04000802", "周休日采用的日时段表号", 1, "-", "AA", true, 0);

    m_dataItems["0x04000901"] = DataItem("0x04000901", "负荷记录模式字", 1, "-", "AA", true, 0);
    m_dataItems["0x04000902"] = DataItem("0x04000902", "定时冻结数据模式字", 1, "-", "AA", true, 0);
    m_dataItems["0x04000903"] = DataItem("0x04000903", "瞬时冻结数据模式字", 1, "-", "AA", true, 0);
    m_dataItems["0x04000904"] = DataItem("0x04000904", "约定冻结数据模式字", 1, "-", "AA", true, 0);
    m_dataItems["0x04000905"] = DataItem("0x04000905", "整点冻结数据模式字", 1, "-", "AA", true, 0);
    m_dataItems["0x04000906"] = DataItem("0x04000906", "日冻结数据模式字", 1, "-", "AA", true, 0);

    m_dataItems["0x04000A01"] = DataItem("0x04000A01", "负荷记录起始时间", QStringLiteral("01010000"), "-", "AA,AA,AA,AA", true, 0);
    m_dataItems["0x04000A02"] = DataItem("0x04000A02", "第1类负荷记录间隔时间", 15, "分钟", "AA,AA", true, 0);
    m_dataItems["0x04000A03"] = DataItem("0x04000A03", "第2类负荷记录间隔时间", 30, "分钟", "AA,AA", true, 0);
    m_dataItems["0x04000A04"] = DataItem("0x04000A04", "第3类负荷记录间隔时间", 60, "分钟", "AA,AA", true, 0);
    m_dataItems["0x04000A05"] = DataItem("0x04000A05", "第4类负荷记录间隔时间", 60, "分钟", "AA,AA", true, 0);
    m_dataItems["0x04000A06"] = DataItem("0x04000A06", "第5类负荷记录间隔时间", 60, "分钟", "AA,AA", true, 0);
    m_dataItems["0x04000A07"] = DataItem("0x04000A07", "第6类负荷记录间隔时间", 60, "分钟", "AA,AA", true, 0);

    m_dataItems["0x04000B01"] = DataItem("0x04000B01", "每月第1结算日", QStringLiteral("0100"), "-", "AA,AA", true, 0);
    m_dataItems["0x04000B02"] = DataItem("0x04000B02", "每月第2结算日", QStringLiteral("1500"), "-", "AA,AA", true, 0);
    m_dataItems["0x04000B03"] = DataItem("0x04000B03", "每月第3结算日", QStringLiteral("9999"), "-", "AA,AA", true, 0);

    m_dataItems["0x04000C01"] = DataItem("0x04000C01", "安全认证错误信息字", 0, "-", "AA,AA", true, 0);

    m_dataItems["0x04001201"] = DataItem("0x04001201", "整点冻结起始时间", compactDateTimeToMinutes(QDateTime(QDate(now.date().year(), 1, 1), QTime(0, 0))), "-", "AA,AA,AA,AA,AA", true, 0);
    m_dataItems["0x04001202"] = DataItem("0x04001202", "整点冻结时间间隔", 60, "分钟", "AA", true, 0);
    m_dataItems["0x04001203"] = DataItem("0x04001203", "日冻结时间", QStringLiteral("0000"), "-", "AA,AA", true, 0);
    m_dataItems["0x04001301"] = DataItem("0x04001301", "无线通信在线及信号强弱指示", 132, "-", "AA", true, 0);
    m_dataItems["0x04001401"] = DataItem("0x04001401", "跳闸延时时间", 15, "分钟", "AA,AA", true, 0);

    // 厂商扩展：认证状态（控制码 0x2A 读 / 0x2F 写，数据 1 字节：0x00 关闭 / 0xAA 开启）
    m_dataItems["0x1C0104CD"] = DataItem("0x1C0104CD", "认证状态", 0xAA, "-", "AA", true, 0);

    QVariantList firstSetYearZones;
    QVariantList secondSetYearZones;
    for (int zoneIndex = 0; zoneIndex < kMaxYearZoneCount; ++zoneIndex)
    {
        firstSetYearZones.append(buildYearZoneEntryValue(11, 0, zoneIndex));
        secondSetYearZones.append(buildYearZoneEntryValue(37, 1, zoneIndex));
    }
    m_dataItems["0x04010000"] = DataItem("0x04010000", "第一套时区表数据", firstSetYearZones, "-", repeatedFormatParts("AA,AA,AA", kMaxYearZoneCount), true, 0);
    m_dataItems["0x04020000"] = DataItem("0x04020000", "第二套时区表数据", secondSetYearZones, "-", repeatedFormatParts("AA,AA,AA", kMaxYearZoneCount), true, 0);

    for (int tableIndex = 0; tableIndex < kMaxDayScheduleTableCount; ++tableIndex)
    {
        const QString firstDiCode = QStringLiteral("0x040100%1").arg(tableIndex + 1, 2, 16, QChar('0')).toUpper().replace(QStringLiteral("0X"), QStringLiteral("0x"));
        const QString secondDiCode = QStringLiteral("0x040200%1").arg(tableIndex + 1, 2, 16, QChar('0')).toUpper().replace(QStringLiteral("0X"), QStringLiteral("0x"));
        m_dataItems[firstDiCode] = DataItem(firstDiCode,
                                            QStringLiteral("第一套第%1日时段表数据").arg(tableIndex + 1),
                                            QVariantList(),
                                            "-",
                                            repeatedFormatParts("AA,AA,AA", kMaxDayScheduleCount),
                                            true,
                                            0);
        m_dataItems[secondDiCode] = DataItem(secondDiCode,
                                             QStringLiteral("第二套第%1日时段表数据").arg(tableIndex + 1),
                                             QVariantList(),
                                             "-",
                                             repeatedFormatParts("AA,AA,AA", kMaxDayScheduleCount),
                                             true,
                                             0);
        const QString firstValue = buildDayScheduleTableValue(19, 0, tableIndex);
        const QString secondValue = buildDayScheduleTableValue(43, 1, tableIndex);
        QVariantList firstTableEntries;
        QVariantList secondTableEntries;
        for (int slotIndex = 0; slotIndex < kMaxDayScheduleCount; ++slotIndex)
        {
            firstTableEntries.append(firstValue.mid(slotIndex * 6, 6));
            secondTableEntries.append(secondValue.mid(slotIndex * 6, 6));
        }
        m_dataItems[firstDiCode].value = firstTableEntries;
        m_dataItems[secondDiCode].value = secondTableEntries;
    }

    for (int holidayIndex = 0; holidayIndex < configuredHolidayCount; ++holidayIndex)
    {
        const QString diCode = QStringLiteral("0x0403%1").arg(holidayIndex + 1, 4, 16, QChar('0')).toUpper().replace(QStringLiteral("0X"), QStringLiteral("0x"));
        const QDate holidayDate(now.date().year(), (holidayIndex % 12) + 1, qMin(holidayIndex + 1, 28));
        const int scheduleTableIndex = (holidayIndex % configuredDayScheduleTableCount) + 1;
        m_dataItems[diCode] = DataItem(diCode,
                                       QStringLiteral("第%1公共假日日期及日时段表号").arg(holidayIndex + 1),
                                       buildHolidayEntryValue(holidayDate, scheduleTableIndex),
                                       "-",
                                       "AA,AA,AA,AA",
                                       true,
                                       0);
    }

    const QStringList autoScreenTargets = {
        QStringLiteral("0x00000000"),
        QStringLiteral("0x00010000"),
        QStringLiteral("0x02010100"),
        QStringLiteral("0x02020100"),
        QStringLiteral("0x02030000"),
        QStringLiteral("0x02060000")};
    for (int screenIndex = 0; screenIndex < autoScreenTargets.size(); ++screenIndex)
    {
        const QString diCode = QStringLiteral("0x040401%1").arg(screenIndex + 1, 2, 16, QChar('0')).toUpper().replace(QStringLiteral("0X"), QStringLiteral("0x"));
        m_dataItems[diCode] = DataItem(diCode,
                                       QStringLiteral("自动循环显示第%1屏显示数据项(是否分屏)").arg(screenIndex + 1),
                                       buildDisplayScreenEntryValue(autoScreenTargets.at(screenIndex), screenIndex % 2),
                                       "-",
                                       "AA,AA,AA,AA,AA",
                                       true,
                                       0);
    }

    const QStringList keyScreenTargets = {
        QStringLiteral("0x00020000"),
        QStringLiteral("0x02010200"),
        QStringLiteral("0x02020200"),
        QStringLiteral("0x02040000")};
    for (int screenIndex = 0; screenIndex < keyScreenTargets.size(); ++screenIndex)
    {
        const QString diCode = QStringLiteral("0x040402%1").arg(screenIndex + 1, 2, 16, QChar('0')).toUpper().replace(QStringLiteral("0X"), QStringLiteral("0x"));
        m_dataItems[diCode] = DataItem(diCode,
                                       QStringLiteral("按键循环显示第%1屏显示数据项(是否分屏)").arg(screenIndex + 1),
                                       buildDisplayScreenEntryValue(keyScreenTargets.at(screenIndex), screenIndex % 2),
                                       "-",
                                       "AA,AA,AA,AA,AA",
                                       true,
                                       0);
    }

    const QList<double> firstRatePrices = {0.5688, 0.6288, 0.6888, 0.7488, 0.8088, 0.8688, 0.9288, 0.9888};
    const QList<double> secondRatePrices = {0.5888, 0.6488, 0.7088, 0.7688, 0.8288, 0.8888, 0.9488, 1.0088};
    for (int rateIndex = 0; rateIndex < kMaxRateCount; ++rateIndex)
    {
        const QString firstDiCode = QStringLiteral("0x040501%1").arg(rateIndex + 1, 2, 16, QChar('0')).toUpper().replace(QStringLiteral("0X"), QStringLiteral("0x"));
        const QString secondDiCode = QStringLiteral("0x040502%1").arg(rateIndex + 1, 2, 16, QChar('0')).toUpper().replace(QStringLiteral("0X"), QStringLiteral("0x"));
        m_dataItems[firstDiCode] = DataItem(firstDiCode, QStringLiteral("第一套费率电价%1").arg(rateIndex + 1), firstRatePrices.at(rateIndex), "元", "AA,AA,.,AA,AA", true, 4);
        m_dataItems[secondDiCode] = DataItem(secondDiCode, QStringLiteral("第二套费率电价%1").arg(rateIndex + 1), secondRatePrices.at(rateIndex), "元", "AA,AA,.,AA,AA", true, 4);
    }

    const QList<double> firstStepValues = {1800.0, 3200.0, 5000.0};
    const QList<double> firstStepPrices = {0.5688, 0.6188, 0.6888};
    const QList<double> secondStepValues = {2000.0, 3500.0, 5200.0};
    const QList<double> secondStepPrices = {0.5888, 0.6388, 0.7088};
    for (int stepIndex = 0; stepIndex < configuredStepCount; ++stepIndex)
    {
        const QString firstValueDiCode = QStringLiteral("0x040600%1").arg(stepIndex + 1, 2, 16, QChar('0')).toUpper().replace(QStringLiteral("0X"), QStringLiteral("0x"));
        const QString firstPriceDiCode = QStringLiteral("0x040601%1").arg(stepIndex + 1, 2, 16, QChar('0')).toUpper().replace(QStringLiteral("0X"), QStringLiteral("0x"));
        const QString secondValueDiCode = QStringLiteral("0x040602%1").arg(stepIndex + 1, 2, 16, QChar('0')).toUpper().replace(QStringLiteral("0X"), QStringLiteral("0x"));
        const QString secondPriceDiCode = QStringLiteral("0x040603%1").arg(stepIndex + 1, 2, 16, QChar('0')).toUpper().replace(QStringLiteral("0X"), QStringLiteral("0x"));
        m_dataItems[firstValueDiCode] = DataItem(firstValueDiCode, QStringLiteral("第一套第%1阶梯值").arg(stepIndex + 1), firstStepValues.at(stepIndex), "kWh", "AA,AA,AA,.,AA", true, 2);
        m_dataItems[firstPriceDiCode] = DataItem(firstPriceDiCode, QStringLiteral("第一套阶梯电价%1").arg(stepIndex + 1), firstStepPrices.at(stepIndex), "元", "AA,AA,.,AA,AA", true, 4);
        m_dataItems[secondValueDiCode] = DataItem(secondValueDiCode, QStringLiteral("第二套第%1阶梯值").arg(stepIndex + 1), secondStepValues.at(stepIndex), "kWh", "AA,AA,AA,.,AA", true, 2);
        m_dataItems[secondPriceDiCode] = DataItem(secondPriceDiCode, QStringLiteral("第二套阶梯电价%1").arg(stepIndex + 1), secondStepPrices.at(stepIndex), "元", "AA,AA,.,AA,AA", true, 4);
    }

    m_dataItems["0x04090101"] = DataItem("0x04090101", "失压事件电压触发上限", 60.0, "V", "AA,AA,.,A", true, 1);
    m_dataItems["0x04090102"] = DataItem("0x04090102", "失压事件电压恢复下限", 180.0, "V", "AA,AA,.,A", true, 1);
    m_dataItems["0x04090103"] = DataItem("0x04090103", "失压事件电流触发下限", 1.5, "A", "AA,.,AA,AA", true, 4);
    m_dataItems["0x04090104"] = DataItem("0x04090104", "失压事件判定延时时间", 10, "秒", "AA", true, 0);
    m_dataItems["0x04090201"] = DataItem("0x04090201", "欠压事件电压触发上限", 180.0, "V", "AA,AA,.,A", true, 1);
    m_dataItems["0x04090202"] = DataItem("0x04090202", "欠压事件判定延时时间", 10, "秒", "AA", true, 0);
    m_dataItems["0x04090301"] = DataItem("0x04090301", "过压事件电压触发下限", 260.0, "V", "AA,AA,.,A", true, 1);
    m_dataItems["0x04090302"] = DataItem("0x04090302", "过压事件判定延时时间", 10, "秒", "AA", true, 0);
    m_dataItems["0x04090401"] = DataItem("0x04090401", "断相事件电压触发上限", 60.0, "V", "AA,AA,.,A", true, 1);
    m_dataItems["0x04090402"] = DataItem("0x04090402", "断相事件电流触发上限", 1.0, "A", "AA,.,AA,AA", true, 4);
    m_dataItems["0x04090403"] = DataItem("0x04090403", "断相事件判定延时时间", 10, "秒", "AA", true, 0);
    m_dataItems["0x04090501"] = DataItem("0x04090501", "电压不平衡率限值", 20.0, "%", "AA,.,AA", true, 2);
    m_dataItems["0x04090502"] = DataItem("0x04090502", "电压不平衡率判定延时时间", 30, "秒", "AA", true, 0);
    m_dataItems["0x04090601"] = DataItem("0x04090601", "电流不平衡率限值", 30.0, "%", "AA,.,AA", true, 2);
    m_dataItems["0x04090602"] = DataItem("0x04090602", "电流不平衡率判定延时时间", 30, "秒", "AA", true, 0);
    m_dataItems["0x04090701"] = DataItem("0x04090701", "失流事件电压触发下限", 180.0, "V", "AA,AA,.,A", true, 1);
    m_dataItems["0x04090702"] = DataItem("0x04090702", "失流事件电流触发上限", 0.8, "A", "AA,.,AA,AA", true, 4);
    m_dataItems["0x04090703"] = DataItem("0x04090703", "失流事件电流触发下限", 0.1, "A", "AA,.,AA,AA", true, 4);
    m_dataItems["0x04090704"] = DataItem("0x04090704", "失流事件判定延时时间", 10, "秒", "AA", true, 0);
    m_dataItems["0x04090801"] = DataItem("0x04090801", "过流事件电流触发下限", 80.0, "A", "AA,AA,.,A", true, 1);
    m_dataItems["0x04090802"] = DataItem("0x04090802", "过流事件判定延时时间", 10, "秒", "AA", true, 0);
    m_dataItems["0x04090901"] = DataItem("0x04090901", "断流事件电压触发下限", 180.0, "V", "AA,AA,.,A", true, 1);
    m_dataItems["0x04090902"] = DataItem("0x04090902", "断流事件电流触发上限", 0.5, "A", "AA,.,AA,AA", true, 4);
    m_dataItems["0x04090903"] = DataItem("0x04090903", "断流事件判定延时时间", 10, "秒", "AA", true, 0);
    m_dataItems["0x04090A01"] = DataItem("0x04090A01", "潮流反向事件有功功率触发下限", 1.5, "kW", "AA,.,AA,AA", true, 4);
    m_dataItems["0x04090A02"] = DataItem("0x04090A02", "潮流反向事件判定延时时间", 10, "秒", "AA", true, 0);
    m_dataItems["0x04090B01"] = DataItem("0x04090B01", "过载事件有功功率触发下限", 15.0, "kW", "AA,.,AA,AA", true, 4);
    m_dataItems["0x04090B02"] = DataItem("0x04090B02", "过载事件判定延时时间", 10, "秒", "AA", true, 0);
    m_dataItems["0x04090C01"] = DataItem("0x04090C01", "电压考核上限", 242.0, "V", "AA,AA,.,A", true, 1);
    m_dataItems["0x04090C02"] = DataItem("0x04090C02", "电压考核下限", 198.0, "V", "AA,AA,.,A", true, 1);
    m_dataItems["0x04090D01"] = DataItem("0x04090D01", "有功需量超限事件需量触发下限", 10.0, "kW", "AA,.,AA,AA", true, 4);
    m_dataItems["0x04090D02"] = DataItem("0x04090D02", "无功需量超限事件需量触发下限", 6.0, "kVar", "AA,.,AA,AA", true, 4);
    m_dataItems["0x04090D03"] = DataItem("0x04090D03", "需量超限事件判定延时时间", 60, "秒", "AA", true, 0);
    m_dataItems["0x04090E01"] = DataItem("0x04090E01", "总功率因数超下限阀值", 0.85, "-", "xx,AA", true, 3);
    m_dataItems["0x04090E02"] = DataItem("0x04090E02", "总功率因数超下限判定延时时间", 60, "秒", "AA", true, 0);
    m_dataItems["0x04090F01"] = DataItem("0x04090F01", "电流严重不平衡限值", 40.0, "%", "AA,.,AA", true, 2);
    m_dataItems["0x04090F02"] = DataItem("0x04090F02", "电流严重不平衡触发延时时间", 60, "秒", "AA", true, 0);
}

