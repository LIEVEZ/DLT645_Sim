#include "MeterModelPrivate.h"

using namespace MeterModelPrivate;

QString MeterModel::getMeterAddress() const
{
    QMutexLocker locker(&m_mutex);
    return m_meterAddress;
}


QString MeterModel::getOperatorCode() const
{
    QMutexLocker locker(&m_mutex);
    return m_operatorCode;
}


QString MeterModel::getPassword() const
{
    QMutexLocker locker(&m_mutex);
    return m_password;
}


bool MeterModel::getIncludePrefixFE() const
{
    QMutexLocker locker(&m_mutex);
    return m_includePrefixFE;
}

bool MeterModel::getEventRecordPresent() const
{
    QMutexLocker locker(&m_mutex);
    return m_eventRecordPresent;
}


MeterPhaseType MeterModel::getMeterPhaseType() const
{
    QMutexLocker locker(&m_mutex);
    return m_meterPhaseType;
}


bool MeterModel::isSinglePhase() const
{
    return getMeterPhaseType() == MeterPhaseType::SinglePhase;
}


bool MeterModel::isDataItemSupported(const QString &diCode) const
{
    QMutexLocker locker(&m_mutex);
    return isDataItemSupportedUnlocked(diCode);
}


bool MeterModel::isDataItemSupportedUnlocked(const QString &diCode) const
{
    if (!m_dataItems.contains(diCode))
    {
        return false;
    }

    if (!isParameterStructureCodeSupported(m_dataItems, diCode))
    {
        return false;
    }

    if (m_meterPhaseType != MeterPhaseType::SinglePhase)
    {
        return isRateControlledCodeSupported(m_dataItems, diCode);
    }

    return !isSinglePhaseUnsupportedCode(diCode) && isRateControlledCodeSupported(m_dataItems, diCode);
}


DataItem MeterModel::getDataItem(const QString &diCode) const
{
    QMutexLocker locker(&m_mutex);
    if (!isDataItemSupportedUnlocked(diCode))
    {
        return DataItem();
    }

    DataItem item = m_dataItems.value(diCode, DataItem());
    const QVariant currentValue = currentDateTimeParameterValue(diCode);
    if (currentValue.isValid())
    {
        item.value = currentValue;
    }
    return item;
}

DataItem MeterModel::getDataItemRaw(const QString &diCode) const
{
    QMutexLocker locker(&m_mutex);
    DataItem item = m_dataItems.value(diCode, DataItem());
    const QVariant currentValue = currentDateTimeParameterValue(diCode);
    if (currentValue.isValid())
    {
        item.value = currentValue;
    }
    return item;
}


