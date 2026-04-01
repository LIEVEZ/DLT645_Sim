#ifndef DATAGENERATOR_H
#define DATAGENERATOR_H

#include <QObject>
#include <QTimer>
#include <QRandomGenerator>
#include "Models/MeterModel.h"

/**
 * @brief 数据生成服务类
 *
 * 负责生成模拟电表数据，支持自动刷新和随机波动
 */
class DataGenerator : public QObject
{
    Q_OBJECT

public:
    explicit DataGenerator(QObject *parent = nullptr);
    ~DataGenerator();

    /**
     * @brief 启动数据生成
     * @param interval 刷新间隔（毫秒）
     */
    void start(int interval = 1000);

    /**
     * @brief 停止数据生成
     */
    void stop();

    /**
     * @brief 设置是否启用随机波动
     * @param enabled 是否启用
     */
    void setRandomFluctuationEnabled(bool enabled);

    /**
     * @brief 设置电压波动范围
     * @param range 波动范围（伏特）
     */
    void setVoltageFluctuationRange(double range);

    /**
     * @brief 设置电流波动范围
     * @param range 波动范围（安培）
     */
    void setCurrentFluctuationRange(double range);

    /**
     * @brief 手动触发一次数据更新
     */
    void triggerUpdate();

signals:
    /**
     * @brief 数据更新信号
     */
    void dataUpdated();

private slots:
    void onTimerTimeout();

private:
    void updateVariableData();
    void updateEnergyData();
    double generateRandomValue(double baseValue, double range);

    MeterModel *m_meterModel;
    QTimer *m_timer;
    QRandomGenerator *m_randomGenerator;
    bool m_randomFluctuationEnabled;
    double m_voltageFluctuationRange;
    double m_currentFluctuationRange;
};

#endif // DATAGENERATOR_H
