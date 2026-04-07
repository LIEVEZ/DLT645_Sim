#include "MeterModel.h"
#include <QRandomGenerator>
#include <QDateTime>
#include <QDebug>
#include <QSet>
#include <cmath>

namespace
{
    struct NumericFormatSpec
    {
        bool valid = false;
        bool allowNegative = false;
        int integerDigits = 0;
        int decimalDigits = 0;
    };

    bool isSupportedNumericToken(const QString &token)
    {
        return token == QStringLiteral("AA") || token == QStringLiteral("xx");
    }

    bool isDataBlockItem(const DataItem &item)
    {
        return item.format.contains(':') || item.diCode.endsWith(QStringLiteral("FF00"));
    }

    NumericFormatSpec parseNumericFormatSpec(const QString &format)
    {
        NumericFormatSpec spec;
        if (format.isEmpty() || format.contains(':'))
        {
            return spec;
        }

        QStringList tokens = format.split(',', Qt::SkipEmptyParts);
        for (QString &token : tokens)
        {
            token = token.trimmed();
        }

        if (tokens.isEmpty())
        {
            return spec;
        }

        spec.allowNegative = (tokens.first() == QStringLiteral("F9"));

        QStringList nonF9Tokens;
        for (const QString &token : tokens)
        {
            if (token == QStringLiteral("F9"))
            {
                continue;
            }

            if (token == QStringLiteral(".") || isSupportedNumericToken(token))
            {
                nonF9Tokens.append(token);
                continue;
            }

            return NumericFormatSpec();
        }

        if (nonF9Tokens.isEmpty())
        {
            return spec;
        }

        const int decimalIndex = nonF9Tokens.indexOf(QStringLiteral("."));
        const int xxCount = nonF9Tokens.count(QStringLiteral("xx"));

        if (xxCount > 1)
        {
            return NumericFormatSpec();
        }

        if (decimalIndex != -1)
        {
            int digitsBeforeDecimal = 0;
            int digitsAfterDecimal = 0;

            for (int i = 0; i < nonF9Tokens.size(); ++i)
            {
                if (nonF9Tokens[i] == QStringLiteral("."))
                {
                    continue;
                }

                if (i < decimalIndex)
                {
                    digitsBeforeDecimal += 2;
                }
                else
                {
                    digitsAfterDecimal += 2;
                }
            }

            spec.valid = true;
            spec.integerDigits = digitsBeforeDecimal;
            if (spec.allowNegative && decimalIndex == 0)
            {
                spec.integerDigits += 2;
            }
            spec.decimalDigits = digitsAfterDecimal;
            return spec;
        }

        if (xxCount == 1)
        {
            const int xxIndex = nonF9Tokens.indexOf(QStringLiteral("xx"));
            int totalDigits = 0;
            int digitsAfterXx = 0;

            for (int i = 0; i < nonF9Tokens.size(); ++i)
            {
                if (nonF9Tokens[i] == QStringLiteral("."))
                {
                    continue;
                }

                totalDigits += 2;
                if (i > xxIndex)
                {
                    digitsAfterXx += 2;
                }
            }

            spec.valid = true;
            spec.decimalDigits = digitsAfterXx + 1;
            spec.integerDigits = qMax(0, totalDigits - spec.decimalDigits);
            return spec;
        }

        int totalDigits = 0;
        for (const QString &token : nonF9Tokens)
        {
            if (token != QStringLiteral("."))
            {
                totalDigits += 2;
            }
        }

        spec.valid = (totalDigits > 0);
        spec.integerDigits = totalDigits;
        spec.decimalDigits = 0;
        return spec;
    }

    double maxRepresentableValue(const NumericFormatSpec &spec)
    {
        const double scale = std::pow(10.0, spec.decimalDigits);
        const double step = (scale > 1.0) ? (1.0 / scale) : 1.0;
        return std::pow(10.0, spec.integerDigits) - step;
    }

    QVariant normalizeValueForFormat(const QVariant &value, const NumericFormatSpec &spec)
    {
        bool ok = false;
        double numericValue = value.toDouble(&ok);
        if (!ok || !spec.valid)
        {
            return value;
        }

        const double scale = std::pow(10.0, spec.decimalDigits);
        const double maxValue = maxRepresentableValue(spec);
        const double minValue = spec.allowNegative ? -maxValue : 0.0;

        double normalizedValue = numericValue;
        if (scale > 1.0)
        {
            normalizedValue = std::round(normalizedValue * scale) / scale;
        }
        else
        {
            normalizedValue = std::round(normalizedValue);
        }

        normalizedValue = qBound(minValue, normalizedValue, maxValue);

        if (spec.decimalDigits == 0)
        {
            return static_cast<qint64>(std::llround(normalizedValue));
        }

        return normalizedValue;
    }

    double roundToDecimals(double value, int decimalPlaces)
    {
        if (decimalPlaces <= 0)
        {
            return std::round(value);
        }

        const double scale = std::pow(10.0, decimalPlaces);
        return std::round(value * scale) / scale;
    }