QVariant MeterModel::getDataValue(const QString &diCode) const
{
    QMutexLocker locker(&m_mutex);
    if (isDataItemSupportedUnlocked(diCode))
    {
        const QVariant currentValue = currentDateTimeParameterValue(diCode);
        if (currentValue.isValid())
        {
            return currentValue;
        }

        // 检查是否为数据块
        if (m_dataItems[diCode].unit == "-")
        {
            // 数据块，返回包含相关数据项的列表
            QVariantList values;

            // 根据数据块类型，添加相关数据项
            if (diCode == "0x0201FF00")
            {
                // 电压数据块，包含A相、B相、C相电压
                if (m_dataItems.contains("0x02010100"))
                    values.append(m_dataItems["0x02010100"].value);
                if (m_dataItems.contains("0x02010200"))
                    values.append(m_dataItems["0x02010200"].value);
                if (m_dataItems.contains("0x02010300"))
                    values.append(m_dataItems["0x02010300"].value);
            }
            else if (diCode == "0x0202FF00")
            {
                // 电流数据块，包含A相、B相、C相电流
                if (m_dataItems.contains("0x02020100"))
                    values.append(m_dataItems["0x02020100"].value);
                if (m_dataItems.contains("0x02020200"))
                    values.append(m_dataItems["0x02020200"].value);
                if (m_dataItems.contains("0x02020300"))
                    values.append(m_dataItems["0x02020300"].value);
            }
            else if (diCode == "0x0203FF00")
            {
                // 功率数据块，包含A相、B相、C相功率
                if (m_dataItems.contains("0x02030000"))
                    values.append(m_dataItems["0x02030000"].value);
                if (m_dataItems.contains("0x02030100"))
                    values.append(m_dataItems["0x02030100"].value);
                if (m_dataItems.contains("0x02030200"))
                    values.append(m_dataItems["0x02030200"].value);
                if (m_dataItems.contains("0x02030300"))
                    values.append(m_dataItems["0x02030300"].value);
            }
            else if (diCode == "0x0204FF00")
            {
                if (m_dataItems.contains("0x02040000"))
                    values.append(m_dataItems["0x02040000"].value);
                if (m_dataItems.contains("0x02040100"))
                    values.append(m_dataItems["0x02040100"].value);
                if (m_dataItems.contains("0x02040200"))
                    values.append(m_dataItems["0x02040200"].value);
                if (m_dataItems.contains("0x02040300"))
                    values.append(m_dataItems["0x02040300"].value);
            }
            else if (diCode == "0x0205FF00")
            {
                if (m_dataItems.contains("0x02050000"))
                    values.append(m_dataItems["0x02050000"].value);
                if (m_dataItems.contains("0x02050100"))
                    values.append(m_dataItems["0x02050100"].value);
                if (m_dataItems.contains("0x02050200"))
                    values.append(m_dataItems["0x02050200"].value);
                if (m_dataItems.contains("0x02050300"))
                    values.append(m_dataItems["0x02050300"].value);
            }
            else if (diCode == "0x0206FF00")
            {
                // 功率因数数据块，包含总功率因数
                if (m_dataItems.contains("0x02060000"))
                    values.append(m_dataItems["0x02060000"].value);
                if (m_dataItems.contains("0x02060100"))
                    values.append(m_dataItems["0x02060100"].value);
                if (m_dataItems.contains("0x02060200"))
                    values.append(m_dataItems["0x02060200"].value);
                if (m_dataItems.contains("0x02060300"))
                    values.append(m_dataItems["0x02060300"].value);
            }
            else if (diCode == "0x0207FF00")
            {
                // 相角数据块，包含A相、B相、C相相角
                if (m_dataItems.contains("0x02070100"))
                    values.append(m_dataItems["0x02070100"].value);
                if (m_dataItems.contains("0x02070200"))
                    values.append(m_dataItems["0x02070200"].value);
                if (m_dataItems.contains("0x02070300"))
                    values.append(m_dataItems["0x02070300"].value);
            }
            else if (diCode == "0x0208FF00")
            {
                if (m_dataItems.contains("0x02080100"))
                    values.append(m_dataItems["0x02080100"].value);
                if (m_dataItems.contains("0x02080200"))
                    values.append(m_dataItems["0x02080200"].value);
                if (m_dataItems.contains("0x02080300"))
                    values.append(m_dataItems["0x02080300"].value);
            }
            else if (diCode == "0x0209FF00")
            {
                if (m_dataItems.contains("0x02090100"))
                    values.append(m_dataItems["0x02090100"].value);
                if (m_dataItems.contains("0x02090200"))
                    values.append(m_dataItems["0x02090200"].value);
                if (m_dataItems.contains("0x02090300"))
                    values.append(m_dataItems["0x02090300"].value);
            }
            else if (isHarmonicFamilyCode(diCode, QStringLiteral("0x020A"), nullptr, nullptr, nullptr) ||
                     isHarmonicFamilyCode(diCode, QStringLiteral("0x020B"), nullptr, nullptr, nullptr))
            {
                appendValuesForCodes(m_dataItems, harmonicPayloadCodes(diCode), values);
            }
            else if (diCode == "0x230100FF")
            {
                appendValuesForCodes(m_dataItems,
                                     {QStringLiteral("0x23010001"),
                                      QStringLiteral("0x23010002"),
                                      QStringLiteral("0x23010003"),
                                      QStringLiteral("0x23010004"),
                                      QStringLiteral("0x23010005"),
                                      QStringLiteral("0x23010006"),
                                      QStringLiteral("0x23010007"),
                                      QStringLiteral("0x23010008")},
                                     values);
            }
            else if (diCode.endsWith("FF00") && isMultiRateEnergyCode(diCode))
            {
                appendValuesForCodes(m_dataItems, currentEnergyBlockCodes(m_dataItems, diCode), values);
            }
            else if (diCode.endsWith(QStringLiteral("FF00")) && isDemandDataCode(diCode))
            {
                appendValuesForCodes(m_dataItems, currentDemandBlockCodes(m_dataItems, diCode.left(6)), values);
            }
            else if (isFreezeEnergyBlockCode(diCode))
            {
                const QString basePrefix = diCode.left(8);
                appendValuesForCodes(m_dataItems, currentFreezeEnergyBlockCodes(m_dataItems, basePrefix), values);
            }

            if (!values.isEmpty())
            {
                return values;
            }
            return m_dataItems[diCode].value;
        }
        else
        {
            // 普通数据项，直接返回值
            return m_dataItems[diCode].value;
        }
    }
    return QVariant();
}


