#ifndef METERMODEL_H
#define METERMODEL_H

#include <QObject>
#include <QList>
#include <QMap>
#include <QMutex>
#include "DataItem.h"
#include "MeterPhaseType.h"

/**
 * @brief 数据分类枚举
 */
enum class DataCategory
{
    Energy,    // 电能量
    Demand,    // 最大需量及发生时间
    Variable,  // 变量
    Event,     // 事件记录
    Parameter, // 参变量
    Freeze,    // 冻结量
    LoadRecord // 负荷记录
};

/**
 * @brief 电表核心模型类
 *
 * 管理电表的所有数据项，提供线程安全的数据访问接口
 */
class MeterModel : public QObject
{
    Q_OBJECT

public:
    explicit MeterModel(QObject *parent = nullptr);
    ~MeterModel();

    /**
     * @brief 设置电表地址
     * @param address 电表地址（12位BCD码）
     */
    void setMeterAddress(const QString &address);

    /**
     * @brief 获取电表地址
     * @return 电表地址
     */
    QString getMeterAddress() const;

    /**
     * @brief 设置操作者代码
     * @param code 操作者代码（8位数字）
     */
    void setOperatorCode(const QString &code);

    /**
     * @brief 获取操作者代码
     * @return 操作者代码
     */
    QString getOperatorCode() const;

    /**
     * @brief 设置密码
     * @param password 密码（8位数字）
     */
    void setPassword(const QString &password);

    /**
     * @brief 获取密码
     * @return 密码
     */
    QString getPassword() const;

    /**
     * @brief 设置是否包含前导FE
     * @param include 是否包含
     */
    void setIncludePrefixFE(bool include);

    /**
     * @brief 获取是否包含前导FE
     * @return 是否包含
     */
    bool getIncludePrefixFE() const;
    void setMeterPhaseType(MeterPhaseType meterPhaseType);
    MeterPhaseType getMeterPhaseType() const;
    bool isSinglePhase() const;

    /**
     * @brief 设置事件记录是否存在
     * @param present 为 false 时，读取事件类数据项（0x03 族）返回异常应答（无请求数据）
     */
    void setEventRecordPresent(bool present);

    /**
     * @brief 获取事件记录是否存在
     * @return 事件记录是否存在
     */
    bool getEventRecordPresent() const;

    /**
     * @brief 获取数据项
     * @param diCode 数据标识
     * @return 数据项（若电表不支持该数据项，返回空）
     */
    DataItem getDataItem(const QString &diCode) const;

    /**
     * @brief 获取原始数据项（不经过支持性过滤）
     * @param diCode 数据标识
     * @return 数据项（即使电表不支持也返回，用于界面显示所有数据项）
     */
    DataItem getDataItemRaw(const QString &diCode) const;

    /**
     * @brief 获取数据值
     * @param diCode 数据标识
     * @return 数据值
     */
    QVariant getDataValue(const QString &diCode) const;
    bool isDataItemSupported(const QString &diCode) const;

    /**
     * @brief 设置数据值
     * @param diCode 数据标识
     * @param value 数据值
     * @return 是否成功
     */
    bool setDataValue(const QString &diCode, const QVariant &value);
    void applyPersistedData(const QMap<QString, QVariant> &dataSnapshot);

    /**
     * @brief 获取指定分类的所有数据项
     * @param category 数据分类
     * @return 数据项列表
     */
    QList<DataItem> getDataItemsByCategory(DataCategory category) const;

    /**
     * @brief 获取所有数据项
     * @return 数据项列表
     */
    QList<DataItem> getAllDataItems() const;

    /**
     * @brief 获取电能计量数据标识列表
     * @return 数据标识列表
     */
    QList<QString> getEnergyDataCodes() const;
    QList<QString> getSettlementDayEnergyDataCodes(int settlementDay) const;

    /**
     * @brief 获取需量计量数据标识列表
     * @return 数据标识列表
     */
    QList<QString> getDemandDataCodes() const;
    QList<QString> getSettlementDayDemandDataCodes(int settlementDay) const;

    /**
     * @brief 获取最大需量数据标识列表
     * @return 数据标识列表
     */
    QList<QString> getMaxDemandDataCodes() const;

    /**
     * @brief 获取变量数据标识列表
     * @return 数据标识列表
     */
    QList<QString> getVariableDataCodes() const;

    /**
     * @brief 获取事件数据标识列表
     * @return 数据标识列表
     */
    QList<QString> getEventDataCodes() const;

    /**
     * @brief 获取参数数据标识列表
     * @return 数据标识列表
     */
    QList<QString> getParameterDataCodes() const;

    /**
     * @brief 获取冻结数据标识列表
     * @return 数据标识列表
     */
    QList<QString> getFreezeDataCodes() const;
    QList<QString> getFreezeDataCodes(const QString &freezeCategory) const;

    /**
     * @brief 获取负荷记录数据标识列表
     * @return 数据标识列表
     */
    QList<QString> getLoadRecordDataCodes() const;

    /**
     * @brief 获取状态数据标识列表
     * @return 数据标识列表
     */
    QList<QString> getStatusDataCodes() const;

    /**
     * @brief 获取数据名称
     * @param diCode 数据标识
     * @return 数据名称
     */
    QString getDataName(const QString &diCode) const;

    /**
     * @brief 获取数据单位
     * @param diCode 数据标识
     * @return 数据单位
     */
    QString getDataUnit(const QString &diCode) const;

    /**
     * @brief 重置所有数据为默认值
     */
    void resetAllData();

    /**
     * @brief 随机化所有数据
     */
    void randomizeData();
    void randomizeData(const QList<QString> &diCodes);

    /**
     * @brief 初始化默认数据项
     */
    void initializeDefaultDataItems();

signals:
    /**
     * @brief 数据变更信号
     * @param diCode 数据标识
     * @param newValue 新值
     */
    void dataChanged(const QString &diCode, const QVariant &newValue);
    void dataBatchChanged();

    /**
     * @brief 电表地址变更信号
     * @param newAddress 新地址
     */
    void addressChanged(const QString &newAddress);

private:
    bool isDataItemSupportedUnlocked(const QString &diCode) const;
    void initializeEnergyData();
    void initializeDemandData();
    void initializeVariableData();
    void initializeEventData();
    void initializeParameterData();
    void initializeFreezeData();
    void initializeLoadRecordData();

    mutable QMutex m_mutex;
    QString m_meterAddress;
    QString m_operatorCode;
    QString m_password;
    bool m_includePrefixFE;
    bool m_eventRecordPresent;
    MeterPhaseType m_meterPhaseType;
    QMap<QString, DataItem> m_dataItems;
};

#endif // METERMODEL_H
