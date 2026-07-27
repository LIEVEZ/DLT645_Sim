#include "Models/StatusWordBitCatalog.h"

#include <QRegularExpression>

namespace {

StatusWordBitField makeField(int offset, int width, const QString &name,
                             const QMap<int, QString> &meanings, bool reserved = false)
{
    StatusWordBitField field;
    field.bitOffset = offset;
    field.bitWidth = width;
    field.name = name;
    field.meanings = meanings;
    field.reserved = reserved;
    return field;
}

StatusWordBitField reservedField(int offset, int width)
{
    return makeField(offset, width, QStringLiteral("保留"), QMap<int, QString>(), true);
}

StatusWordBitField binaryField(int offset, const QString &name, const QString &zeroText, const QString &oneText)
{
    QMap<int, QString> meanings;
    meanings.insert(0, zeroText);
    meanings.insert(1, oneText);
    return makeField(offset, 1, name, meanings);
}

StatusWordBitField faultField(int offset, const QString &name)
{
    return binaryField(offset, name, QStringLiteral("无此类故障"), QStringLiteral("当前发生此类故障"));
}

QVector<StatusWordBitField> phaseFaultFields()
{
    QVector<StatusWordBitField> fields;
    fields << faultField(0, QStringLiteral("失压"));
    fields << faultField(1, QStringLiteral("欠压"));
    fields << faultField(2, QStringLiteral("过压"));
    fields << faultField(3, QStringLiteral("失流"));
    fields << faultField(4, QStringLiteral("过流"));
    fields << faultField(5, QStringLiteral("过载"));
    fields << faultField(6, QStringLiteral("潮流反向"));
    fields << faultField(7, QStringLiteral("断相"));
    fields << faultField(8, QStringLiteral("断流"));
    fields << reservedField(9, 7);
    return fields;
}

QStringList phaseFaultNotes()
{
    return QStringList() << QStringLiteral("0 代表无此类故障，1 代表当前发生此类故障");
}

StatusWordDefinition buildStatusWordDefinition(int index)
{
    StatusWordDefinition definition;
    definition.index = index;
    definition.byteWidth = 2;
    definition.diCode = QStringLiteral("0x040005%1").arg(index, 2, 16, QChar('0')).toUpper().replace(QStringLiteral("0X"), QStringLiteral("0x"));
    definition.title = QStringLiteral("电表运行状态字%1").arg(index);

    switch (index) {
    case 1: {
        definition.subtitle = QStringLiteral("运行配置");
        definition.fields
            << binaryField(0, QStringLiteral("外置开关控制方式"), QStringLiteral("电平"), QStringLiteral("脉冲"))
            << binaryField(1, QStringLiteral("需量积算方式"), QStringLiteral("滑差"), QStringLiteral("区间"))
            << binaryField(2, QStringLiteral("时钟电池"), QStringLiteral("正常"), QStringLiteral("欠压"))
            << binaryField(3, QStringLiteral("停电抄表电池"), QStringLiteral("正常"), QStringLiteral("欠压"))
            << binaryField(4, QStringLiteral("有功功率方向"), QStringLiteral("正向"), QStringLiteral("反向"))
            << binaryField(5, QStringLiteral("无功功率方向"), QStringLiteral("正向"), QStringLiteral("反向"))
            << reservedField(6, 1)
            << reservedField(7, 1)
            << reservedField(8, 8);
        break;
    }
    case 2: {
        definition.subtitle = QStringLiteral("功率方向");
        definition.notes << QStringLiteral("0 代表正向，1 代表反向");
        definition.fields
            << binaryField(0, QStringLiteral("A相有功功率方向"), QStringLiteral("正向"), QStringLiteral("反向"))
            << binaryField(1, QStringLiteral("B相有功功率方向"), QStringLiteral("正向"), QStringLiteral("反向"))
            << binaryField(2, QStringLiteral("C相有功功率方向"), QStringLiteral("正向"), QStringLiteral("反向"))
            << reservedField(3, 1)
            << binaryField(4, QStringLiteral("A相无功功率方向"), QStringLiteral("正向"), QStringLiteral("反向"))
            << binaryField(5, QStringLiteral("B相无功功率方向"), QStringLiteral("正向"), QStringLiteral("反向"))
            << binaryField(6, QStringLiteral("C相无功功率方向"), QStringLiteral("正向"), QStringLiteral("反向"))
            << reservedField(7, 1)
            << reservedField(8, 8);
        break;
    }
    case 3: {
        definition.subtitle = QStringLiteral("操作类");
        definition.notes
            << QStringLiteral("注1：编程允许一般指编程按键状态")
            << QStringLiteral("注2：预跳闸报警状态是指剩余电量或剩余金额到达报警电量或报警金额时电表发出报警提示时的状态，该状态位置 1，提示用户需购电或交费")
            << QStringLiteral("注3：电能表类型有非预付费型、电量型预付费和电费型预付费三种。00 为非预付费型，01 为电量型预付费，10 为电费型预付费");

        QMap<int, QString> powerMode;
        powerMode.insert(0, QStringLiteral("主电源"));
        powerMode.insert(1, QStringLiteral("辅助电源"));
        powerMode.insert(2, QStringLiteral("电池供电"));

        QMap<int, QString> meterType;
        meterType.insert(0, QStringLiteral("非预付费表"));
        meterType.insert(1, QStringLiteral("电量型预付费表"));
        meterType.insert(2, QStringLiteral("电费型预付费表"));

        definition.fields
            << binaryField(0, QStringLiteral("当前运行时段"), QStringLiteral("第一套"), QStringLiteral("第二套"))
            << makeField(1, 2, QStringLiteral("供电方式"), powerMode)
            << binaryField(3, QStringLiteral("编程允许"), QStringLiteral("禁止"), QStringLiteral("许可"))
            << binaryField(4, QStringLiteral("继电器状态"), QStringLiteral("通"), QStringLiteral("断"))
            << binaryField(5, QStringLiteral("当前运行时区"), QStringLiteral("第一套"), QStringLiteral("第二套"))
            << binaryField(6, QStringLiteral("继电器命令状态"), QStringLiteral("通"), QStringLiteral("断"))
            << binaryField(7, QStringLiteral("预跳闸报警状态"), QStringLiteral("无"), QStringLiteral("有"))
            << makeField(8, 2, QStringLiteral("电能表类型"), meterType)
            << binaryField(10, QStringLiteral("当前运行费率电价"), QStringLiteral("第一套"), QStringLiteral("第二套"))
            << binaryField(11, QStringLiteral("当前阶梯"), QStringLiteral("第一套"), QStringLiteral("第二套"))
            << reservedField(12, 1)
            << reservedField(13, 1)
            << reservedField(14, 1)
            << reservedField(15, 1);
        break;
    }
    case 4:
        definition.subtitle = QStringLiteral("A相故障状态");
        definition.notes = phaseFaultNotes();
        definition.fields = phaseFaultFields();
        break;
    case 5:
        definition.subtitle = QStringLiteral("B相故障状态");
        definition.notes = phaseFaultNotes();
        definition.fields = phaseFaultFields();
        break;
    case 6:
        definition.subtitle = QStringLiteral("C相故障状态");
        definition.notes = phaseFaultNotes();
        definition.fields = phaseFaultFields();
        break;
    case 7: {
        definition.subtitle = QStringLiteral("合相故障状态");
        definition.notes = phaseFaultNotes();
        definition.fields
            << faultField(0, QStringLiteral("电压逆相序"))
            << faultField(1, QStringLiteral("电流逆相序"))
            << faultField(2, QStringLiteral("电压不平衡"))
            << faultField(3, QStringLiteral("电流不平衡"))
            << faultField(4, QStringLiteral("辅助电源失电"))
            << faultField(5, QStringLiteral("掉电"))
            << faultField(6, QStringLiteral("需量超限"))
            << faultField(7, QStringLiteral("总功率因数超下限"))
            << faultField(8, QStringLiteral("电流严重不平衡"))
            << reservedField(9, 7);
        break;
    }
    default:
        definition = StatusWordDefinition();
        break;
    }

    return definition;
}

StatusWordDefinition buildActiveCombinationFeatureWord()
{
    StatusWordDefinition definition;
    definition.index = 0;
    definition.byteWidth = 1;
    definition.diCode = QStringLiteral("0x04000601");
    definition.title = QStringLiteral("有功组合方式特征字");
    definition.subtitle = QStringLiteral("有功组合");
    definition.notes
        << QStringLiteral("用于配置组合有功电能统计时，正/反向有功的加、减参与方式")
        << QStringLiteral("Bit0/Bit1 控制正向有功是否参与加/减；Bit2/Bit3 控制反向有功是否参与加/减");

    definition.fields
        << binaryField(0, QStringLiteral("正向有功"), QStringLiteral("不加"), QStringLiteral("加"))
        << binaryField(1, QStringLiteral("正向有功"), QStringLiteral("不减"), QStringLiteral("减"))
        << binaryField(2, QStringLiteral("反向有功"), QStringLiteral("不加"), QStringLiteral("加"))
        << binaryField(3, QStringLiteral("反向有功"), QStringLiteral("不减"), QStringLiteral("减"))
        << reservedField(4, 4);

    return definition;
}

StatusWordDefinition buildReactiveCombinationFeatureWord(int modeIndex)
{
    StatusWordDefinition definition;
    definition.index = 0;
    definition.byteWidth = 1;
    definition.diCode = QStringLiteral("0x0400060%1").arg(modeIndex + 1);
    definition.title = QStringLiteral("无功组合方式%1特征字").arg(modeIndex);
    definition.subtitle = QStringLiteral("无功组合");
    definition.notes
        << QStringLiteral("用于配置组合无功电能统计时，I~IV 象限无功的加、减参与方式")
        << QStringLiteral("每个象限占 2 bit：低位控制是否加，高位控制是否减")
        << QStringLiteral("无功组合方式1、2 位布局相同，分别对应不同组合口径");

    definition.fields
        << binaryField(0, QStringLiteral("I 象限"), QStringLiteral("不加"), QStringLiteral("加"))
        << binaryField(1, QStringLiteral("I 象限"), QStringLiteral("不减"), QStringLiteral("减"))
        << binaryField(2, QStringLiteral("II 象限"), QStringLiteral("不加"), QStringLiteral("加"))
        << binaryField(3, QStringLiteral("II 象限"), QStringLiteral("不减"), QStringLiteral("减"))
        << binaryField(4, QStringLiteral("III 象限"), QStringLiteral("不加"), QStringLiteral("加"))
        << binaryField(5, QStringLiteral("III 象限"), QStringLiteral("不减"), QStringLiteral("减"))
        << binaryField(6, QStringLiteral("IV 象限"), QStringLiteral("不加"), QStringLiteral("加"))
        << binaryField(7, QStringLiteral("IV 象限"), QStringLiteral("不减"), QStringLiteral("减"));

    return definition;
}

StatusWordDefinition buildWeekRestFeatureWord()
{
    StatusWordDefinition definition;
    definition.index = 0;
    definition.byteWidth = 1;
    definition.diCode = QStringLiteral("0x04000801");
    definition.title = QStringLiteral("周休日特征字");
    definition.subtitle = QStringLiteral("周休日");
    definition.notes
        << QStringLiteral("0 代表休息，1 代表工作")
        << QStringLiteral("Bit0~Bit6 依次对应周日到周六，Bit7 保留");

    definition.fields
        << binaryField(0, QStringLiteral("周日"), QStringLiteral("休息"), QStringLiteral("工作"))
        << binaryField(1, QStringLiteral("周一"), QStringLiteral("休息"), QStringLiteral("工作"))
        << binaryField(2, QStringLiteral("周二"), QStringLiteral("休息"), QStringLiteral("工作"))
        << binaryField(3, QStringLiteral("周三"), QStringLiteral("休息"), QStringLiteral("工作"))
        << binaryField(4, QStringLiteral("周四"), QStringLiteral("休息"), QStringLiteral("工作"))
        << binaryField(5, QStringLiteral("周五"), QStringLiteral("休息"), QStringLiteral("工作"))
        << binaryField(6, QStringLiteral("周六"), QStringLiteral("休息"), QStringLiteral("工作"))
        << reservedField(7, 1);

    return definition;
}

StatusWordDefinition buildCommRateFeatureWord(const QString &diCode, const QString &title)
{
    StatusWordDefinition definition;
    definition.index = 0;
    definition.byteWidth = 1;
    definition.diCode = diCode;
    definition.title = title;
    definition.subtitle = QStringLiteral("通信速率");
    definition.notes
        << QStringLiteral("0 代表非当前接口通信速率，1 代表当前接口通信速率")
        << QStringLiteral("特征字仅在某一位为 1 时有效");

    definition.fields
        << reservedField(0, 1)
        << binaryField(1, QStringLiteral("600bps"), QStringLiteral("非当前速率"), QStringLiteral("当前速率"))
        << binaryField(2, QStringLiteral("1200bps"), QStringLiteral("非当前速率"), QStringLiteral("当前速率"))
        << binaryField(3, QStringLiteral("2400bps"), QStringLiteral("非当前速率"), QStringLiteral("当前速率"))
        << binaryField(4, QStringLiteral("4800bps"), QStringLiteral("非当前速率"), QStringLiteral("当前速率"))
        << binaryField(5, QStringLiteral("9600bps"), QStringLiteral("非当前速率"), QStringLiteral("当前速率"))
        << binaryField(6, QStringLiteral("19200bps"), QStringLiteral("非当前速率"), QStringLiteral("当前速率"))
        << reservedField(7, 1);

    return definition;
}

StatusWordDefinition buildCommRateFeatureWordByCode(const QString &normalizedDiCode)
{
    if (normalizedDiCode == QStringLiteral("0X04000701")) {
        return buildCommRateFeatureWord(QStringLiteral("0x04000701"), QStringLiteral("调制型红外光口通信速率特征字"));
    }
    if (normalizedDiCode == QStringLiteral("0X04000702")) {
        return buildCommRateFeatureWord(QStringLiteral("0x04000702"), QStringLiteral("接触式红外光口通信速率特征字"));
    }
    if (normalizedDiCode == QStringLiteral("0X04000703")) {
        return buildCommRateFeatureWord(QStringLiteral("0x04000703"), QStringLiteral("通信口1通信速率特征字"));
    }
    if (normalizedDiCode == QStringLiteral("0X04000704")) {
        return buildCommRateFeatureWord(QStringLiteral("0x04000704"), QStringLiteral("通信口2通信速率特征字"));
    }
    if (normalizedDiCode == QStringLiteral("0X04000705")) {
        return buildCommRateFeatureWord(QStringLiteral("0x04000705"), QStringLiteral("通信口3通信速率特征字"));
    }
    return StatusWordDefinition();
}

StatusWordDefinition buildCommRateFeatureWordByDescription(const QString &description)
{
    if (description.contains(QStringLiteral("调制型红外光口通信速率特征字"))) {
        return buildCommRateFeatureWord(QStringLiteral("0x04000701"), QStringLiteral("调制型红外光口通信速率特征字"));
    }
    if (description.contains(QStringLiteral("接触式红外光口通信速率特征字"))) {
        return buildCommRateFeatureWord(QStringLiteral("0x04000702"), QStringLiteral("接触式红外光口通信速率特征字"));
    }
    if (description.contains(QStringLiteral("通信口1通信速率特征字"))) {
        return buildCommRateFeatureWord(QStringLiteral("0x04000703"), QStringLiteral("通信口1通信速率特征字"));
    }
    if (description.contains(QStringLiteral("通信口2通信速率特征字"))) {
        return buildCommRateFeatureWord(QStringLiteral("0x04000704"), QStringLiteral("通信口2通信速率特征字"));
    }
    if (description.contains(QStringLiteral("通信口3通信速率特征字"))) {
        return buildCommRateFeatureWord(QStringLiteral("0x04000705"), QStringLiteral("通信口3通信速率特征字"));
    }
    // 兼容统称
    if (description.contains(QStringLiteral("通信速率特征字")) || description.contains(QStringLiteral("速率特征字"))) {
        return buildCommRateFeatureWord(QStringLiteral("0x04000703"), QStringLiteral("通信速率特征字"));
    }
    return StatusWordDefinition();
}

StatusWordDefinition buildLoadRecordModeWord()
{
    StatusWordDefinition definition;
    definition.index = 0;
    definition.byteWidth = 1;
    definition.diCode = QStringLiteral("0x04000901");
    definition.title = QStringLiteral("负荷记录模式字");
    definition.subtitle = QStringLiteral("负荷记录");
    definition.notes
        << QStringLiteral("0 代表不记录此类数据，1 代表记录此类数据")
        << QStringLiteral("用于配置负荷记录通道中需要采集的数据类型集合");

    definition.fields
        << binaryField(0, QStringLiteral("电压、电流、频率"), QStringLiteral("不记录"), QStringLiteral("记录"))
        << binaryField(1, QStringLiteral("有、无功功率"), QStringLiteral("不记录"), QStringLiteral("记录"))
        << binaryField(2, QStringLiteral("功率因数"), QStringLiteral("不记录"), QStringLiteral("记录"))
        << binaryField(3, QStringLiteral("有、无功总电能"), QStringLiteral("不记录"), QStringLiteral("记录"))
        << binaryField(4, QStringLiteral("四象限无功总电能"), QStringLiteral("不记录"), QStringLiteral("记录"))
        << binaryField(5, QStringLiteral("当前需量"), QStringLiteral("不记录"), QStringLiteral("记录"))
        << reservedField(6, 1)
        << reservedField(7, 1);

    return definition;
}

StatusWordDefinition buildFreezeModeWord(const QString &diCode, const QString &title)
{
    StatusWordDefinition definition;
    definition.index = 0;
    definition.byteWidth = 1;
    definition.diCode = diCode;
    definition.title = title;
    definition.subtitle = QStringLiteral("冻结模式");
    definition.notes
        << QStringLiteral("0 代表不记录此类数据，1 代表记录此类数据")
        << QStringLiteral("用于配置对应冻结类型需要保存的数据通道");

    definition.fields
        << binaryField(0, QStringLiteral("正向有功电能"), QStringLiteral("不记录"), QStringLiteral("记录"))
        << binaryField(1, QStringLiteral("反向有功电能"), QStringLiteral("不记录"), QStringLiteral("记录"))
        << binaryField(2, QStringLiteral("组合无功1电能"), QStringLiteral("不记录"), QStringLiteral("记录"))
        << binaryField(3, QStringLiteral("组合无功2电能"), QStringLiteral("不记录"), QStringLiteral("记录"))
        << binaryField(4, QStringLiteral("四象限无功电能"), QStringLiteral("不记录"), QStringLiteral("记录"))
        << binaryField(5, QStringLiteral("正向有功最大需量及发生时间"), QStringLiteral("不记录"), QStringLiteral("记录"))
        << binaryField(6, QStringLiteral("反向有功最大需量及发生时间"), QStringLiteral("不记录"), QStringLiteral("记录"))
        << binaryField(7, QStringLiteral("变量"), QStringLiteral("不记录"), QStringLiteral("记录"));

    return definition;
}

StatusWordDefinition buildFreezeModeWordByCode(const QString &normalizedDiCode)
{
    if (normalizedDiCode == QStringLiteral("0X04000902")) {
        return buildFreezeModeWord(QStringLiteral("0x04000902"), QStringLiteral("定时冻结数据模式字"));
    }
    if (normalizedDiCode == QStringLiteral("0X04000903")) {
        return buildFreezeModeWord(QStringLiteral("0x04000903"), QStringLiteral("瞬时冻结数据模式字"));
    }
    if (normalizedDiCode == QStringLiteral("0X04000904")) {
        return buildFreezeModeWord(QStringLiteral("0x04000904"), QStringLiteral("约定冻结数据模式字"));
    }
    if (normalizedDiCode == QStringLiteral("0X04000905")) {
        return buildFreezeModeWord(QStringLiteral("0x04000905"), QStringLiteral("整点冻结数据模式字"));
    }
    if (normalizedDiCode == QStringLiteral("0X04000906")) {
        return buildFreezeModeWord(QStringLiteral("0x04000906"), QStringLiteral("日冻结数据模式字"));
    }
    return StatusWordDefinition();
}

StatusWordDefinition buildFreezeModeWordByDescription(const QString &description)
{
    if (description.contains(QStringLiteral("定时冻结数据模式字"))) {
        return buildFreezeModeWord(QStringLiteral("0x04000902"), QStringLiteral("定时冻结数据模式字"));
    }
    if (description.contains(QStringLiteral("瞬时冻结数据模式字"))) {
        return buildFreezeModeWord(QStringLiteral("0x04000903"), QStringLiteral("瞬时冻结数据模式字"));
    }
    if (description.contains(QStringLiteral("约定冻结数据模式字"))) {
        return buildFreezeModeWord(QStringLiteral("0x04000904"), QStringLiteral("约定冻结数据模式字"));
    }
    if (description.contains(QStringLiteral("整点冻结数据模式字"))) {
        return buildFreezeModeWord(QStringLiteral("0x04000905"), QStringLiteral("整点冻结数据模式字"));
    }
    if (description.contains(QStringLiteral("日冻结数据模式字"))) {
        return buildFreezeModeWord(QStringLiteral("0x04000906"), QStringLiteral("日冻结数据模式字"));
    }
    // 兼容“冻结数据模式字”统称（默认定时冻结）
    if (description.contains(QStringLiteral("冻结数据模式字")) || (description.contains(QStringLiteral("冻结")) && description.contains(QStringLiteral("模式字")))) {
        return buildFreezeModeWord(QStringLiteral("0x04000902"), QStringLiteral("冻结数据模式字"));
    }
    return StatusWordDefinition();
}

StatusWordDefinition buildSecurityAuthErrorWord()
{
    StatusWordDefinition definition;
    definition.index = 0;
    definition.byteWidth = 2;
    definition.diCode = QStringLiteral("0x04000C01");
    definition.title = QStringLiteral("安全认证错误信息字");
    definition.subtitle = QStringLiteral("SERR");
    definition.notes
        << QStringLiteral("注1：0 代表无相应错误发生，1 代表相应错误发生")
        << QStringLiteral("注2：当表内已充值次数比充值指令中充值次数小 1 时，为正常充值；充值完成后表内已充值次数加 1。当两者相等时为重复充值；其余情况为充值次数错误")
        << QStringLiteral("注3：身份认证失败和身份认证超时统称为身份认证失败，需要重新进行身份认证");

    definition.fields
        << binaryField(0, QStringLiteral("其它错误"), QStringLiteral("无错误"), QStringLiteral("发生错误"))
        << binaryField(1, QStringLiteral("重复充值"), QStringLiteral("无错误"), QStringLiteral("发生错误"))
        << binaryField(2, QStringLiteral("ESAM 验证失败"), QStringLiteral("无错误"), QStringLiteral("发生错误"))
        << binaryField(3, QStringLiteral("身份认证失败"), QStringLiteral("无错误"), QStringLiteral("发生错误"))
        << binaryField(4, QStringLiteral("客户编号不匹配"), QStringLiteral("无错误"), QStringLiteral("发生错误"))
        << binaryField(5, QStringLiteral("充值次数错误"), QStringLiteral("无错误"), QStringLiteral("发生错误"))
        << binaryField(6, QStringLiteral("购电超囤积"), QStringLiteral("无错误"), QStringLiteral("发生错误"))
        << reservedField(7, 1)
        << reservedField(8, 8);

    return definition;
}

StatusWordDefinition buildProtocolErrorWord()
{
    StatusWordDefinition definition;
    definition.index = 0;
    definition.byteWidth = 1;
    definition.diCode = QString();
    definition.title = QStringLiteral("错误信息字");
    definition.subtitle = QStringLiteral("ERR");
    definition.notes
        << QStringLiteral("0 代表无相应错误发生，1 代表相应错误发生")
        << QStringLiteral("除 Bit1~Bit6 定义的错误以外，其他情况都归为 Bit0 其他错误")
        << QStringLiteral("出现在控制码异常应答（bit6=1）时的 1 字节错误信息字中");

    definition.fields
        << binaryField(0, QStringLiteral("其他错误"), QStringLiteral("无错误"), QStringLiteral("发生错误"))
        << binaryField(1, QStringLiteral("无请求数据"), QStringLiteral("无错误"), QStringLiteral("发生错误"))
        << binaryField(2, QStringLiteral("密码错/未授权"), QStringLiteral("无错误"), QStringLiteral("发生错误"))
        << binaryField(3, QStringLiteral("通信速率不能更改"), QStringLiteral("无错误"), QStringLiteral("发生错误"))
        << binaryField(4, QStringLiteral("年时区数超"), QStringLiteral("无错误"), QStringLiteral("发生错误"))
        << binaryField(5, QStringLiteral("日时段数超"), QStringLiteral("无错误"), QStringLiteral("发生错误"))
        << binaryField(6, QStringLiteral("费率数超"), QStringLiteral("无错误"), QStringLiteral("发生错误"))
        << reservedField(7, 1);

    return definition;
}

int parseStatusWordIndex(const QString &text)
{
    static const QRegularExpression re(QStringLiteral("电表运行状态字\\s*([1-7])"));
    const QRegularExpressionMatch match = re.match(text);
    if (!match.hasMatch()) {
        return 0;
    }
    return match.captured(1).toInt();
}

QString normalizeDiCode(QString diCode)
{
    diCode = diCode.trimmed().toUpper();
    if (!diCode.startsWith(QStringLiteral("0X")) && diCode.size() == 8) {
        diCode.prepend(QStringLiteral("0X"));
    }
    return diCode;
}

} // namespace

