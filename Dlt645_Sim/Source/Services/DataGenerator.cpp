#include "DataGenerator.h"
#include <QDateTime>

DataGenerator::DataGenerator(QObject *parent)
    : QObject(parent), m_meterModel(new MeterModel(this)), m_timer(new QTimer(this)), m_randomGenerator(QRandomGenerator::global()), m_randomFluctuationEnabled(true), m_voltageFluctuationRange(2.0), m_currentFluctuationRange(0.1)
{
    connect(m_timer, &QTimer::timeout, this, &DataGenerator::onTimerTimeout);
}

DataGenerator::~DataGenerator()
{
    stop();
}

void DataGenerator::start(int interval)
{
    m_timer->start(interval);
}

void DataGenerator::stop()
{
    m_timer->stop();
}

void DataGenerator::setRandomFluctuationEnabled(bool enabled)
{
    m_randomFluctuationEnabled = enabled;
}

void DataGenerator::setVoltageFluctuationRange(double range)
{
    m_voltageFluctuationRange = range;
}

void DataGenerator::setCurrentFluctuationRange(double range)
{
    m_currentFluctuationRange = range;
}

void DataGenerator::triggerUpdate()
{
    onTimerTimeout();
}

void DataGenerator::onTimerTimeout()
{
    updateVariableData();
    updateEnergyData();
    emit dataUpdated();
}

void DataGenerator::updateVariableData()
{
    if (!m_meterModel)
    {
        return;
    }

    // 更新电压数据
    QStringList voltageCodes = {"0x02010100", "0x02010200", "0x02010300"};
    for (const QString &code : voltageCodes)
    {
        double currentValue = m_meterModel->getDataValue(code).toDouble();
        double newValue = generateRandomValue(currentValue, m_voltageFluctuationRange);
        // 限制在合理范围内（220V ± 10%）
        if (newValue < 198.0)
            newValue = 198.0;
        if (newValue > 242.0)
            newValue = 242.0;
        m_meterModel->setDataValue(code, newValue);
    }

    // 更新电流数据
    QStringList currentCodes = {"0x02020100", "0x02020200", "0x02020300", "0x02800001"};
    for (const QString &code : currentCodes)
    {
        double currentValue = m_meterModel->getDataValue(code).toDouble();
        double newValue = generateRandomValue(currentValue, m_currentFluctuationRange);
        // 限制在合理范围内（0-10A）
        if (newValue < 0.0)
            newValue = 0.0;
        if (newValue > 10.0)
            newValue = 10.0;
        m_meterModel->setDataValue(code, newValue);
    }

    // 更新功率数据（根据电压和电流计算）
    double voltageA = m_meterModel->getDataValue("0x02010100").toDouble();
    double currentA = m_meterModel->getDataValue("0x02020100").toDouble();
    double powerA = (voltageA * currentA) / 1000.0; // kW
    m_meterModel->setDataValue("0x02030100", powerA);

    // 总有功功率 = 三相功率之和
    double voltageB = m_meterModel->getDataValue("0x02010200").toDouble();
    double voltageC = m_meterModel->getDataValue("0x02010300").toDouble();
    double currentB = m_meterModel->getDataValue("0x02020200").toDouble();
    double currentC = m_meterModel->getDataValue("0x02020300").toDouble();
    double totalPower = (voltageA * currentA + voltageB * currentB + voltageC * currentC) / 1000.0;
    m_meterModel->setDataValue("0x02030000", totalPower);

    // 更新功率因数（随机波动）
    double pf = m_meterModel->getDataValue("0x02060000").toDouble();
    pf = generateRandomValue(pf, 0.05);
    if (pf < -1.0)
        pf = -1.0;
    if (pf > 1.0)
        pf = 1.0;
    m_meterModel->setDataValue("0x02060000", pf);

    // 更新相角
    double angle = m_meterModel->getDataValue("0x02070100").toDouble();
    angle = generateRandomValue(angle, 5.0);
    if (angle < 0.0)
        angle += 360.0;
    if (angle >= 360.0)
        angle -= 360.0;
    m_meterModel->setDataValue("0x02070100", angle);

    // 更新频率
    double freq = m_meterModel->getDataValue("0x02800002").toDouble();
    freq = generateRandomValue(freq, 0.5);
    if (freq < 45.0)
        freq = 45.0;
    if (freq > 55.0)
        freq = 55.0;
    m_meterModel->setDataValue("0x02800002", freq);

    // 更新温度
    double temp = m_meterModel->getDataValue("0x02800007").toDouble();
    temp = generateRandomValue(temp, 0.5);
    if (temp < -40.0)
        temp = -40.0;
    if (temp > 85.0)
        temp = 85.0;
    m_meterModel->setDataValue("0x02800007", temp);
}

void DataGenerator::updateEnergyData()
{
    if (!m_meterModel)
    {
        return;
    }

    // 电能量数据累加
    QStringList energyCodes = {
        "0x00000000", "0x00000100", "0x00000200", "0x00000300", "0x00000400",
        "0x00000001", "0x00000002", "0x00000010", "0x00000020", "0x00000030", "0x00000040"};

    for (const QString &code : energyCodes)
    {
        double currentValue = m_meterModel->getDataValue(code).toDouble();
        // 根据功率计算增量（假设每秒增量）
        double increment = m_randomGenerator->bounded(0, 100) / 10000.0; // 0-0.01 kWh
        double newValue = currentValue + increment;
        m_meterModel->setDataValue(code, newValue);
    }
}

double DataGenerator::generateRandomValue(double baseValue, double range)
{
    if (!m_randomFluctuationEnabled)
    {
        return baseValue;
    }

    double offset = m_randomGenerator->bounded(-1000, 1001) / 1000.0; // -1.0 ~ 1.0
    return baseValue + (offset * range);
}