    void synchronizeGroupTotal(QMap<QString, DataItem> &items,
                               const QString &totalCode,
                               const QStringList &rateCodes)
    {
        if (!items.contains(totalCode))
        {
            return;
        }

        double sum = 0.0;
        for (const QString &rateCode : rateCodes)
        {
            if (!items.contains(rateCode))
            {
                return;
            }

            sum += items[rateCode].value.toDouble();
        }

        DataItem &totalItem = items[totalCode];
        const double roundedSum = roundToDecimals(sum, totalItem.decimalPlaces);
        const NumericFormatSpec spec = parseNumericFormatSpec(totalItem.format);
        totalItem.value = spec.valid ? normalizeValueForFormat(roundedSum, spec) : QVariant(roundedSum);
    }

    void synchronizeCurrentEnergyTotals(QMap<QString, DataItem> &items)
    {
        const QStringList energyPrefixes = {
            "0x0000",
            "0x0001",
            "0x0002",
            "0x0003",
            "0x0004",
            "0x0005",
            "0x0006",
            "0x0007",
            "0x0008",
            "0x0009",
            "0x000A"};

        for (const QString &prefix : energyPrefixes)
        {
            synchronizeGroupTotal(items,
                                  prefix + "0000",
                                  {prefix + "0100", prefix + "0200", prefix + "0300", prefix + "0400"});
        }
    }

    void synchronizeDailyFreezeTotals(QMap<QString, DataItem> &items)
    {
        synchronizeGroupTotal(items, "0x05060100", {"0x05060102", "0x05060103", "0x05060104", "0x05060105"});
        synchronizeGroupTotal(items, "0x05060200", {"0x05060202", "0x05060203", "0x05060204", "0x05060205"});
        synchronizeGroupTotal(items, "0x05060300", {"0x05060302", "0x05060303", "0x05060304", "0x05060305"});
        synchronizeGroupTotal(items, "0x05060400", {"0x05060402", "0x05060403", "0x05060404", "0x05060405"});
    }

    void synchronizeInstantaneousPowerTotals(QMap<QString, DataItem> &items)
    {
        synchronizeGroupTotal(items, "0x02030000", {"0x02030100", "0x02030200", "0x02030300"});
        synchronizeGroupTotal(items, "0x02040000", {"0x02040100", "0x02040200", "0x02040300"});
        synchronizeGroupTotal(items, "0x02050000", {"0x02050100", "0x02050200", "0x02050300"});
    }

    void synchronizeDemandTotals(QMap<QString, DataItem> &items)
    {
        synchronizeGroupTotal(items, "0x01010000", {"0x01010100", "0x01010200", "0x01010300", "0x01010400"});
        synchronizeGroupTotal(items, "0x01020000", {"0x01020100", "0x01020200", "0x01020300", "0x01020400"});
    }

    QString compactDateTimeToMinutes(const QDateTime &dateTime)
    {
        return dateTime.toString(QStringLiteral("yyMMddHHmm"));
    }

    QString compactDateWithWeek(const QDate &date)
    {
        const int weekCode = date.dayOfWeek() % 7;
        return date.toString(QStringLiteral("yyMMdd")) + QStringLiteral("%1").arg(weekCode, 2, 10, QChar('0'));
    }

