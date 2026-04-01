#include "MeterModel.h"
#include <QRandomGenerator>
#include <QDateTime>

MeterModel::MeterModel(QObject *parent)
    : QObject(parent), m_meterAddress("202008060056"), m_operatorCode("00000000"), m_password("00000000"), m_includePrefixFE(true)
{
    initializeDefaultDataItems();
}

MeterModel::~MeterModel()
{
}

void MeterModel::setMeterAddress(const QString &address)
{
    QMutexLocker locker(&m_mutex);
    if (m_meterAddress != address)
    {
        m_meterAddress = address;
        emit addressChanged(address);
    }
}

QString MeterModel::getMeterAddress() const
{
    QMutexLocker locker(&m_mutex);
    return m_meterAddress;
}

void MeterModel::setOperatorCode(const QString &code)
{
    QMutexLocker locker(&m_mutex);
    m_operatorCode = code;
}

QString MeterModel::getOperatorCode() const
{
    QMutexLocker locker(&m_mutex);
    return m_operatorCode;
}

void MeterModel::setPassword(const QString &password)
{
    QMutexLocker locker(&m_mutex);
    m_password = password;
}

QString MeterModel::getPassword() const
{
    QMutexLocker locker(&m_mutex);
    return m_password;
}

void MeterModel::setIncludePrefixFE(bool include)
{
    QMutexLocker locker(&m_mutex);
    m_includePrefixFE = include;
}

bool MeterModel::getIncludePrefixFE() const
{
    QMutexLocker locker(&m_mutex);
    return m_includePrefixFE;
}

DataItem MeterModel::getDataItem(const QString &diCode) const
{
    QMutexLocker locker(&m_mutex);
    return m_dataItems.value(diCode, DataItem());
}

QVariant MeterModel::getDataValue(const QString &diCode) const
{
    QMutexLocker locker(&m_mutex);
    if (m_dataItems.contains(diCode))
    {
        // 检查是否为数据块
        if (m_dataItems[diCode].unit == "-")
        {
            // 数据块，返回包含相关数据项的列表
            QVariantList values;

            // 根据数据块类型，添加相关数据项
            if (diCode == "0x0201FF00")
            {
                // 电压数据块，包含A相、B相、C相电压
                if (m_dataItems.contains("0x02010300"))
                    values.append(m_dataItems["0x02010300"].value);
                if (m_dataItems.contains("0x02010200"))
                    values.append(m_dataItems["0x02010200"].value);
                if (m_dataItems.contains("0x02010100"))
                    values.append(m_dataItems["0x02010100"].value);
            }
            else if (diCode == "0x0202FF00")
            {
                // 电流数据块，包含A相、B相、C相电流
                if (m_dataItems.contains("0x02020300"))
                    values.append(m_dataItems["0x02020300"].value);
                if (m_dataItems.contains("0x02020200"))
                    values.append(m_dataItems["0x02020200"].value);
                if (m_dataItems.contains("0x02020100"))
                    values.append(m_dataItems["0x02020100"].value);
            }
            else if (diCode == "0x0203FF00")
            {
                // 功率数据块，包含A相、B相、C相功率
                if (m_dataItems.contains("0x02030300"))
                    values.append(m_dataItems["0x02030300"].value);
                if (m_dataItems.contains("0x02030200"))
                    values.append(m_dataItems["0x02030200"].value);
                if (m_dataItems.contains("0x02030100"))
                    values.append(m_dataItems["0x02030100"].value);
                if (m_dataItems.contains("0x02030000"))
                    values.append(m_dataItems["0x02030000"].value);
            }
            else if (diCode == "0x0204FF00")
            {
                if (m_dataItems.contains("0x02040300"))
                    values.append(m_dataItems["0x02040300"].value);
                if (m_dataItems.contains("0x02040200"))
                    values.append(m_dataItems["0x02040200"].value);
                if (m_dataItems.contains("0x02040100"))
                    values.append(m_dataItems["0x02040100"].value);
                if (m_dataItems.contains("0x02040000"))
                    values.append(m_dataItems["0x02040000"].value);
            }
            else if (diCode == "0x0205FF00")
            {
                if (m_dataItems.contains("0x02050300"))
                    values.append(m_dataItems["0x02050300"].value);
                if (m_dataItems.contains("0x02050200"))
                    values.append(m_dataItems["0x02050200"].value);
                if (m_dataItems.contains("0x02050100"))
                    values.append(m_dataItems["0x02050100"].value);
                if (m_dataItems.contains("0x02050000"))
                    values.append(m_dataItems["0x02050000"].value);
            }
            else if (diCode == "0x0206FF00")
            {
                // 功率因数数据块，包含总功率因数
                if (m_dataItems.contains("0x02060300"))
                    values.append(m_dataItems["0x02060300"].value);
                if (m_dataItems.contains("0x02060200"))
                    values.append(m_dataItems["0x02060200"].value);
                if (m_dataItems.contains("0x02060100"))
                    values.append(m_dataItems["0x02060100"].value);
                if (m_dataItems.contains("0x02060000"))
                    values.append(m_dataItems["0x02060000"].value);
            }
            else if (diCode == "0x0207FF00")
            {
                // 相角数据块，包含A相、B相、C相相角
                if (m_dataItems.contains("0x02070300"))
                    values.append(m_dataItems["0x02070300"].value);
                if (m_dataItems.contains("0x02070200"))
                    values.append(m_dataItems["0x02070200"].value);
                if (m_dataItems.contains("0x02070100"))
                    values.append(m_dataItems["0x02070100"].value);
            }
            else if (diCode.startsWith("0x00") && diCode.endsWith("FF00"))
            {
                // 电能数据块，包含总电能和各费率电能
                QString prefix = diCode.left(6); // 获取前6个字符，如 "0x0000"
                if (m_dataItems.contains(prefix + "0400"))
                    values.append(m_dataItems[prefix + "0400"].value);
                if (m_dataItems.contains(prefix + "0300"))
                    values.append(m_dataItems[prefix + "0300"].value);
                if (m_dataItems.contains(prefix + "0200"))
                    values.append(m_dataItems[prefix + "0200"].value);
                if (m_dataItems.contains(prefix + "0100"))
                    values.append(m_dataItems[prefix + "0100"].value);
                if (m_dataItems.contains(prefix + "0000"))
                    values.append(m_dataItems[prefix + "0000"].value);
            }

            return values;
        }
        else
        {
            // 普通数据项，直接返回值
            return m_dataItems[diCode].value;
        }
    }
    return QVariant();
}

