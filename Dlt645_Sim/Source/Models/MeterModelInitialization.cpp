#include "MeterModelPrivate.h"

using namespace MeterModelPrivate;

void MeterModel::initializeDefaultDataItems()
{
    initializeEnergyData();
    initializeDemandData();
    initializeVariableData();
    initializeEventData();
    initializeParameterData();
    refreshParameterScheduleData(m_dataItems);
    initializeFreezeData();
    initializeLoadRecordData();
    ensureCurrentEnergyRateItems(m_dataItems);
    ensureDemandRateItems(m_dataItems);
    ensureFreezeRateItems(m_dataItems);
    refreshParameterScheduleData(m_dataItems);
    refreshRateDependentBlockFormats(m_dataItems);
    synchronizeDemandTotals(m_dataItems);
    synchronizeInstantaneousPowerTotals(m_dataItems);
    synchronizeCurrentEnergyTotals(m_dataItems);
    synchronizeFreezeTotals(m_dataItems);
    refreshLatestFreezeCompositeBlocks(m_dataItems);
}

