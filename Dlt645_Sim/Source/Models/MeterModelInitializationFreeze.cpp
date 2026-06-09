#include "MeterModelPrivate.h"

using namespace MeterModelPrivate;

void MeterModel::initializeFreezeData()
{
    // 冻结数据 (0x05xxxxxx)
    const QDateTime now = QDateTime::currentDateTime();
    for (const FreezeCategoryDefinition &category : latestFreezeCategoryDefinitions())
    {
        const QDateTime freezeTime = defaultLatestFreezeSnapshotTime(category, now);
        initializeLatestFreezeEnergyCategory(m_dataItems, category, freezeTime);
    }

    initializeLatestHourlyFreezeCategory(m_dataItems, now.addSecs(-60 * 60));
    synchronizeFreezeTotals(m_dataItems);
    refreshLatestFreezeCompositeBlocks(m_dataItems);
}

void MeterModel::initializeLoadRecordData()
{
    // 负荷记录 (0x06xxxxxx)
    m_dataItems["0x06010101"] = DataItem("0x06010101", "预约远程跳闸控制(数值+单位代码)", QStringLiteral("3002"), "-", "AA,AA", true, 0);
    m_dataItems["0x06020101"] = DataItem("0x06020101", "预约远程合闸控制(数值+单位代码)", QStringLiteral("1502"), "-", "AA,AA", true, 0);
    m_dataItems["0x06030101"] = DataItem("0x06030101", "预约模拟试跳控制(数值+单位代码)", QStringLiteral("0502"), "-", "AA,AA", true, 0);
}