bool MeterModel::setDataValue(const QString &diCode, const QVariant &value)
{
    QMutexLocker locker(&m_mutex);
    if (m_dataItems.contains(diCode))
    {
        m_dataItems[diCode].value = value;
        emit dataChanged(diCode, value);
        return true;
    }
    return false;
}

QList<DataItem> MeterModel::getDataItemsByCategory(DataCategory category) const
{
    QMutexLocker locker(&m_mutex);
    QList<DataItem> items;

    QString prefix;
    switch (category)
    {
    case DataCategory::Energy:
        prefix = "0x00";
        break;
    case DataCategory::Demand:
        prefix = "0x01";
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
        if (it.key().startsWith(prefix))
        {
            items.append(it.value());
        }
    }

    return items;
}

QList<DataItem> MeterModel::getAllDataItems() const
{
    QMutexLocker locker(&m_mutex);
    return m_dataItems.values();
}

void MeterModel::resetAllData()
{
    QMutexLocker locker(&m_mutex);
    for (auto it = m_dataItems.begin(); it != m_dataItems.end(); ++it)
    {
        it.value().value = 0.0;
    }
}

void MeterModel::randomizeData()
{
    QMutexLocker locker(&m_mutex);
    QRandomGenerator *rng = QRandomGenerator::global();

    for (auto it = m_dataItems.begin(); it != m_dataItems.end(); ++it)
    {
        DataItem &item = it.value();

        if (item.diCode.startsWith("0x02"))
        {
            // 变量数据：在合理范围内随机
            if (item.diCode.contains("0100") || item.diCode.contains("0200") || item.diCode.contains("0300"))
            {
                // 电压：220V ± 10%
                item.value = 220.0 + (rng->bounded(-2200, 2200) / 100.0);
            }
            else if (item.diCode.contains("0101") || item.diCode.contains("0201") || item.diCode.contains("0202"))
            {
                // 电流：0-10A
                item.value = rng->bounded(0, 10000) / 1000.0;
            }
            else if (item.diCode.contains("0300"))
            {
                // 功率：0-100kW
                item.value = rng->bounded(0, 100000) / 10000.0;
            }
            else if (item.diCode.contains("0600"))
            {
                // 功率因数：-1.0 ~ 1.0
                item.value = rng->bounded(-1000, 1000) / 1000.0;
            }
            else if (item.diCode.contains("0701"))
            {
                // 相角：0-360度
                item.value = rng->bounded(0, 3600) / 10.0;
            }
            else if (item.diCode.contains("00002"))
            {
                // 频率：50Hz ± 5Hz
                item.value = 50.0 + (rng->bounded(-50, 50) / 10.0);
            }
            else
            {
                item.value = rng->bounded(0, 1000) / 10.0;
            }
        }
        else if (item.diCode.startsWith("0x00"))
        {
            // 电能量数据：累加
            item.value = item.value.toDouble() + rng->bounded(0, 100) / 100.0;
        }
        else
        {
            // 其他数据：随机生成
            item.value = rng->bounded(0, 10000) / 100.0;
        }
    }
}