    void randomizeDataItems(QMap<QString, DataItem> &items,
                            const QString &meterAddress,
                            const QSet<QString> *targetCodes = nullptr)
    {
        QRandomGenerator *rng = QRandomGenerator::global();

        for (auto it = items.begin(); it != items.end(); ++it)
        {
            DataItem &item = it.value();
            if (targetCodes && !targetCodes->contains(item.diCode))
            {
                continue;
            }

            if (isDataBlockItem(item))
            {
                continue;
            }

            if (item.diCode == "0x04000000" || item.diCode == "0x04000401")
            {
                item.value = meterAddress;
                continue;
            }

            if (item.diCode == "0x04000101")
            {
                item.value = compactDateWithWeek(QDate::currentDate().addDays(-rng->bounded(0, 7)));
                continue;
            }

            if (item.diCode == "0x04000102")
            {
                item.value = QTime::currentTime().addSecs(rng->bounded(0, 24 * 60 * 60)).toString(QStringLiteral("HHmmss"));
                continue;
            }

            if (item.format == "AA,AA,AA,AA,AA" && (item.description.contains(QStringLiteral("鏃堕棿")) || item.description.contains(QStringLiteral("鍙戠敓"))))
            {
                const QDateTime randomTime = QDateTime::currentDateTime().addSecs(-rng->bounded(0, 30 * 24 * 60 * 60));
                item.value = compactDateTimeToMinutes(randomTime);
                continue;
            }

            if (item.diCode.startsWith("0x01") && item.diCode.endsWith("01") && item.format == "AA,AA,AA,AA,AA")
            {
                const QDateTime randomTime = QDateTime::currentDateTime().addSecs(-rng->bounded(1, 24 * 60 * 60 + 1));
                item.value = compactDateTimeToMinutes(randomTime);
                continue;
            }

            if (item.diCode.startsWith("0x06") && item.format == "AA,AA")
            {
                const int duration = rng->bounded(1, 60);
                const int unitCode = (rng->bounded(0, 2) == 0) ? 2 : 3;
                item.value = QStringLiteral("%1%2")
                                 .arg(duration, 2, 10, QChar('0'))
                                 .arg(unitCode, 2, 10, QChar('0'));
                continue;
            }

            const QVariant originalValue = item.value;
            const NumericFormatSpec formatSpec = parseNumericFormatSpec(item.format);
            bool variableValueHandled = false;

            if (item.diCode.startsWith("0x0202") || item.diCode == "0x02080001")
            {
                item.value = rng->bounded(0, 10000) / 1000.0;
                variableValueHandled = true;
            }
            else if (item.diCode.startsWith("0x0203"))
            {
                item.value = rng->bounded(0, 300001) / 10000.0;
                variableValueHandled = true;
            }
            else if (item.diCode.startsWith("0x0204"))
            {
                item.value = rng->bounded(-300000, 300001) / 10000.0;
                variableValueHandled = true;
            }
            else if (item.diCode.startsWith("0x0205"))
            {
                item.value = rng->bounded(0, 300001) / 10000.0;
                variableValueHandled = true;
            }
            else if (item.diCode.startsWith("0x0206"))
            {
                item.value = formatSpec.allowNegative
                                 ? rng->bounded(-1000, 1001) / 1000.0
                                 : rng->bounded(0, 1001) / 1000.0;
                variableValueHandled = true;
            }
            else if (item.diCode.startsWith("0x0207"))
            {
                item.value = rng->bounded(0, 3600) / 10.0;
                variableValueHandled = true;
            }
            else if (item.diCode == "0x02800002")
            {
                item.value = 50.0 + (rng->bounded(-50, 51) / 10.0);
                variableValueHandled = true;
            }

            if (!variableValueHandled && item.diCode.startsWith("0x0201"))
            {
                if (item.diCode.contains("0100") || item.diCode.contains("0200") || item.diCode.contains("0300"))
                {
                    item.value = 220.0 + (rng->bounded(-2200, 2200) / 100.0);
                }
                else if (item.diCode.contains("0101") || item.diCode.contains("0201") || item.diCode.contains("0202"))
                {
                    item.value = rng->bounded(0, 10000) / 1000.0;
                }
                else if (item.diCode.contains("0300"))
                {
                    item.value = rng->bounded(0, 100000) / 10000.0;
                }
                else if (item.diCode.contains("0600"))
                {
                    item.value = formatSpec.allowNegative
                                     ? rng->bounded(-1000, 1000) / 1000.0
                                     : rng->bounded(0, 1000) / 1000.0;
                }
                else if (item.diCode.contains("0701"))
                {
                    item.value = rng->bounded(0, 3600) / 10.0;
                }
                else if (item.diCode.contains("00002"))
                {
                    item.value = 50.0 + (rng->bounded(-50, 50) / 10.0);
                }
                else
                {
                    item.value = rng->bounded(0, 1000) / 10.0;
                }
            }
            else if (!variableValueHandled && item.diCode.startsWith("0x00"))
            {
                item.value = item.value.toDouble() + rng->bounded(0, 100) / 100.0;
            }
            else if (!variableValueHandled)
            {
                item.value = rng->bounded(0, 10000) / 100.0;
            }

            if (formatSpec.valid)
            {
                item.value = normalizeValueForFormat(item.value, formatSpec);
            }
            else
            {
                item.value = originalValue;
            }
        }

        synchronizeDemandTotals(items);
        synchronizeInstantaneousPowerTotals(items);
        synchronizeCurrentEnergyTotals(items);
        synchronizeDailyFreezeTotals(items);
    }
}
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
    bool changed = false;
    {
        QMutexLocker locker(&m_mutex);
        if (m_meterAddress != address)
        {
            m_meterAddress = address;
            if (m_dataItems.contains("0x04000000"))
            {
                m_dataItems["0x04000000"].value = address;
            }
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
            else if (diCode == "0x0101FF00")
            {
                const QStringList codes = {"0x01010401", "0x01010400",
                                           "0x01010301", "0x01010300",
                                           "0x01010201", "0x01010200",
                                           "0x01010101", "0x01010100",
                                           "0x01010001", "0x01010000"};
                for (const QString &code : codes)
                {
                    if (m_dataItems.contains(code))
                    {
                        values.append(m_dataItems[code].value);
                    }
                }
            }
            else if (diCode == "0x0102FF00")
            {
                const QStringList codes = {"0x01020401", "0x01020400",
                                           "0x01020301", "0x01020300",
                                           "0x01020201", "0x01020200",
                                           "0x01020101", "0x01020100",
                                           "0x01020001", "0x01020000"};
                for (const QString &code : codes)
                {
                    if (m_dataItems.contains(code))
                    {
                        values.append(m_dataItems[code].value);
                    }
                }
            }
            else if (diCode == "0x05060101" || diCode == "0x05060201" || diCode == "0x05060301" || diCode == "0x05060401")
            {
                const QString basePrefix = diCode.left(8);
                const QStringList codes = {basePrefix + "05",
                                           basePrefix + "04",
                                           basePrefix + "03",
                                           basePrefix + "02",
                                           basePrefix + "00"};
                for (const QString &code : codes)
                {
                    if (m_dataItems.contains(code))
                    {
                        values.append(m_dataItems[code].value);
                    }
                }
            }

            if (!values.isEmpty())
            {
                qDebug() << "MeterModel::getDataValue: " << diCode << " " << values;
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

bool MeterModel::setDataValue(const QString &diCode, const QVariant &value)
{
    bool updated = false;
    {
        QMutexLocker locker(&m_mutex);
        if (m_dataItems.contains(diCode))
        {
            m_dataItems[diCode].value = value;
            if (diCode.startsWith("0x00"))
            {
                synchronizeCurrentEnergyTotals(m_dataItems);
            }
            if (diCode.startsWith("0x0101") || diCode.startsWith("0x0102"))
            {
                synchronizeDemandTotals(m_dataItems);
            }
            if (diCode.startsWith("0x0203") || diCode.startsWith("0x0204") || diCode.startsWith("0x0205"))
            {
                synchronizeInstantaneousPowerTotals(m_dataItems);
            }
            if (diCode.startsWith("0x050601") || diCode.startsWith("0x050602") || diCode.startsWith("0x050603") || diCode.startsWith("0x050604"))
            {
                synchronizeDailyFreezeTotals(m_dataItems);
            }
            updated = true;
        }
    }

    if (updated)
    {
        emit dataChanged(diCode, value);
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
                m_dataItems[it.key()].value = it.value();
            }
        }

        if (m_dataItems.contains("0x04000000"))
        {
            m_dataItems["0x04000000"].value = m_meterAddress;
        }
        if (m_dataItems.contains("0x04000401"))
        {
            m_dataItems["0x04000401"].value = m_meterAddress;
        }

        synchronizeDemandTotals(m_dataItems);
        synchronizeInstantaneousPowerTotals(m_dataItems);
        synchronizeCurrentEnergyTotals(m_dataItems);
        synchronizeDailyFreezeTotals(m_dataItems);
    }

    emit dataBatchChanged();
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
    randomizeDataItems(m_dataItems, m_meterAddress);
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
    randomizeDataItems(m_dataItems, m_meterAddress, &targetCodes);
    locker.unlock();
    emit dataBatchChanged();
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
    m_dataItems["0x0000FF00"] = DataItem("0x0000FF00", "(当前)组合有功电能数据块", QVariantList(), "-", "F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA", false, 2);
    // 2. 正向有功电能 [0001]
    m_dataItems["0x00010000"] = DataItem("0x00010000", "(当前)正向有功总电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00010100"] = DataItem("0x00010100", "(当前)正向有功费率1电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00010200"] = DataItem("0x00010200", "(当前)正向有功费率2电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00010300"] = DataItem("0x00010300", "(当前)正向有功费率3电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00010400"] = DataItem("0x00010400", "(当前)正向有功费率4电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x0001FF00"] = DataItem("0x0001FF00", "(当前)正向有功电能数据块", QVariantList(), "-", "F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA", false, 2);

    // 3. 反向有功电能 [0002]
    m_dataItems["0x00020000"] = DataItem("0x00020000", "(当前)反向有功总电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00020100"] = DataItem("0x00020100", "(当前)反向有功费率1电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00020200"] = DataItem("0x00020200", "(当前)反向有功费率2电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00020300"] = DataItem("0x00020300", "(当前)反向有功费率3电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00020400"] = DataItem("0x00020400", "(当前)反向有功费率4电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x0002FF00"] = DataItem("0x0002FF00", "(当前)反向有功电能数据块", QVariantList(), "-", "F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA", false, 2);

    // 4. 组合无功 1 电能 [0003]
    m_dataItems["0x00030000"] = DataItem("0x00030000", "(当前)组合无功1总电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00030100"] = DataItem("0x00030100", "(当前)组合无功1费率1电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00030200"] = DataItem("0x00030200", "(当前)组合无功1费率2电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00030300"] = DataItem("0x00030300", "(当前)组合无功1费率3电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00030400"] = DataItem("0x00030400", "(当前)组合无功1费率4电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x0003FF00"] = DataItem("0x0003FF00", "(当前)组合无功1电能数据块", QVariantList(), "-", "F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA", false, 2);

    // 5. 组合无功 2 电能 [0004]
    m_dataItems["0x00040000"] = DataItem("0x00040000", "(当前)组合无功2总电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00040100"] = DataItem("0x00040100", "(当前)组合无功2费率1电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00040200"] = DataItem("0x00040200", "(当前)组合无功2费率2电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00040300"] = DataItem("0x00040300", "(当前)组合无功2费率3电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00040400"] = DataItem("0x00040400", "(当前)组合无功2费率4电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x0004FF00"] = DataItem("0x0004FF00", "(当前)组合无功2电能数据块", QVariantList(), "-", "F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA", false, 2);

    // 6. 四象限无功电能 [0005-0008]
    // 第一象限无功电能 [0005]
    m_dataItems["0x00050000"] = DataItem("0x00050000", "(当前)第一象限无功总电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00050100"] = DataItem("0x00050100", "(当前)第一象限无功费率1电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00050200"] = DataItem("0x00050200", "(当前)第一象限无功费率2电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00050300"] = DataItem("0x00050300", "(当前)第一象限无功费率3电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00050400"] = DataItem("0x00050400", "(当前)第一象限无功费率4电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x0005FF00"] = DataItem("0x0005FF00", "(当前)第一象限无功电能数据块", QVariantList(), "-", "F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA", false, 2);

    // 第二象限无功电能 [0006]
    m_dataItems["0x00060000"] = DataItem("0x00060000", "(当前)第二象限无功总电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00060100"] = DataItem("0x00060100", "(当前)第二象限无功费率1电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00060200"] = DataItem("0x00060200", "(当前)第二象限无功费率2电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00060300"] = DataItem("0x00060300", "(当前)第二象限无功费率3电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00060400"] = DataItem("0x00060400", "(当前)第二象限无功费率4电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x0006FF00"] = DataItem("0x0006FF00", "(当前)第二象限无功电能数据块", QVariantList(), "-", "F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA", false, 2);

    // 第三象限无功电能 [0007]
    m_dataItems["0x00070000"] = DataItem("0x00070000", "(当前)第三象限无功总电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00070100"] = DataItem("0x00070100", "(当前)第三象限无功费率1电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00070200"] = DataItem("0x00070200", "(当前)第三象限无功费率2电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00070300"] = DataItem("0x00070300", "(当前)第三象限无功费率3电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00070400"] = DataItem("0x00070400", "(当前)第三象限无功费率4电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x0007FF00"] = DataItem("0x0007FF00", "(当前)第三象限无功电能数据块", QVariantList(), "-", "F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA", false, 2);

    // 第四象限无功电能 [0008]
    m_dataItems["0x00080000"] = DataItem("0x00080000", "(当前)第四象限无功总电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00080100"] = DataItem("0x00080100", "(当前)第四象限无功费率1电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00080200"] = DataItem("0x00080200", "(当前)第四象限无功费率2电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00080300"] = DataItem("0x00080300", "(当前)第四象限无功费率3电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00080400"] = DataItem("0x00080400", "(当前)第四象限无功费率4电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x0008FF00"] = DataItem("0x0008FF00", "(当前)第四象限无功电能数据块", QVariantList(), "-", "F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA", false, 2);

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
    synchronizeCurrentEnergyTotals(m_dataItems);
}

void MeterModel::initializeDemandData()
{
    // 最大需量及发生时间 (0x01xxxxxx)
    const QDateTime now = QDateTime::currentDateTime();

    m_dataItems["0x01010000"] = DataItem("0x01010000", "(当前)正向有功总最大需量", 36.3985, "kW", "AA,AA,.,AA,AA", false, 4);
    m_dataItems["0x01010001"] = DataItem("0x01010001", "(当前)正向有功总最大需量发生时间", compactDateTimeToMinutes(now.addSecs(-5 * 60 * 60)), "-", "AA,AA,AA,AA,AA", false, 0);
    m_dataItems["0x01010100"] = DataItem("0x01010100", "(当前)正向有功费率1最大需量", 10.1023, "kW", "AA,.,AA,AA", false, 4);
    m_dataItems["0x01010101"] = DataItem("0x01010101", "(当前)正向有功费率1最大需量发生时间", compactDateTimeToMinutes(now.addSecs(-4 * 60 * 60)), "-", "AA,AA,AA,AA,AA", false, 0);
    m_dataItems["0x01010200"] = DataItem("0x01010200", "(当前)正向有功费率2最大需量", 9.8765, "kW", "AA,.,AA,AA", false, 4);
    m_dataItems["0x01010201"] = DataItem("0x01010201", "(当前)正向有功费率2最大需量发生时间", compactDateTimeToMinutes(now.addSecs(-3 * 60 * 60)), "-", "AA,AA,AA,AA,AA", false, 0);
    m_dataItems["0x01010300"] = DataItem("0x01010300", "(当前)正向有功费率3最大需量", 8.7654, "kW", "AA,.,AA,AA", false, 4);
    m_dataItems["0x01010301"] = DataItem("0x01010301", "(当前)正向有功费率3最大需量发生时间", compactDateTimeToMinutes(now.addSecs(-2 * 60 * 60)), "-", "AA,AA,AA,AA,AA", false, 0);
    m_dataItems["0x01010400"] = DataItem("0x01010400", "(当前)正向有功费率4最大需量", 7.6543, "kW", "AA,.,AA,AA", false, 4);
    m_dataItems["0x01010401"] = DataItem("0x01010401", "(当前)正向有功费率4最大需量发生时间", compactDateTimeToMinutes(now.addSecs(-1 * 60 * 60)), "-", "AA,AA,AA,AA,AA", false, 0);
    m_dataItems["0x0101FF00"] = DataItem("0x0101FF00", "(当前)正向有功最大需量及发生时间数据块", QVariantList(), "-", "AA,AA,AA,AA,AA:AA,.,AA,AA:AA,AA,AA,AA,AA:AA,.,AA,AA:AA,AA,AA,AA,AA:AA,.,AA,AA:AA,AA,AA,AA,AA:AA,.,AA,AA:AA,AA,AA,AA,AA:AA,.,AA,AA", false, 0);

    m_dataItems["0x01020000"] = DataItem("0x01020000", "(当前)反向有功总最大需量", 15.0738, "kW", "AA,AA,.,AA,AA", false, 4);
    m_dataItems["0x01020001"] = DataItem("0x01020001", "(当前)反向有功总最大需量发生时间", compactDateTimeToMinutes(now.addSecs(-10 * 60 * 60)), "-", "AA,AA,AA,AA,AA", false, 0);
    m_dataItems["0x01020100"] = DataItem("0x01020100", "(当前)反向有功费率1最大需量", 5.4321, "kW", "AA,.,AA,AA", false, 4);
    m_dataItems["0x01020101"] = DataItem("0x01020101", "(当前)反向有功费率1最大需量发生时间", compactDateTimeToMinutes(now.addSecs(-9 * 60 * 60)), "-", "AA,AA,AA,AA,AA", false, 0);
    m_dataItems["0x01020200"] = DataItem("0x01020200", "(当前)反向有功费率2最大需量", 4.321, "kW", "AA,.,AA,AA", false, 4);
    m_dataItems["0x01020201"] = DataItem("0x01020201", "(当前)反向有功费率2最大需量发生时间", compactDateTimeToMinutes(now.addSecs(-8 * 60 * 60)), "-", "AA,AA,AA,AA,AA", false, 0);
    m_dataItems["0x01020300"] = DataItem("0x01020300", "(当前)反向有功费率3最大需量", 3.2109, "kW", "AA,.,AA,AA", false, 4);
    m_dataItems["0x01020301"] = DataItem("0x01020301", "(当前)反向有功费率3最大需量发生时间", compactDateTimeToMinutes(now.addSecs(-7 * 60 * 60)), "-", "AA,AA,AA,AA,AA", false, 0);
    m_dataItems["0x01020400"] = DataItem("0x01020400", "(当前)反向有功费率4最大需量", 2.1098, "kW", "AA,.,AA,AA", false, 4);
    m_dataItems["0x01020401"] = DataItem("0x01020401", "(当前)反向有功费率4最大需量发生时间", compactDateTimeToMinutes(now.addSecs(-6 * 60 * 60)), "-", "AA,AA,AA,AA,AA", false, 0);
    m_dataItems["0x0102FF00"] = DataItem("0x0102FF00", "(当前)反向有功最大需量及发生时间数据块", QVariantList(), "-", "AA,AA,AA,AA,AA:AA,.,AA,AA:AA,AA,AA,AA,AA:AA,.,AA,AA:AA,AA,AA,AA,AA:AA,.,AA,AA:AA,AA,AA,AA,AA:AA,.,AA,AA:AA,AA,AA,AA,AA:AA,.,AA,AA", false, 0);
    synchronizeDemandTotals(m_dataItems);
}

void MeterModel::initializeVariableData()
{
    // 变量数据 (0x02xxxxxx)
    // 电压
    m_dataItems["0x02010100"] = DataItem("0x02010100", "A相电压", 20.1, "V", "AA,xx", false, 1);
    m_dataItems["0x02010200"] = DataItem("0x02010200", "B相电压", 220.2, "V", "AA,xx", false, 1);
    m_dataItems["0x02010300"] = DataItem("0x02010300", "C相电压", 220.3, "V", "AA,xx", false, 1);
    m_dataItems["0x0201FF00"] = DataItem("0x02010400", "电压数据块", QVariantList(), "-", "AA,xx:AA,xx:AA,xx", false, 1);

    // 电流
    m_dataItems["0x02020100"] = DataItem("0x02020100", "A相电流", 1.01, "A", "AA,xx,AA", false, 3);
    m_dataItems["0x02020200"] = DataItem("0x02020200", "B相电流", 1.023, "A", "AA,xx,AA", false, 3);
    m_dataItems["0x02020300"] = DataItem("0x02020300", "C相电流", 1.1, "A", "AA,xx,AA", false, 3);
    m_dataItems["0x0202FF00"] = DataItem("0x0202FF00", "电流数据块", QVariantList(), "-", "AA,xx,AA:AA,xx,AA:AA,xx,AA", false, 3);

    // 功率
    m_dataItems["0x02030000"] = DataItem("0x02030000", "瞬时总有功功率", 0.0, "kW", "AA,.,AA,AA", false, 4);
    m_dataItems["0x02030100"] = DataItem("0x02030100", "瞬时A相有功功率", 0.0, "kW", "AA,.,AA,AA", false, 4);
    m_dataItems["0x02030200"] = DataItem("0x02030200", "瞬时B相有功功率", 0.0, "kW", "AA,.,AA,AA", false, 4);
    m_dataItems["0x02030300"] = DataItem("0x02030300", "瞬时C相有功功率", 0.0, "kW", "AA,.,AA,AA", false, 4);
    m_dataItems["0x0203FF00"] = DataItem("0x0203FF00", "瞬时有功功率数据块", QVariantList(), "-", "AA,.,AA,AA:AA,.,AA,AA:AA,.,AA,AA:AA,.,AA,AA", false, 4);

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
    m_dataItems["0x0207FF00"] = DataItem("0x0207FF00", "相角数据块", QVariantList(), "-", "AA,xx:AA,xx:AA,xx:AA,xx", false, 1);

    // 零相电流
    m_dataItems["0x02080001"] = DataItem("0x02080001", "零相电流", 0.0, "A", "AA,xx,AA", false, 3);
    // 频率
    m_dataItems["0x02800002"] = DataItem("0x02800002", "电网频率", 50.0, "Hz", "AA,.,AA", false, 2);
    synchronizeInstantaneousPowerTotals(m_dataItems);
}

void MeterModel::initializeEventData()
{
    // 事件记录 (0x03xxxxxx)
    m_dataItems["0x03300100"] = DataItem("0x03300100", "数据清零总次数", 0, "次", "AA,AA,AA", false, 0);
    m_dataItems["0x03810000"] = DataItem("0x03810000", "总跳闸次数", 0, "次", "AA,AA", false, 0);
    m_dataItems["0x03810001"] = DataItem("0x03810001", "闭锁性跳闸次数", 0, "次", "AA,AA", false, 0);
    m_dataItems["0x03810002"] = DataItem("0x03810002", "剩余电流保护跳闸次数", 0, "次", "AA,AA", false, 0);
    m_dataItems["0x03810003"] = DataItem("0x03810003", "电流保护跳闸次数", 0, "次", "AA,AA", false, 0);
    m_dataItems["0x03810004"] = DataItem("0x03810004", "电压保护跳闸次数", 0, "次", "AA,AA", false, 0);
    m_dataItems["0x03810005"] = DataItem("0x03810005", "手动闭锁跳闸次数", 0, "次", "AA,AA", false, 0);
    m_dataItems["0x03810006"] = DataItem("0x03810006", "缺零保护跳闸次数", 0, "次", "AA,AA", false, 0);
    m_dataItems["0x03810007"] = DataItem("0x03810007", "试验跳闸次数（定时、远程、按键）", 0, "次", "AA,AA", false, 0);
    m_dataItems["0x03810101"] = DataItem("0x03810101", "退出剩余电流保护次数", 0, "次", "AA,AA", false, 0);
    m_dataItems["0x03810201"] = DataItem("0x03810201", "保护器运行时间总累计", 0, "分钟", "AA,AA,AA,AA", false, 0);
}

void MeterModel::initializeParameterData()
{
    // 参变量 (0x04xxxxxx)
    const QDateTime now = QDateTime::currentDateTime();

    m_dataItems["0x04000000"] = DataItem("0x04000000", "电表地址", m_meterAddress, "-", "AA,AA,AA,AA,AA,AA", true, 0);
    m_dataItems["0x04000001"] = DataItem("0x04000001", "波特率特征字(08=2400bps)", 8, "-", "AA", true, 0);
    m_dataItems["0x04000040"] = DataItem("0x04000040", "需量周期", 15, "分钟", "AA", true, 0);
    m_dataItems["0x04000041"] = DataItem("0x04000041", "滑差时间", 1, "分钟", "AA", true, 0);

    m_dataItems["0x04000101"] = DataItem("0x04000101", "当前日期(含星期代码)", compactDateWithWeek(now.date()), "-", "AA,AA,AA,AA", true, 0);
    m_dataItems["0x04000102"] = DataItem("0x04000102", "当前时间", now.time().toString(QStringLiteral("HHmmss")), "-", "AA,AA,AA", true, 0);
    m_dataItems["0x04000106"] = DataItem("0x04000106", "两套时区表切换时间", compactDateTimeToMinutes(now.addDays(30)), "-", "AA,AA,AA,AA,AA", true, 0);
    m_dataItems["0x04000107"] = DataItem("0x04000107", "两套日时段表切换时间", compactDateTimeToMinutes(now.addDays(60)), "-", "AA,AA,AA,AA,AA", true, 0);
    m_dataItems["0x04000201"] = DataItem("0x04000201", "年时区数p", 2, "-", "AA", true, 0);
    m_dataItems["0x04000202"] = DataItem("0x04000202", "日时段表数q", 2, "-", "AA", true, 0);
    m_dataItems["0x04000203"] = DataItem("0x04000203", "日时段数m", 8, "-", "AA", true, 0);
    m_dataItems["0x04000204"] = DataItem("0x04000204", "费率数k", 4, "-", "AA", true, 0);
    m_dataItems["0x04000205"] = DataItem("0x04000205", "公共假日数n", 12, "天", "AA,AA", true, 0);
    m_dataItems["0x04000206"] = DataItem("0x04000206", "谐波分析次数", 1, "次", "AA", true, 0);
    m_dataItems["0x04000401"] = DataItem("0x04000401", "通信地址", m_meterAddress, "-", "AA,AA,AA,AA,AA,AA", true, 0);
    m_dataItems["0x04000402"] = DataItem("0x04000402", "设备号", QStringLiteral("000000000001"), "-", "AA,AA,AA,AA,AA,AA", true, 0);
    m_dataItems["0x04000703"] = DataItem("0x04000703", "波特率代码", 8, "-", "AA", true, 0);
}

void MeterModel::initializeFreezeData()
{
    // 冻结数据 (0x05xxxxxx)
    const QDateTime lastFreeze = QDateTime::currentDateTime().addDays(-1);

    m_dataItems["0x05060001"] = DataItem("0x05060001", "上1次日冻结时间", compactDateTimeToMinutes(lastFreeze), "-", "AA,AA,AA,AA,AA", false, 0);

    m_dataItems["0x05060100"] = DataItem("0x05060100", "上1次日冻结正向有功总电能", 123.45, "kWh", "AA,AA,AA,.,AA", false, 2);
    m_dataItems["0x05060102"] = DataItem("0x05060102", "上1次日冻结正向有功费率1电能", 25.11, "kWh", "AA,AA,AA,.,AA", false, 2);
    m_dataItems["0x05060103"] = DataItem("0x05060103", "上1次日冻结正向有功费率2电能", 24.22, "kWh", "AA,AA,AA,.,AA", false, 2);
    m_dataItems["0x05060104"] = DataItem("0x05060104", "上1次日冻结正向有功费率3电能", 36.33, "kWh", "AA,AA,AA,.,AA", false, 2);
    m_dataItems["0x05060105"] = DataItem("0x05060105", "上1次日冻结正向有功费率4电能", 37.79, "kWh", "AA,AA,AA,.,AA", false, 2);
    m_dataItems["0x05060101"] = DataItem("0x05060101", "上1次日冻结正向有功电能数据块", QVariantList(), "-", "AA,AA,AA,.,AA:AA,AA,AA,.,AA:AA,AA,AA,.,AA:AA,AA,AA,.,AA:AA,AA,AA,.,AA", false, 2);

    m_dataItems["0x05060200"] = DataItem("0x05060200", "上1次日冻结反向有功总电能", 80.15, "kWh", "AA,AA,AA,.,AA", false, 2);
    m_dataItems["0x05060202"] = DataItem("0x05060202", "上1次日冻结反向有功费率1电能", 15.01, "kWh", "AA,AA,AA,.,AA", false, 2);
    m_dataItems["0x05060203"] = DataItem("0x05060203", "上1次日冻结反向有功费率2电能", 18.22, "kWh", "AA,AA,AA,.,AA", false, 2);
    m_dataItems["0x05060204"] = DataItem("0x05060204", "上1次日冻结反向有功费率3电能", 20.33, "kWh", "AA,AA,AA,.,AA", false, 2);
    m_dataItems["0x05060205"] = DataItem("0x05060205", "上1次日冻结反向有功费率4电能", 26.59, "kWh", "AA,AA,AA,.,AA", false, 2);
    m_dataItems["0x05060201"] = DataItem("0x05060201", "上1次日冻结反向有功电能数据块", QVariantList(), "-", "AA,AA,AA,.,AA:AA,AA,AA,.,AA:AA,AA,AA,.,AA:AA,AA,AA,.,AA:AA,AA,AA,.,AA", false, 2);

    m_dataItems["0x05060300"] = DataItem("0x05060300", "上1次日冻结组合无功1总电能", 61.28, "kvarh", "AA,AA,AA,.,AA", false, 2);
    m_dataItems["0x05060302"] = DataItem("0x05060302", "上1次日冻结组合无功1费率1电能", 12.12, "kvarh", "AA,AA,AA,.,AA", false, 2);
    m_dataItems["0x05060303"] = DataItem("0x05060303", "上1次日冻结组合无功1费率2电能", 13.13, "kvarh", "AA,AA,AA,.,AA", false, 2);
    m_dataItems["0x05060304"] = DataItem("0x05060304", "上1次日冻结组合无功1费率3电能", 14.14, "kvarh", "AA,AA,AA,.,AA", false, 2);
    m_dataItems["0x05060305"] = DataItem("0x05060305", "上1次日冻结组合无功1费率4电能", 21.89, "kvarh", "AA,AA,AA,.,AA", false, 2);
    m_dataItems["0x05060301"] = DataItem("0x05060301", "上1次日冻结组合无功1电能数据块", QVariantList(), "-", "AA,AA,AA,.,AA:AA,AA,AA,.,AA:AA,AA,AA,.,AA:AA,AA,AA,.,AA:AA,AA,AA,.,AA", false, 2);

    m_dataItems["0x05060400"] = DataItem("0x05060400", "上1次日冻结组合无功2总电能", 55.67, "kvarh", "AA,AA,AA,.,AA", false, 2);
    m_dataItems["0x05060402"] = DataItem("0x05060402", "上1次日冻结组合无功2费率1电能", 10.1, "kvarh", "AA,AA,AA,.,AA", false, 2);
    m_dataItems["0x05060403"] = DataItem("0x05060403", "上1次日冻结组合无功2费率2电能", 11.2, "kvarh", "AA,AA,AA,.,AA", false, 2);
    m_dataItems["0x05060404"] = DataItem("0x05060404", "上1次日冻结组合无功2费率3电能", 12.3, "kvarh", "AA,AA,AA,.,AA", false, 2);
    m_dataItems["0x05060405"] = DataItem("0x05060405", "上1次日冻结组合无功2费率4电能", 22.07, "kvarh", "AA,AA,AA,.,AA", false, 2);
    m_dataItems["0x05060401"] = DataItem("0x05060401", "上1次日冻结组合无功2电能数据块", QVariantList(), "-", "AA,AA,AA,.,AA:AA,AA,AA,.,AA:AA,AA,AA,.,AA:AA,AA,AA,.,AA:AA,AA,AA,.,AA", false, 2);

    synchronizeDailyFreezeTotals(m_dataItems);
}

void MeterModel::initializeLoadRecordData()
{
    // 负荷记录 (0x06xxxxxx)
    m_dataItems["0x06010101"] = DataItem("0x06010101", "预约远程跳闸控制(数值+单位代码)", QStringLiteral("3002"), "-", "AA,AA", true, 0);
    m_dataItems["0x06020101"] = DataItem("0x06020101", "预约远程合闸控制(数值+单位代码)", QStringLiteral("1502"), "-", "AA,AA", true, 0);
    m_dataItems["0x06030101"] = DataItem("0x06030101", "预约模拟试跳控制(数值+单位代码)", QStringLiteral("0502"), "-", "AA,AA", true, 0);
}

QList<QString> MeterModel::getEnergyDataCodes() const
{
    QMutexLocker locker(&m_mutex);
    QList<QString> codes;
    for (auto it = m_dataItems.constBegin(); it != m_dataItems.constEnd(); ++it)
    {
        if (it.key().startsWith("0x00"))
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
