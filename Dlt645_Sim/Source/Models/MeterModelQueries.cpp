#include "MeterModelPrivate.h"

using namespace MeterModelPrivate;

QList<DataItem> MeterModel::getDataItemsByCategory(DataCategory category) const
{
    QMutexLocker locker(&m_mutex);
    QList<DataItem> items;

    QString prefix;
    switch (category)
    {
    case DataCategory::Energy:
        prefix.clear();
        break;
    case DataCategory::Demand:
        prefix.clear();
        break;
    case DataCategory::Variable:
        prefix = "0x02";
        break;
    case DataCategory::Event:
        prefix = "0x03";
        break;
    case DataCategory::Parameter:
        prefix = "0x04";
        break;
    case DataCategory::Freeze:
        prefix = "0x05";
        break;
    case DataCategory::LoadRecord:
        prefix = "0x06";
        break;
    }

    for (auto it = m_dataItems.constBegin(); it != m_dataItems.constEnd(); ++it)
    {
        const bool categoryMatched = (category == DataCategory::Energy)
                                         ? isEnergyDataCode(it.key())
                                         : (category == DataCategory::Demand)
                                               ? isDemandDataCode(it.key())
                                               : it.key().startsWith(prefix);
        if (categoryMatched && isDataItemSupportedUnlocked(it.key()))
        {
            items.append(it.value());
        }
    }

    return items;
}

QList<DataItem> MeterModel::getAllDataItems() const
{
    QMutexLocker locker(&m_mutex);
    QList<DataItem> items;
    for (auto it = m_dataItems.constBegin(); it != m_dataItems.constEnd(); ++it)
    {
        if (isDataItemSupportedUnlocked(it.key()))
        {
            items.append(it.value());
        }
    }
    return items;
}

QList<QString> MeterModel::getEnergyDataCodes() const
{
    QMutexLocker locker(&m_mutex);
    QList<QString> codes;
    for (auto it = m_dataItems.constBegin(); it != m_dataItems.constEnd(); ++it)
    {
        if (isEnergyDataCode(it.key()) && !isHistoricalEnergyCode(it.key()) && isDataItemSupportedUnlocked(it.key()))
        {
            codes.append(it.key());
        }
    }
    return codes;
}

QList<QString> MeterModel::getSettlementDayEnergyDataCodes(int settlementDay) const
{
    if (!isValidSettlementDayIndex(settlementDay) || settlementDay == 0)
    {
        return {};
    }

    QMutexLocker locker(&m_mutex);
    QList<QString> codes;
    for (auto it = m_dataItems.constBegin(); it != m_dataItems.constEnd(); ++it)
    {
        if (!isEnergyDataCode(it.key()) || !isDataItemSupportedUnlocked(it.key()))
        {
            continue;
        }

        bool ok = false;
        const int prefixValue = prefixedCodeValue(it.key(), &ok);
        if (ok && settlementDayFromPrefixValue(prefixValue) == settlementDay)
        {
            codes.append(it.key());
        }
    }
    return codes;
}

QList<QString> MeterModel::getDemandDataCodes() const
{
    QMutexLocker locker(&m_mutex);
    QList<QString> codes;
    for (auto it = m_dataItems.constBegin(); it != m_dataItems.constEnd(); ++it)
    {
        if (isDemandDataCode(it.key()) && isDataItemSupportedUnlocked(it.key()))
        {
            codes.append(it.key());
        }
    }
    return codes;
}

QList<QString> MeterModel::getSettlementDayDemandDataCodes(int settlementDay) const
{
    if (!isValidSettlementDayIndex(settlementDay))
    {
        return {};
    }

    QMutexLocker locker(&m_mutex);
    QList<QString> codes;
    for (auto it = m_dataItems.constBegin(); it != m_dataItems.constEnd(); ++it)
    {
        if (!isDemandDataCode(it.key()) || !isDataItemSupportedUnlocked(it.key()))
        {
            continue;
        }

        bool ok = false;
        const int prefixValue = prefixedCodeValue(it.key(), &ok);
        if (ok && settlementDayFromPrefixValue(prefixValue) == settlementDay)
        {
            codes.append(it.key());
        }
    }
    return codes;
}

QList<QString> MeterModel::getMaxDemandDataCodes() const
{
    QMutexLocker locker(&m_mutex);
    QList<QString> codes;
    for (auto it = m_dataItems.constBegin(); it != m_dataItems.constEnd(); ++it)
    {
        if (isDemandDataCode(it.key()) && isDataItemSupportedUnlocked(it.key()))
        {
            codes.append(it.key());
        }
    }
    return codes;
}