void MeterModel::initializeDefaultDataItems()
{
    initializeEnergyData();
    initializeDemandData();
    initializeVariableData();
    initializeEventData();
    initializeParameterData();
    initializeFreezeData();
    initializeLoadRecordData();
}

void MeterModel::initializeEnergyData()
{
    // 电能量数据 (0x00xxxxxx)

    // 1. 组合有功电能 [0000]
    m_dataItems["0x00000000"] = DataItem("0x00000000", "(当前)组合有功总电能", 1.2, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00000100"] = DataItem("0x00000100", "(当前)组合有功费率1电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00000200"] = DataItem("0x00000200", "(当前)组合有功费率2电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00000300"] = DataItem("0x00000300", "(当前)组合有功费率3电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00000400"] = DataItem("0x00000400", "(当前)组合有功费率4电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x0000FF00"] = DataItem("0x0000FF00", "(当前)组合有功电能数据块", 0.0, "-", "F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA", false, 2);
    // 2. 正向有功电能 [0001]
    m_dataItems["0x00010000"] = DataItem("0x00010000", "(当前)正向有功总电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00010100"] = DataItem("0x00010100", "(当前)正向有功费率1电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00010200"] = DataItem("0x00010200", "(当前)正向有功费率2电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00010300"] = DataItem("0x00010300", "(当前)正向有功费率3电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00010400"] = DataItem("0x00010400", "(当前)正向有功费率4电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x0001FF00"] = DataItem("0x0001FF00", "(当前)正向有功电能数据块", 0.0, "-", "F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA", false, 2);

    // 3. 反向有功电能 [0002]
    m_dataItems["0x00020000"] = DataItem("0x00020000", "(当前)反向有功总电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00020100"] = DataItem("0x00020100", "(当前)反向有功费率1电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00020200"] = DataItem("0x00020200", "(当前)反向有功费率2电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00020300"] = DataItem("0x00020300", "(当前)反向有功费率3电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00020400"] = DataItem("0x00020400", "(当前)反向有功费率4电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x0002FF00"] = DataItem("0x0002FF00", "(当前)反向有功电能数据块", 0.0, "-", "F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA", false, 2);

    // 4. 组合无功 1 电能 [0003]
    m_dataItems["0x00030000"] = DataItem("0x00030000", "(当前)组合无功1总电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00030100"] = DataItem("0x00030100", "(当前)组合无功1费率1电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00030200"] = DataItem("0x00030200", "(当前)组合无功1费率2电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00030300"] = DataItem("0x00030300", "(当前)组合无功1费率3电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00030400"] = DataItem("0x00030400", "(当前)组合无功1费率4电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x0003FF00"] = DataItem("0x0003FF00", "(当前)组合无功1电能数据块", 0.0, "-", "F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA", false, 2);

    // 5. 组合无功 2 电能 [0004]
    m_dataItems["0x00040000"] = DataItem("0x00040000", "(当前)组合无功2总电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00040100"] = DataItem("0x00040100", "(当前)组合无功2费率1电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00040200"] = DataItem("0x00040200", "(当前)组合无功2费率2电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00040300"] = DataItem("0x00040300", "(当前)组合无功2费率3电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00040400"] = DataItem("0x00040400", "(当前)组合无功2费率4电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x0004FF00"] = DataItem("0x0004FF00", "(当前)组合无功2电能数据块", 0.0, "-", "F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA", false, 2);

    // 6. 四象限无功电能 [0005-0008]
    // 第一象限无功电能 [0005]
    m_dataItems["0x00050000"] = DataItem("0x00050000", "(当前)第一象限无功总电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00050100"] = DataItem("0x00050100", "(当前)第一象限无功费率1电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00050200"] = DataItem("0x00050200", "(当前)第一象限无功费率2电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00050300"] = DataItem("0x00050300", "(当前)第一象限无功费率3电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00050400"] = DataItem("0x00050400", "(当前)第一象限无功费率4电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x0005FF00"] = DataItem("0x0005FF00", "(当前)第一象限无功电能数据块", 0.0, "-", "F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA", false, 2);

    // 第二象限无功电能 [0006]
    m_dataItems["0x00060000"] = DataItem("0x00060000", "(当前)第二象限无功总电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00060100"] = DataItem("0x00060100", "(当前)第二象限无功费率1电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00060200"] = DataItem("0x00060200", "(当前)第二象限无功费率2电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00060300"] = DataItem("0x00060300", "(当前)第二象限无功费率3电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00060400"] = DataItem("0x00060400", "(当前)第二象限无功费率4电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x0006FF00"] = DataItem("0x0006FF00", "(当前)第二象限无功电能数据块", 0.0, "-", "F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA", false, 2);

    // 第三象限无功电能 [0007]
    m_dataItems["0x00070000"] = DataItem("0x00070000", "(当前)第三象限无功总电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00070100"] = DataItem("0x00070100", "(当前)第三象限无功费率1电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00070200"] = DataItem("0x00070200", "(当前)第三象限无功费率2电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00070300"] = DataItem("0x00070300", "(当前)第三象限无功费率3电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00070400"] = DataItem("0x00070400", "(当前)第三象限无功费率4电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x0007FF00"] = DataItem("0x0007FF00", "(当前)第三象限无功电能数据块", 0.0, "-", "F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA", false, 2);

    // 第四象限无功电能 [0008]
    m_dataItems["0x00080000"] = DataItem("0x00080000", "(当前)第四象限无功总电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00080100"] = DataItem("0x00080100", "(当前)第四象限无功费率1电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00080200"] = DataItem("0x00080200", "(当前)第四象限无功费率2电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00080300"] = DataItem("0x00080300", "(当前)第四象限无功费率3电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00080400"] = DataItem("0x00080400", "(当前)第四象限无功费率4电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x0008FF00"] = DataItem("0x0008FF00", "(当前)第四象限无功电能数据块", 0.0, "-", "F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA", false, 2);

    // 7. 视在电能 [0009-000A]
    // 正向视在电能 [0009]
    m_dataItems["0x00090000"] = DataItem("0x00090000", "(当前)正向视在总电能", 0.0, "kVAh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00090100"] = DataItem("0x00090100", "(当前)正向视在费率1电能", 0.0, "kVAh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00090200"] = DataItem("0x00090200", "(当前)正向视在费率2电能", 0.0, "kVAh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00090300"] = DataItem("0x00090300", "(当前)正向视在费率3电能", 0.0, "kVAh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00090400"] = DataItem("0x00090400", "(当前)正向视在费率4电能", 0.0, "kVAh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x0009FF00"] = DataItem("0x0009FF00", "(当前)正向视在电能数据块", 0.0, "-", "F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA", false, 2);

    // 反向视在电能 [000A]
    m_dataItems["0x000A0000"] = DataItem("0x000A0000", "(当前)反向视在总电能", 0.0, "kVAh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x000A0100"] = DataItem("0x000A0100", "(当前)反向视在费率1电能", 0.0, "kVAh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x000A0200"] = DataItem("0x000A0200", "(当前)反向视在费率2电能", 0.0, "kVAh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x000A0300"] = DataItem("0x000A0300", "(当前)反向视在费率3电能", 0.0, "kVAh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x000A0400"] = DataItem("0x000A0400", "(当前)反向视在费率4电能", 0.0, "kVAh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x000AFF00"] = DataItem("0x000AFF00", "(当前)反向视在电能数据块", 0.0, "-", "F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA", false, 2);

    // 8. 其他电能数据 [0080-0086]
    m_dataItems["0x00800000"] = DataItem("0x00800000", "(当前)关联总电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00810000"] = DataItem("0x00810000", "(当前)正向有功基波总电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00820000"] = DataItem("0x00820000", "(当前)反向有功基波总电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00830000"] = DataItem("0x00830000", "(当前)正向有功谐波总电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00840000"] = DataItem("0x00840000", "(当前)反向有功谐波总电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00850000"] = DataItem("0x00850000", "(当前)铜损有功总电能补偿量", 0.0, "kWh", "AA,AA,AA,.,AA", false, 2);
    m_dataItems["0x00860000"] = DataItem("0x00860000", "(当前)铁损有功总电能补偿量", 0.0, "kWh", "AA,AA,AA,.,AA", false, 2);

    // 9. A 相电能数据 [0015-001E, 0094-009A]
    m_dataItems["0x00150000"] = DataItem("0x00150000", "(当前)A相正向有功电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00160000"] = DataItem("0x00160000", "(当前)A相反向有功电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00170000"] = DataItem("0x00170000", "(当前)A相组合无功1电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00180000"] = DataItem("0x00180000", "(当前)A相组合无功2电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00190000"] = DataItem("0x00190000", "(当前)A相第一象限无功电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x001A0000"] = DataItem("0x001A0000", "(当前)A相第二象限无功电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x001B0000"] = DataItem("0x001B0000", "(当前)A相第三象限无功电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x001C0000"] = DataItem("0x001C0000", "(当前)A相第四象限无功电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x001D0000"] = DataItem("0x001D0000", "(当前)A相正向视在电能", 0.0, "kVAh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x001E0000"] = DataItem("0x001E0000", "(当前)A相反向视在电能", 0.0, "kVAh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00940000"] = DataItem("0x00940000", "(当前)A相正向有功基波电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00950000"] = DataItem("0x00950000", "(当前)A相反向有功基波电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00960000"] = DataItem("0x00960000", "(当前)A相正向有功谐波电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00970000"] = DataItem("0x00970000", "(当前)A相反向有功谐波电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
}

void MeterModel::initializeDemandData()
{
    // 最大需量及发生时间 (0x01xxxxxx)
    m_dataItems["0x01010000"] = DataItem("0x01010000", "(当前)正向有功总最大需量", 0.0, "kW", "XX.XXXX", false, 4);
    m_dataItems["0x01010100"] = DataItem("0x01010100", "(当前)正向有功费率1最大需量", 0.0, "kW", "XX.XXXX", false, 4);
    m_dataItems["0x01020000"] = DataItem("0x01020000", "(当前)反向有功总最大需量", 0.0, "kW", "XX.XXXX", false, 4);
}

void MeterModel::initializeVariableData()
{
    // 变量数据 (0x02xxxxxx)
    // 电压
    m_dataItems["0x02010100"] = DataItem("0x02010100", "A相电压", 20.1, "V", "AA,xx", false, 1);
    m_dataItems["0x02010200"] = DataItem("0x02010200", "B相电压", 220.2, "V", "AA,xx", false, 1);
    m_dataItems["0x02010300"] = DataItem("0x02010300", "C相电压", 220.3, "V", "AA,xx", false, 1);
    m_dataItems["0x0201FF00"] = DataItem("0x02010400", "电压数据块", 0.0, "-", "AA,xx:AA,xx:AA,xx", false, 1);

    // 电流
    m_dataItems["0x02020100"] = DataItem("0x02020100", "A相电流", 1.01, "A", "AA,xx,AA", false, 3);
    m_dataItems["0x02020200"] = DataItem("0x02020200", "B相电流", 1.023, "A", "AA,xx,AA", false, 3);
    m_dataItems["0x02020300"] = DataItem("0x02020300", "C相电流", 1.1, "A", "AA,xx,AA", false, 3);
    m_dataItems["0x0202FF00"] = DataItem("0x0202FF00", "电流数据块", 0.0, "-", "AA,xx,AA:AA,xx,AA:AA,xx,AA", false, 3);

    // 功率
    m_dataItems["0x02030000"] = DataItem("0x02030000", "瞬时总有功功率", 0.0, "kW", "AA,.,AA,AA", false, 4);
    m_dataItems["0x02030100"] = DataItem("0x02030100", "瞬时A相有功功率", 0.0, "kW", "AA,.,AA,AA", false, 4);
    m_dataItems["0x02030200"] = DataItem("0x02030200", "瞬时B相有功功率", 0.0, "kW", "AA,.,AA,AA", false, 4);
    m_dataItems["0x02030300"] = DataItem("0x02030300", "瞬时C相有功功率", 0.0, "kW", "AA,.,AA,AA", false, 4);
    m_dataItems["0x0203FF00"] = DataItem("0x0203FF00", "瞬时有功功率数据块", 0.0, "-", "AA,.,AA,AA:AA,.,AA,AA:AA,.,AA,AA:AA,.,AA,AA", false, 4);

    m_dataItems["0x02040000"] = DataItem("0x02040000", "瞬时总无功功率", 0.0, "kvar", "F9,.,AA,AA", false, 4);
    m_dataItems["0x02040100"] = DataItem("0x02040100", "瞬时A相无功功率", 0.0, "kvar", "F9,.,AA,AA", false, 4);
    m_dataItems["0x02040200"] = DataItem("0x02040200", "瞬时B相无功功率", 0.0, "kvar", "F9,.,AA,AA", false, 4);
    m_dataItems["0x02040300"] = DataItem("0x02040300", "瞬时C相无功功率", 0.0, "kvar", "F9,.,AA,AA", false, 4);
    m_dataItems["0x0204FF00"] = DataItem("0x0204FF00", "瞬时无功功率数据块", 0.0, "-", "F9,.,AA,AA:F9,.,AA,AA:F9,.,AA,AA:F9,.,AA,AA", false, 4);

    m_dataItems["0x02050000"] = DataItem("0x02050000", "瞬时总视在功率", 0.0, "kVA", "AA,.,AA,AA", false, 4);
    m_dataItems["0x02050100"] = DataItem("0x02050100", "瞬时A相视在功率", 0.0, "kVA", "AA,.,AA,AA", false, 4);
    m_dataItems["0x02050200"] = DataItem("0x02050200", "瞬时B相视在功率", 0.0, "kVA", "AA,.,AA,AA", false, 4);
    m_dataItems["0x02050300"] = DataItem("0x02050300", "瞬时C相视在功率", 0.0, "kVA", "AA,.,AA,AA", false, 4);
    m_dataItems["0x0205FF00"] = DataItem("0x0205FF00", "瞬时视在功率数据块", 0.0, "-", "AA,.,AA,AA:AA,.,AA,AA:AA,.,AA,AA:AA,.,AA,AA", false, 4);

    // 功率因数
    m_dataItems["0x02060000"] = DataItem("0x02060000", "总功率因数", 1.0, "none", "xx,AA", false, 3);
    m_dataItems["0x02060100"] = DataItem("0x02060100", "A相功率因数", 1.0, "none", "xx,AA", false, 3);
    m_dataItems["0x02060200"] = DataItem("0x02060200", "B相功率因数", 1.0, "none", "xx,AA", false, 3);
    m_dataItems["0x02060300"] = DataItem("0x02060300", "C相功率因数", 1.0, "none", "xx,AA", false, 3);
    m_dataItems["0x0206FF00"] = DataItem("0x0206FF00", "功率因数数据块", 0.0, "-", "xx,AA:xx,AA:xx,AA:xx,AA", false, 3);

    // 相角
    m_dataItems["0x02070100"] = DataItem("0x02070100", "A相相角", 0.0, "°", "AA,xx", false, 1);
    m_dataItems["0x02070200"] = DataItem("0x02070200", "B相相角", 0.0, "°", "AA,xx", false, 1);
    m_dataItems["0x02070300"] = DataItem("0x02070300", "C相相角", 0.0, "°", "AA,xx", false, 1);
    m_dataItems["0x0207FF00"] = DataItem("0x0207FF00", "相角数据块", 0.0, "-", "AA,xx:AA,xx:AA,xx:AA,xx", false, 1);

    // 零相电流
    m_dataItems["0x02080001"] = DataItem("0x02080001", "零相电流", 0.0, "A", "AA,xx,AA", false, 3);
    // 频率
    m_dataItems["0x02800002"] = DataItem("0x02800002", "电网频率", 50.0, "Hz", "AA,.,AA", false, 2);
}

void MeterModel::initializeEventData()
{
    // 事件记录 (0x03xxxxxx)
    m_dataItems["0x03000000"] = DataItem("0x03000000", "编程记录", 0, "次", "XXXXXX", false, 0);
    m_dataItems["0x03000001"] = DataItem("0x03000001", "需量清零记录", 0, "次", "XXXXXX", false, 0);
    m_dataItems["0x03000002"] = DataItem("0x03000002", "电能表清零记录", 0, "次", "XXXXXX", false, 0);
    m_dataItems["0x03000010"] = DataItem("0x03000010", "校时记录", 0, "次", "XXXXXX", false, 0);
    m_dataItems["0x03000020"] = DataItem("0x03000020", "掉电记录", 0, "次", "XXXXXX", false, 0);
    m_dataItems["0x03000021"] = DataItem("0x03000021", "失压记录", 0, "次", "XXXXXX", false, 0);
}

void MeterModel::initializeParameterData()
{
    // 参变量 (0x04xxxxxx)
    m_dataItems["0x04000000"] = DataItem("0x04000000", "电表地址", "000000000000", "-", "XXXXXXXXXXXX", true, 0);
    m_dataItems["0x04000001"] = DataItem("0x04000001", "波特率", 2400, "bps", "XXXXX", true, 0);
    m_dataItems["0x04000040"] = DataItem("0x04000040", "需量周期", 15, "分钟", "XX", true, 0);
    m_dataItems["0x04000041"] = DataItem("0x04000041", "滑差时间", 1, "分钟", "XX", true, 0);
}

void MeterModel::initializeFreezeData()
{
    // 冻结数据 (0x05xxxxxx)
    m_dataItems["0x05000000"] = DataItem("0x05000000", "日冻结数据", 0, "-", "XXXXXX", false, 0);
    m_dataItems["0x05010000"] = DataItem("0x05010000", "月冻结数据", 0, "-", "XXXXXX", false, 0);
    m_dataItems["0x05020000"] = DataItem("0x05020000", "瞬时冻结数据", 0, "-", "XXXXXX", false, 0);
    m_dataItems["0x05030000"] = DataItem("0x05030000", "约定冻结数据", 0, "-", "XXXXXX", false, 0);
}

void MeterModel::initializeLoadRecordData()
{
    // 负荷记录 (0x06xxxxxx)
    m_dataItems["0x06000000"] = DataItem("0x06000000", "负荷记录数据", 0, "-", "XXXXXX", false, 0);
}

QList<QString> MeterModel::getEnergyDataCodes() const
{
    QMutexLocker locker(&m_mutex);
    QList<QString> codes;
    for (auto it = m_dataItems.constBegin(); it != m_dataItems.constEnd(); ++it)
    {
        // 返回名称中包含"当前"的数据项
        if (it.value().description.contains("当前"))
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
        if (it.key().startsWith("0x01"))
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
        if (it.key().startsWith("0x01"))
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
        if (it.key().startsWith("0x02"))
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
        if (it.key().startsWith("0x03"))
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
        if (it.key().startsWith("0x04"))
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
        if (it.key().startsWith("0x05"))
        {
            codes.append(it.key());
        }
    }
    return codes;
}

QList<QString> MeterModel::getLoadRecordDataCodes() const
{
    QMutexLocker locker(&m_mutex);
    QList<QString> codes;
    for (auto it = m_dataItems.constBegin(); it != m_dataItems.constEnd(); ++it)
    {
        if (it.key().startsWith("0x06"))
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
        if (it.key().startsWith("0x05") || it.key().startsWith("0x06"))
        {
            codes.append(it.key());
        }
    }
    return codes;
}

QString MeterModel::getDataName(const QString &diCode) const
{
    QMutexLocker locker(&m_mutex);
    if (m_dataItems.contains(diCode))
    {
        return m_dataItems[diCode].description;
    }
    return QString();
}

QString MeterModel::getDataUnit(const QString &diCode) const
{
    QMutexLocker locker(&m_mutex);
    if (m_dataItems.contains(diCode))
    {
        return m_dataItems[diCode].unit;
    }
    return QString();
}
