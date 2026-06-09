#include "MeterModelPrivate.h"

using namespace MeterModelPrivate;

MeterModel::MeterModel(QObject *parent)
    : QObject(parent), m_meterAddress("202101190552"), m_operatorCode("00000000"), m_password("00000000"), m_includePrefixFE(true), m_meterPhaseType(MeterPhaseType::ThreePhase)
{
    initializeDefaultDataItems();
}

MeterModel::~MeterModel()
{
}

void MeterModel::setMeterAddress(const QString &address)
{
    bool changed = false;
    {
        QMutexLocker locker(&m_mutex);
        if (m_meterAddress != address)
        {
            m_meterAddress = address;
            if (m_dataItems.contains("0x04000401"))
            {
                m_dataItems["0x04000401"].value = address;
            }
            changed = true;
        }
    }

    if (changed)
    {
        emit addressChanged(address);
    }
}

void MeterModel::setOperatorCode(const QString &code)
{
    QMutexLocker locker(&m_mutex);
    m_operatorCode = code;
}

void MeterModel::setPassword(const QString &password)
{
    QMutexLocker locker(&m_mutex);
    m_password = password;
}

void MeterModel::setIncludePrefixFE(bool include)
{
    QMutexLocker locker(&m_mutex);
    m_includePrefixFE = include;
}

void MeterModel::setMeterPhaseType(MeterPhaseType meterPhaseType)
{
    bool changed = false;
    {
        QMutexLocker locker(&m_mutex);
        if (m_meterPhaseType != meterPhaseType)
        {
            m_meterPhaseType = meterPhaseType;
            changed = true;
        }
    }

    if (changed)
    {
        emit dataBatchChanged();
    }
}

bool MeterModel::setDataValue(const QString &diCode, const QVariant &value)
{
    bool updated = false;
    bool requiresBatchRefresh = false;
    QVariant emittedValue = value;
    {
        QMutexLocker locker(&m_mutex);
        if (isDataItemSupportedUnlocked(diCode))
        {
            emittedValue = normalizedParameterSettingValue(diCode, value);

            m_dataItems[diCode].value = emittedValue;
            if (applyEnergyBlockValues(m_dataItems, diCode, emittedValue.toList()))
            {
                requiresBatchRefresh = true;
            }
            if (isMultiRateEnergyCode(diCode))
            {
                synchronizeCurrentEnergyTotals(m_dataItems);
            }
            if (isDemandDataCode(diCode))
            {
                synchronizeDemandTotals(m_dataItems);
            }
            if (diCode.startsWith("0x0203") || diCode.startsWith("0x0204") || diCode.startsWith("0x0205"))
            {
                synchronizeInstantaneousPowerTotals(m_dataItems);
            }
            if (isFreezeEnergyFamilyCode(diCode))
            {
                synchronizeFreezeTotals(m_dataItems);
            }
            if (diCode.startsWith(QStringLiteral("0x05")))
            {
                refreshLatestFreezeCompositeBlocks(m_dataItems);
            }
            if (diCode.startsWith(QStringLiteral("0x040100")) ||
                diCode.startsWith(QStringLiteral("0x040200")) ||
                diCode.startsWith(QStringLiteral("0x0403")))
            {
                refreshParameterScheduleData(m_dataItems);
                requiresBatchRefresh = true;
            }
            if (diCode == "0x04000201" ||
                diCode == "0x04000202" ||
                diCode == "0x04000203" ||
                diCode == "0x04000204" ||
                diCode == "0x04000205")
            {
                refreshParameterScheduleData(m_dataItems);
                refreshRateDependentBlockFormats(m_dataItems);
                synchronizeDemandTotals(m_dataItems);
                synchronizeInstantaneousPowerTotals(m_dataItems);
                synchronizeCurrentEnergyTotals(m_dataItems);
                synchronizeFreezeTotals(m_dataItems);
                refreshLatestFreezeCompositeBlocks(m_dataItems);
                requiresBatchRefresh = true;
            }
            if (diCode == "0x04000401")
            {
                m_meterAddress = emittedValue.toString();
            }
            updated = true;
        }
    }

    if (updated)
    {
        emit dataChanged(diCode, emittedValue);
        if (diCode == "0x04000401")
        {
            emit addressChanged(emittedValue.toString());
        }
        if (requiresBatchRefresh)
        {
            emit dataBatchChanged();
        }
    }

    return updated;
}

void MeterModel::applyPersistedData(const QMap<QString, QVariant> &dataSnapshot)
{
    if (dataSnapshot.isEmpty())
    {
        return;
    }

    {
        QMutexLocker locker(&m_mutex);
        for (auto it = dataSnapshot.constBegin(); it != dataSnapshot.constEnd(); ++it)
        {
            if (m_dataItems.contains(it.key()))
            {
                m_dataItems[it.key()].value = normalizedParameterSettingValue(it.key(), it.value());
            }
        }

        if (m_dataItems.contains("0x04000401"))
        {
            m_dataItems["0x04000401"].value = m_meterAddress;
        }

        refreshParameterScheduleData(m_dataItems);
        refreshRateDependentBlockFormats(m_dataItems);
        synchronizeDemandTotals(m_dataItems);
        synchronizeInstantaneousPowerTotals(m_dataItems);
        synchronizeCurrentEnergyTotals(m_dataItems);
        synchronizeFreezeTotals(m_dataItems);
        refreshLatestFreezeCompositeBlocks(m_dataItems);
    }

    emit dataBatchChanged();
}

void MeterModel::resetAllData()
{
    {
        QMutexLocker locker(&m_mutex);
        m_dataItems.clear();
        initializeDefaultDataItems();
    }
    emit dataBatchChanged();
}

void MeterModel::randomizeData()
{
    QMutexLocker locker(&m_mutex);
    randomizeDataItems(m_dataItems, m_meterAddress, m_meterPhaseType);
    locker.unlock();
    emit dataBatchChanged();
}

void MeterModel::randomizeData(const QList<QString> &diCodes)
{
    QMutexLocker locker(&m_mutex);
    const QSet<QString> targetCodes(diCodes.begin(), diCodes.end());
    if (targetCodes.isEmpty())
    {
        return;
    }
    randomizeDataItems(m_dataItems, m_meterAddress, m_meterPhaseType, &targetCodes);
    locker.unlock();
    emit dataBatchChanged();
}