QList<QString> MeterModel::getVariableDataCodes() const
{
    QMutexLocker locker(&m_mutex);
    QList<QString> codes;
    for (auto it = m_dataItems.constBegin(); it != m_dataItems.constEnd(); ++it)
    {
        bool ok = false;
        const quint32 codeValue = it.key().toUInt(&ok, 16);
        if (ok &&
            codeValue >= 0x02000000 &&
            codeValue < 0x02900000 &&
            isDataItemSupportedUnlocked(it.key()))
        {
            codes.append(it.key());
        }
    }
    return codes;
}

QList<QString> MeterModel::getEventDataCodes() const
{
    QMutexLocker locker(&m_mutex);
    QList<QString> codes;
    for (auto it = m_dataItems.constBegin(); it != m_dataItems.constEnd(); ++it)
    {
        if (it.key().startsWith("0x03") && isDataItemSupportedUnlocked(it.key()))
        {
            codes.append(it.key());
        }
    }
    return codes;
}

QList<QString> MeterModel::getParameterDataCodes() const
{
    QMutexLocker locker(&m_mutex);
    QList<QString> codes;
    for (auto it = m_dataItems.constBegin(); it != m_dataItems.constEnd(); ++it)
    {
        if (it.key().startsWith("0x04") && isDataItemSupportedUnlocked(it.key()))
        {
            codes.append(it.key());
        }
    }
    return codes;
}

QList<QString> MeterModel::getFreezeDataCodes() const
{
    QMutexLocker locker(&m_mutex);
    QList<QString> codes;
    for (auto it = m_dataItems.constBegin(); it != m_dataItems.constEnd(); ++it)
    {
        if (it.key().startsWith("0x05") && isDataItemSupportedUnlocked(it.key()))
        {
            codes.append(it.key());
        }
    }
    return codes;
}

QList<QString> MeterModel::getFreezeDataCodes(const QString &freezeCategory) const
{
    QMutexLocker locker(&m_mutex);

    QString prefix;
    if (freezeCategory == QStringLiteral("定时冻结"))
    {
        prefix = QStringLiteral("0x0500");
    }
    else if (freezeCategory == QStringLiteral("瞬时冻结"))
    {
        prefix = QStringLiteral("0x0501");
    }
    else if (freezeCategory == QStringLiteral("时区切换冻结"))
    {
        prefix = QStringLiteral("0x0502");
    }
    else if (freezeCategory == QStringLiteral("日时段切换冻结"))
    {
        prefix = QStringLiteral("0x0503");
    }
    else if (freezeCategory == QStringLiteral("整点冻结"))
    {
        prefix = QStringLiteral("0x0504");
    }
    else if (freezeCategory == QStringLiteral("分时费率切换冻结"))
    {
        prefix = QStringLiteral("0x0505");
    }
    else if (freezeCategory == QStringLiteral("日冻结"))
    {
        prefix = QStringLiteral("0x0506");
    }
    else if (freezeCategory == QStringLiteral("阶梯切换冻结"))
    {
        prefix = QStringLiteral("0x0507");
    }

    QList<QString> codes;
    for (auto it = m_dataItems.constBegin(); it != m_dataItems.constEnd(); ++it)
    {
        if (!it.key().startsWith(QStringLiteral("0x05")) || !isDataItemSupportedUnlocked(it.key()))
        {
            continue;
        }

        if (!prefix.isEmpty() && !it.key().startsWith(prefix))
        {
            continue;
        }

        codes.append(it.key());
    }

    return codes;
}

QList<QString> MeterModel::getLoadRecordDataCodes() const
{
    QMutexLocker locker(&m_mutex);
    QList<QString> codes;
    for (auto it = m_dataItems.constBegin(); it != m_dataItems.constEnd(); ++it)
    {
        if (it.key().startsWith("0x06") && isDataItemSupportedUnlocked(it.key()))
        {
            codes.append(it.key());
        }
    }
    return codes;
}

QList<QString> MeterModel::getStatusDataCodes() const
{
    QMutexLocker locker(&m_mutex);
    QList<QString> codes;
    for (auto it = m_dataItems.constBegin(); it != m_dataItems.constEnd(); ++it)
    {
        if ((it.key().startsWith("0x05") || it.key().startsWith("0x06")) && isDataItemSupportedUnlocked(it.key()))
        {
            codes.append(it.key());
        }
    }
    return codes;
}

QString MeterModel::getDataName(const QString &diCode) const
{
    QMutexLocker locker(&m_mutex);
    if (isDataItemSupportedUnlocked(diCode))
    {
        return m_dataItems[diCode].description;
    }
    return QString();
}

QString MeterModel::getDataUnit(const QString &diCode) const
{
    QMutexLocker locker(&m_mutex);
    if (isDataItemSupportedUnlocked(diCode))
    {
        return m_dataItems[diCode].unit;
    }
    return QString();
}