namespace StatusWordBitCatalog {

bool isStatusWordCode(const QString &diCode)
{
    return statusWordIndexFromCode(diCode) > 0;
}

bool hasBitDefinition(const QString &diCode)
{
    return !definitionForCode(diCode).fields.isEmpty();
}

bool hasBitDefinitionForDescription(const QString &description)
{
    return !definitionForDescription(description).fields.isEmpty();
}

int statusWordIndexFromCode(const QString &diCode)
{
    const QString normalized = normalizeDiCode(diCode);
    if (!normalized.startsWith(QStringLiteral("0X040005"))) {
        return 0;
    }

    bool ok = false;
    const int index = normalized.right(2).toInt(&ok, 16);
    if (!ok || index < 1 || index > 7) {
        return 0;
    }
    return index;
}

int statusWordIndexFromDescription(const QString &description)
{
    return parseStatusWordIndex(description);
}

StatusWordDefinition definitionForIndex(int index)
{
    if (index < 1 || index > 7) {
        return StatusWordDefinition();
    }
    return buildStatusWordDefinition(index);
}

StatusWordDefinition definitionForCode(const QString &diCode)
{
    const QString normalized = normalizeDiCode(diCode);
    if (normalized == QStringLiteral("0X04000601")) {
        return buildActiveCombinationFeatureWord();
    }
    if (normalized == QStringLiteral("0X04000602")) {
        return buildReactiveCombinationFeatureWord(1);
    }
    if (normalized == QStringLiteral("0X04000603")) {
        return buildReactiveCombinationFeatureWord(2);
    }
    if (normalized == QStringLiteral("0X04000801")) {
        return buildWeekRestFeatureWord();
    }
    if (normalized == QStringLiteral("0X04000901")) {
        return buildLoadRecordModeWord();
    }
    if (normalized == QStringLiteral("0X04000C01")) {
        return buildSecurityAuthErrorWord();
    }

    const StatusWordDefinition freezeDefinition = buildFreezeModeWordByCode(normalized);
    if (!freezeDefinition.fields.isEmpty()) {
        return freezeDefinition;
    }

    const StatusWordDefinition rateDefinition = buildCommRateFeatureWordByCode(normalized);
    if (!rateDefinition.fields.isEmpty()) {
        return rateDefinition;
    }

    return definitionForIndex(statusWordIndexFromCode(diCode));
}

StatusWordDefinition definitionForDescription(const QString &description)
{
    if (description.contains(QStringLiteral("有功组合方式特征字"))) {
        return buildActiveCombinationFeatureWord();
    }
    if (description.contains(QStringLiteral("无功组合方式1特征字"))) {
        return buildReactiveCombinationFeatureWord(1);
    }
    if (description.contains(QStringLiteral("无功组合方式2特征字"))) {
        return buildReactiveCombinationFeatureWord(2);
    }
    // 兼容“无功组合方式特征字”这类统称（优先方式1）
    if (description.contains(QStringLiteral("无功组合方式")) && description.contains(QStringLiteral("特征字"))) {
        if (description.contains(QStringLiteral("2"))) {
            return buildReactiveCombinationFeatureWord(2);
        }
        return buildReactiveCombinationFeatureWord(1);
    }

    if (description.contains(QStringLiteral("周休日特征字"))) {
        return buildWeekRestFeatureWord();
    }
    if (description.contains(QStringLiteral("负荷记录模式字"))) {
        return buildLoadRecordModeWord();
    }
    if (description.contains(QStringLiteral("安全认证错误信息字")) ||
        description.contains(QStringLiteral("SERR"), Qt::CaseInsensitive)) {
        return buildSecurityAuthErrorWord();
    }
    // 协议异常应答中的错误信息字 ERR（非 DI 数据项，按名称匹配）
    if ((description.contains(QStringLiteral("错误信息字")) && !description.contains(QStringLiteral("安全认证"))) ||
        description.compare(QStringLiteral("ERR"), Qt::CaseInsensitive) == 0) {
        return buildProtocolErrorWord();
    }

    const StatusWordDefinition freezeDefinition = buildFreezeModeWordByDescription(description);
    if (!freezeDefinition.fields.isEmpty()) {
        return freezeDefinition;
    }

    const StatusWordDefinition rateDefinition = buildCommRateFeatureWordByDescription(description);
    if (!rateDefinition.fields.isEmpty()) {
        return rateDefinition;
    }

    const int index = statusWordIndexFromDescription(description);
    if (index > 0) {
        return definitionForIndex(index);
    }
    return StatusWordDefinition();
}

QString bitRangeLabel(const StatusWordBitField &field)
{
    if (field.bitWidth <= 1) {
        return QStringLiteral("Bit%1").arg(field.bitOffset);
    }
    return QStringLiteral("Bit%1~Bit%2")
        .arg(field.bitOffset)
        .arg(field.bitOffset + field.bitWidth - 1);
}

int extractFieldValue(quint16 word, const StatusWordBitField &field)
{
    if (field.bitWidth <= 0) {
        return 0;
    }
    const quint16 mask = static_cast<quint16>(((1u << field.bitWidth) - 1u) << field.bitOffset);
    return static_cast<int>((word & mask) >> field.bitOffset);
}

quint16 applyFieldValue(quint16 word, const StatusWordBitField &field, int fieldValue)
{
    if (field.bitWidth <= 0) {
        return word;
    }
    const quint16 mask = static_cast<quint16>(((1u << field.bitWidth) - 1u) << field.bitOffset);
    const int maxValue = (1 << field.bitWidth) - 1;
    const int clamped = qBound(0, fieldValue, maxValue);
    word = static_cast<quint16>(word & ~mask);
    word = static_cast<quint16>(word | ((static_cast<quint16>(clamped) << field.bitOffset) & mask));
    return word;
}

QString meaningText(const StatusWordBitField &field, int fieldValue)
{
    if (field.reserved) {
        return QStringLiteral("保留");
    }
    if (field.meanings.contains(fieldValue)) {
        return field.meanings.value(fieldValue);
    }
    return QStringLiteral("未定义取值");
}

QString meaningsSummary(const StatusWordBitField &field)
{
    if (field.reserved) {
        return QStringLiteral("-");
    }
    if (field.meanings.isEmpty()) {
        return QStringLiteral("-");
    }

    QStringList parts;
    const QList<int> keys = field.meanings.keys();
    for (int key : keys) {
        if (field.bitWidth > 1) {
            parts << QStringLiteral("%1 %2")
                         .arg(key, field.bitWidth, 2, QChar('0'))
                         .arg(field.meanings.value(key));
        } else {
            parts << QStringLiteral("%1 %2").arg(key).arg(field.meanings.value(key));
        }
    }
    return parts.join(QStringLiteral("，"));
}

QString formatWordHex(quint16 word, int byteWidth)
{
    const int width = qBound(1, byteWidth, 2) * 2;
    if (byteWidth <= 1) {
        word = static_cast<quint16>(word & 0xFF);
    }
    return QStringLiteral("0x%1").arg(word, width, 16, QChar('0')).toUpper().replace(QStringLiteral("0X"), QStringLiteral("0x"));
}

QString formatProtocolBytes(quint16 word, int byteWidth)
{
    if (byteWidth <= 1) {
        const quint8 low = static_cast<quint8>(word & 0xFF);
        return QStringLiteral("%1").arg(low, 2, 16, QChar('0')).toUpper();
    }

    const quint8 low = static_cast<quint8>(word & 0xFF);
    const quint8 high = static_cast<quint8>((word >> 8) & 0xFF);
    return QStringLiteral("%1 %2")
        .arg(low, 2, 16, QChar('0'))
        .arg(high, 2, 16, QChar('0'))
        .toUpper();
}

QString htmlBitDefinition(const StatusWordDefinition &definition)
{
    if (definition.fields.isEmpty()) {
        return QString();
    }

    QStringList lines;
    for (const StatusWordBitField &field : definition.fields) {
        if (field.reserved) {
            lines << QStringLiteral("%1：保留").arg(bitRangeLabel(field));
            continue;
        }

        if (field.meanings.isEmpty()) {
            lines << QStringLiteral("%1：%2").arg(bitRangeLabel(field), field.name);
        } else {
            lines << QStringLiteral("%1：%2（%3）")
                         .arg(bitRangeLabel(field), field.name, meaningsSummary(field));
        }
    }

    for (const QString &note : definition.notes) {
        lines << note;
    }

    return lines.join(QStringLiteral("<br/>"));
}

QString htmlBitDefinitionForDescription(const QString &description)
{
    return htmlBitDefinition(definitionForDescription(description));
}

QStringList detailLinesForWord(const StatusWordDefinition &definition, quint16 word)
{
    if (definition.fields.isEmpty()) {
        return {};
    }

    QStringList lines;
    const QString title = definition.subtitle.isEmpty()
                              ? definition.title
                              : QStringLiteral("%1（%2）").arg(definition.title, definition.subtitle);
    lines << QStringLiteral("位解析：%1 = %2")
                 .arg(title.isEmpty() ? QStringLiteral("状态字") : title,
                      formatWordHex(word, definition.byteWidth));

    for (const StatusWordBitField &field : definition.fields) {
        if (field.reserved) {
            continue;
        }

        const int fieldValue = extractFieldValue(word, field);
        QString valueBits;
        if (field.bitWidth > 1) {
            valueBits = QStringLiteral("%1").arg(fieldValue, field.bitWidth, 2, QChar('0'));
        } else {
            valueBits = QString::number(fieldValue);
        }

        lines << QStringLiteral("  %1 %2: %3 %4")
                     .arg(bitRangeLabel(field),
                          field.name,
                          valueBits,
                          meaningText(field, fieldValue));
    }

    return lines;
}

QStringList detailLinesForPayload(const QString &diCode, const QByteArray &payload)
{
    const StatusWordDefinition definition = definitionForCode(diCode);
    if (definition.fields.isEmpty()) {
        return {};
    }

    const int width = qBound(1, definition.byteWidth, 2);
    if (payload.size() < width) {
        return {};
    }

    quint16 word = 0;
    if (width <= 1) {
        word = static_cast<quint8>(payload.at(0));
    } else {
        // 协议传输低字节在前：06 00 => 0x0006
        word = static_cast<quint16>(static_cast<quint8>(payload.at(0))
                                    | (static_cast<quint16>(static_cast<quint8>(payload.at(1))) << 8));
    }

    return detailLinesForWord(definition, word);
}

QString describeErrorInfoByte(quint8 errorCode)
{
    const StatusWordDefinition definition = definitionForDescription(QStringLiteral("错误信息字"));
    QStringList active;
    for (const StatusWordBitField &field : definition.fields) {
        if (field.reserved) {
            continue;
        }
        const int value = extractFieldValue(errorCode, field);
        if (value != 0) {
            active << QStringLiteral("%1 %2").arg(bitRangeLabel(field), field.name);
        }
    }
    if (active.isEmpty()) {
        return QStringLiteral("无错误标志");
    }
    return active.join(QStringLiteral("；"));
}

} // namespace StatusWordBitCatalog