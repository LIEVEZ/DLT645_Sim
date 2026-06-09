#include "MeterModelPrivate.h"

using namespace MeterModelPrivate;

void MeterModel::initializeVariableData()
{
    // 变量数据 (0x02xxxxxx)
    // 电压
    m_dataItems["0x02010100"] = DataItem("0x02010100", "A相电压", 20.1, "V", "AA,xx", false, 1);
    m_dataItems["0x02010200"] = DataItem("0x02010200", "B相电压", 220.2, "V", "AA,xx", false, 1);
    m_dataItems["0x02010300"] = DataItem("0x02010300", "C相电压", 220.3, "V", "AA,xx", false, 1);
    m_dataItems["0x0201FF00"] = DataItem("0x0201FF00", "电压数据块", QVariantList(), "-", "AA,xx:AA,xx:AA,xx", false, 1);

    // 电流
    m_dataItems["0x02020100"] = DataItem("0x02020100", "A相电流", 1.01, "A", "F9,xx,AA", false, 3);
    m_dataItems["0x02020200"] = DataItem("0x02020200", "B相电流", 1.023, "A", "F9,xx,AA", false, 3);
    m_dataItems["0x02020300"] = DataItem("0x02020300", "C相电流", 1.1, "A", "F9,xx,AA", false, 3);
    m_dataItems["0x0202FF00"] = DataItem("0x0202FF00", "电流数据块", QVariantList(), "-", "F9,xx,AA:F9,xx,AA:F9,xx,AA", false, 3);

    // 功率
    m_dataItems["0x02030000"] = DataItem("0x02030000", "瞬时总有功功率", 0.0, "kW", "F9,.,AA,AA", false, 4);
    m_dataItems["0x02030100"] = DataItem("0x02030100", "瞬时A相有功功率", 0.0, "kW", "F9,.,AA,AA", false, 4);
    m_dataItems["0x02030200"] = DataItem("0x02030200", "瞬时B相有功功率", 0.0, "kW", "F9,.,AA,AA", false, 4);
    m_dataItems["0x02030300"] = DataItem("0x02030300", "瞬时C相有功功率", 0.0, "kW", "F9,.,AA,AA", false, 4);
    m_dataItems["0x0203FF00"] = DataItem("0x0203FF00", "瞬时有功功率数据块", QVariantList(), "-", "F9,.,AA,AA:F9,.,AA,AA:F9,.,AA,AA:F9,.,AA,AA", false, 4);

    m_dataItems["0x02040000"] = DataItem("0x02040000", "瞬时总无功功率", 0.0, "kvar", "F9,.,AA,AA", false, 4);
    m_dataItems["0x02040100"] = DataItem("0x02040100", "瞬时A相无功功率", 0.0, "kvar", "F9,.,AA,AA", false, 4);
    m_dataItems["0x02040200"] = DataItem("0x02040200", "瞬时B相无功功率", 0.0, "kvar", "F9,.,AA,AA", false, 4);
    m_dataItems["0x02040300"] = DataItem("0x02040300", "瞬时C相无功功率", 0.0, "kvar", "F9,.,AA,AA", false, 4);
    m_dataItems["0x0204FF00"] = DataItem("0x0204FF00", "瞬时无功功率数据块", QVariantList(), "-", "F9,.,AA,AA:F9,.,AA,AA:F9,.,AA,AA:F9,.,AA,AA", false, 4);

    m_dataItems["0x02050000"] = DataItem("0x02050000", "瞬时总视在功率", 0.0, "kVA", "AA,.,AA,AA", false, 4);
    m_dataItems["0x02050100"] = DataItem("0x02050100", "瞬时A相视在功率", 0.0, "kVA", "AA,.,AA,AA", false, 4);
    m_dataItems["0x02050200"] = DataItem("0x02050200", "瞬时B相视在功率", 0.0, "kVA", "AA,.,AA,AA", false, 4);
    m_dataItems["0x02050300"] = DataItem("0x02050300", "瞬时C相视在功率", 0.0, "kVA", "AA,.,AA,AA", false, 4);
    m_dataItems["0x0205FF00"] = DataItem("0x0205FF00", "瞬时视在功率数据块", QVariantList(), "-", "AA,.,AA,AA:AA,.,AA,AA:AA,.,AA,AA:AA,.,AA,AA", false, 4);

    // 功率因数
    m_dataItems["0x02060000"] = DataItem("0x02060000", "总功率因数", 1.0, "none", "xx,AA", false, 3);
    m_dataItems["0x02060100"] = DataItem("0x02060100", "A相功率因数", 1.0, "none", "xx,AA", false, 3);
    m_dataItems["0x02060200"] = DataItem("0x02060200", "B相功率因数", 1.0, "none", "xx,AA", false, 3);
    m_dataItems["0x02060300"] = DataItem("0x02060300", "C相功率因数", 1.0, "none", "xx,AA", false, 3);
    m_dataItems["0x0206FF00"] = DataItem("0x0206FF00", "功率因数数据块", QVariantList(), "-", "xx,AA:xx,AA:xx,AA:xx,AA", false, 3);

    // 相角
    m_dataItems["0x02070100"] = DataItem("0x02070100", "A相相角", 0.0, "°", "AA,xx", false, 1);
    m_dataItems["0x02070200"] = DataItem("0x02070200", "B相相角", 0.0, "°", "AA,xx", false, 1);
    m_dataItems["0x02070300"] = DataItem("0x02070300", "C相相角", 0.0, "°", "AA,xx", false, 1);
    m_dataItems["0x0207FF00"] = DataItem("0x0207FF00", "相角数据块", QVariantList(), "-", "AA,xx:AA,xx:AA,xx", false, 1);

    // 波形失真度
    m_dataItems["0x02080100"] = DataItem("0x02080100", "A相电压波形失真度", 1.25, "%", "AA,.,AA", false, 2);
    m_dataItems["0x02080200"] = DataItem("0x02080200", "B相电压波形失真度", 1.18, "%", "AA,.,AA", false, 2);
    m_dataItems["0x02080300"] = DataItem("0x02080300", "C相电压波形失真度", 1.31, "%", "AA,.,AA", false, 2);
    m_dataItems["0x0208FF00"] = DataItem("0x0208FF00", "电压波形失真度数据块", QVariantList(), "-", "AA,.,AA:AA,.,AA:AA,.,AA", false, 2);
    m_dataItems["0x02090100"] = DataItem("0x02090100", "A相电流波形失真度", 2.46, "%", "AA,.,AA", false, 2);
    m_dataItems["0x02090200"] = DataItem("0x02090200", "B相电流波形失真度", 2.38, "%", "AA,.,AA", false, 2);
    m_dataItems["0x02090300"] = DataItem("0x02090300", "C相电流波形失真度", 2.52, "%", "AA,.,AA", false, 2);
    m_dataItems["0x0209FF00"] = DataItem("0x0209FF00", "电流波形失真度数据块", QVariantList(), "-", "AA,.,AA:AA,.,AA:AA,.,AA", false, 2);

    // 谐波含量
    for (int phaseIndex = 1; phaseIndex <= 3; ++phaseIndex)
    {
        initializePhaseHarmonicFamily(m_dataItems, QStringLiteral("0x020A"), QStringLiteral("电压"), phaseIndex, 1.05, 0.18);
        initializePhaseHarmonicFamily(m_dataItems, QStringLiteral("0x020B"), QStringLiteral("电流"), phaseIndex, 1.28, 0.22);
    }

    // 零相电流
    m_dataItems["0x02800001"] = DataItem("0x02800001", "零相电流", 0.0, "A", "F9,xx,AA", false, 3);
    // 频率
    m_dataItems["0x02800002"] = DataItem("0x02800002", "电网频率", 50.0, "Hz", "AA,.,AA", false, 2);
    m_dataItems["0x02800003"] = DataItem("0x02800003", "一分钟有功总平均功率", 12.3456, "kW", "AA,.,AA,AA", false, 4);
    m_dataItems["0x02800004"] = DataItem("0x02800004", "当前有功需量", 10.8765, "kW", "AA,.,AA,AA", false, 4);
    m_dataItems["0x02800005"] = DataItem("0x02800005", "当前无功需量", 8.4321, "kvar", "AA,.,AA,AA", false, 4);
    m_dataItems["0x02800006"] = DataItem("0x02800006", "当前视在需量", 13.2109, "kVA", "AA,.,AA,AA", false, 4);
    m_dataItems["0x02800007"] = DataItem("0x02800007", "表内温度", 26.5, "℃", "AA,xx", false, 1);
    m_dataItems["0x02800008"] = DataItem("0x02800008", "时钟电池电压(内部)", 3.27, "V", "AA,.,AA", false, 2);
    m_dataItems["0x02800009"] = DataItem("0x02800009", "停电抄表电池电压(外部)", 3.18, "V", "AA,.,AA", false, 2);
    m_dataItems["0x0280000A"] = DataItem("0x0280000A", "内部电池工作时间", 8640, "分钟", "AA,AA,AA,AA", false, 0);
    m_dataItems["0x0280000B"] = DataItem("0x0280000B", "当前阶梯电价", 0.5688, "元/kWh", "AA,AA,.,AA,AA", false, 4);
    m_dataItems["0x23010001"] = DataItem("0x23010001", "A相上端子温度", 32.50, "℃", "AA,AA,.,AA", false, 2);
    m_dataItems["0x23010002"] = DataItem("0x23010002", "B相上端子温度", 33.10, "℃", "AA,AA,.,AA", false, 2);
    m_dataItems["0x23010003"] = DataItem("0x23010003", "C相上端子温度", 31.80, "℃", "AA,AA,.,AA", false, 2);
    m_dataItems["0x23010004"] = DataItem("0x23010004", "N相上端子温度", 30.60, "℃", "AA,AA,.,AA", false, 2);
    m_dataItems["0x23010005"] = DataItem("0x23010005", "A相下端子温度", 34.20, "℃", "AA,AA,.,AA", false, 2);
    m_dataItems["0x23010006"] = DataItem("0x23010006", "B相下端子温度", 34.90, "℃", "AA,AA,.,AA", false, 2);
    m_dataItems["0x23010007"] = DataItem("0x23010007", "C相下端子温度", 33.70, "℃", "AA,AA,.,AA", false, 2);
    m_dataItems["0x23010008"] = DataItem("0x23010008", "N相下端子温度", 31.40, "℃", "AA,AA,.,AA", false, 2);
    m_dataItems["0x230100FF"] = DataItem("0x230100FF", "所有端子温度数据块", QVariantList(), "-", "AA,AA,.,AA:AA,AA,.,AA:AA,AA,.,AA:AA,AA,.,AA:AA,AA,.,AA:AA,AA,.,AA:AA,AA,.,AA:AA,AA,.,AA", false, 2);
    synchronizeInstantaneousPowerTotals(m_dataItems);
}

