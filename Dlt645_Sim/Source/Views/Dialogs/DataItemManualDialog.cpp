#include "DataItemManualDialog.h"

#include "Models/DataItem.h"
#include "Models/MeterModel.h"
#include "Services/TermCatalogService.h"
#include "Utils/UiTheme.h"
#include "Views/Common/TermExplanationWidget.h"

#include <QApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSizePolicy>
#include <QSplitter>
#include <QTableWidget>
#include <QTextStream>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QDebug>

namespace {
QString normalizedDiCode(QString text)
{
    text = text.trimmed().toUpper();
    if (!text.startsWith(QStringLiteral("0X")) && text.size() == 8) {
        text.prepend(QStringLiteral("0X"));
    }
    return text;
}

QString cleanedEnergyDescription(QString text)
{
    text.remove(QString::fromUtf8("(当前)"));
    text.remove(QString::fromUtf8("电能数据块"));
    text.remove(QString::fromUtf8("电能"));
    return text.trimmed();
}

QString cleanedDemandDescription(QString text)
{
    text.remove(QStringLiteral("(当前)"));
    text.remove(QStringLiteral("最大需量及发生时间数据块"));
    text.remove(QStringLiteral("总最大需量发生时间"));
    text.remove(QStringLiteral("最大需量发生时间"));
    text.remove(QStringLiteral("总最大需量"));
    text.remove(QStringLiteral("最大需量"));
    return text.trimmed();
}

QString demandDefinitionForDescription(const QString &description)
{
    const bool isPhase = description.contains(QStringLiteral("A相")) ||
                         description.contains(QStringLiteral("B相")) ||
                         description.contains(QStringLiteral("C相"));
    const bool isTariff = description.contains(QStringLiteral("费率"));
    const bool isBlock = description.contains(QStringLiteral("数据块"));
    const bool isOccurrenceTime = description.contains(QStringLiteral("发生时间"));
    const bool isHistory = description.contains(QStringLiteral("结算日"));

    QString subject = cleanedDemandDescription(description);
    if (subject.isEmpty()) {
        subject = QStringLiteral("需量");
    }

    QString definition;
    if (isBlock) {
        definition = QStringLiteral("%1最大需量及发生时间数据块用于一次性返回总最大需量、各费率最大需量及其发生时间，便于批量读取和核对。").arg(subject);
    } else if (isOccurrenceTime) {
        definition = QStringLiteral("%1最大需量发生时间表示该最大需量值对应的记录时刻，用于定位负荷峰值出现的时间。").arg(subject);
    } else if (description.contains(QStringLiteral("铜损"))) {
        definition = QStringLiteral("%1最大需量表示按铜损补偿口径折算得到的最大平均功率，用于分析高负荷时段的线路或绕组损耗影响。").arg(subject);
    } else if (description.contains(QStringLiteral("铁损"))) {
        definition = QStringLiteral("%1最大需量表示按铁损补偿口径折算得到的最大平均功率，用于分析高负荷时段的铁芯损耗影响。").arg(subject);
    } else if (description.contains(QStringLiteral("基波"))) {
        definition = QStringLiteral("%1最大需量表示仅统计基波分量时，在需量周期内出现的最大平均功率。").arg(subject);
    } else if (description.contains(QStringLiteral("谐波"))) {
        definition = QStringLiteral("%1最大需量表示由谐波分量对应的平均功率统计得到的最大需量，用于观察非线性负荷带来的影响。").arg(subject);
    } else if (description.contains(QStringLiteral("组合有功"))) {
        definition = QStringLiteral("%1最大需量表示电表按组合有功规则统计得到的最大平均功率，用于适配不同结算口径。").arg(subject);
    } else if (description.contains(QStringLiteral("正向有功"))) {
        definition = QStringLiteral("%1最大需量表示用户从电网吸收有功功率时，在需量周期内出现的最大平均功率。").arg(subject);
    } else if (description.contains(QStringLiteral("反向有功"))) {
        definition = QStringLiteral("%1最大需量表示反向送电时，在需量周期内出现的最大平均功率。").arg(subject);
    } else if (description.contains(QStringLiteral("组合无功1")) || description.contains(QStringLiteral("组合无功2"))) {
        definition = QStringLiteral("%1最大需量表示按无功组合方式统计得到的最大平均无功功率。").arg(subject);
    } else if (description.contains(QStringLiteral("第一象限"))) {
        definition = QStringLiteral("%1最大需量表示第一象限无功运行状态下，在需量周期内出现的最大平均无功功率。").arg(subject);
    } else if (description.contains(QStringLiteral("第二象限"))) {
        definition = QStringLiteral("%1最大需量表示第二象限无功运行状态下，在需量周期内出现的最大平均无功功率。").arg(subject);
    } else if (description.contains(QStringLiteral("第三象限"))) {
        definition = QStringLiteral("%1最大需量表示第三象限无功运行状态下，在需量周期内出现的最大平均无功功率。").arg(subject);
    } else if (description.contains(QStringLiteral("第四象限"))) {
        definition = QStringLiteral("%1最大需量表示第四象限无功运行状态下，在需量周期内出现的最大平均无功功率。").arg(subject);
    } else if (description.contains(QStringLiteral("正向视在"))) {
        definition = QStringLiteral("%1最大需量表示正向视在功率在需量周期内的最大平均值，反映容量占用峰值。").arg(subject);
    } else if (description.contains(QStringLiteral("反向视在"))) {
        definition = QStringLiteral("%1最大需量表示反向视在功率在需量周期内的最大平均值。").arg(subject);
    } else if (description.contains(QStringLiteral("关联"))) {
        definition = QStringLiteral("%1最大需量表示按关联统计口径得到的最大平均功率。").arg(subject);
    } else {
        definition = QStringLiteral("%1是最大需量分类下的数据项，用于记录规定需量周期内出现的最大平均功率。").arg(subject);
    }

    if (isTariff && !isBlock && !isOccurrenceTime) {
        definition += QStringLiteral(" 费率项只在对应费率时段内参与统计。");
    }
    if (isPhase) {
        definition += QStringLiteral(" 该数据项仅对应单相，不代表三相总需量。");
    }
    if (isHistory) {
        definition += QStringLiteral(" 结算日标识说明这是历史结算快照值。");
    }

    return definition;
}

QString demandFormulaForDescription(const QString &description)
{
    QStringList parts;
    parts << QStringLiteral("D = E / T");
    parts << QStringLiteral("最大需量 = max(D_i)");

    if (description.contains(QStringLiteral("发生时间"))) {
        parts << QStringLiteral("t_occ = arg max(D_i)");
    }

    if (description.contains(QStringLiteral("费率"))) {
        parts << QStringLiteral("D_费率n = max(E_n,i / T)");
    } else if (description.contains(QStringLiteral("总最大需量"))) {
        parts << QStringLiteral("D_总 = max(D_费率1 ... D_费率n)");
    }

    if (description.contains(QStringLiteral("数据块"))) {
        parts << QStringLiteral("数据块 = {D_总, t_总, D_费率1, t_1, ...}");
    }

    return parts.join(QStringLiteral("；"));
}

QString cleanedVariableDescription(QString text)
{
    text.remove(QStringLiteral("瞬时"));
    text.remove(QStringLiteral("总"));
    text.remove(QStringLiteral("当前"));
    text.remove(QStringLiteral("数据块"));
    text.remove(QStringLiteral("波形失真度"));
    text.remove(QStringLiteral("谐波含量"));
    return text.trimmed();
}

QString variableDefinitionForDescription(const QString &description)
{
    const bool isPhase = description.contains(QStringLiteral("A相")) ||
                         description.contains(QStringLiteral("B相")) ||
                         description.contains(QStringLiteral("C相")) ||
                         description.contains(QStringLiteral("N相"));
    const bool isBlock = description.contains(QStringLiteral("数据块"));
    const bool isInstant = description.contains(QStringLiteral("瞬时"));
    const bool isTotal = description.contains(QStringLiteral("总"));

    QString subject = cleanedVariableDescription(description);
    if (subject.isEmpty()) {
        subject = QStringLiteral("变量");
    }

    QString definition;
    if (description.contains(QStringLiteral("电压波形失真度"))) {
        definition = QStringLiteral("%1表示对应相电压总谐波畸变程度，用于衡量电压波形偏离理想正弦波的程度。").arg(subject);
    } else if (description.contains(QStringLiteral("电流波形失真度"))) {
        definition = QStringLiteral("%1表示对应相电流总谐波畸变程度，用于观察非线性负载对电流波形的影响。").arg(subject);
    } else if (description.contains(QStringLiteral("电压")) && description.contains(QStringLiteral("谐波含量"))) {
        definition = QStringLiteral("%1表示指定次谐波电压分量相对于基波的百分比，用于电能质量分析。").arg(subject);
    } else if (description.contains(QStringLiteral("电流")) && description.contains(QStringLiteral("谐波含量"))) {
        definition = QStringLiteral("%1表示指定次谐波电流分量相对于基波的百分比，用于识别谐波源和负载特征。").arg(subject);
    } else if (description.contains(QStringLiteral("电压"))) {
        definition = QStringLiteral("%1是电表当前测得的电压有效值，用于反映相线对参考点之间的电位差。").arg(subject);
    } else if (description.contains(QStringLiteral("电流"))) {
        definition = QStringLiteral("%1是电表当前测得的电流有效值，用于反映导体中的载流大小。").arg(subject);
    } else if (description.contains(QStringLiteral("有功功率"))) {
        definition = QStringLiteral("%1表示当前实际做功部分的功率读数，用于反映实时负载消耗水平。").arg(subject);
    } else if (description.contains(QStringLiteral("无功功率"))) {
        definition = QStringLiteral("%1表示当前无功功率读数，用于反映感性或容性功率交换水平。").arg(subject);
    } else if (description.contains(QStringLiteral("视在功率"))) {
        definition = QStringLiteral("%1表示当前视在功率读数，用于反映容量占用情况。").arg(subject);
    } else if (description.contains(QStringLiteral("功率因数"))) {
        definition = QStringLiteral("%1表示有功功率与视在功率的比值，用于衡量电能利用效率。").arg(subject);
    } else if (description.contains(QStringLiteral("相角"))) {
        definition = QStringLiteral("%1表示对应相位关系的角度信息，用于分析电压、电流之间的相位偏移。").arg(subject);
    } else if (description.contains(QStringLiteral("频率"))) {
        definition = QStringLiteral("%1表示当前电网交流频率，用于反映系统运行节奏是否稳定在额定频率附近。").arg(subject);
    } else if (description.contains(QStringLiteral("一分钟有功总平均功率"))) {
        definition = QStringLiteral("%1表示最近一分钟窗口内的有功平均功率，可用于短时负荷趋势观察。").arg(subject);
    } else if (description.contains(QStringLiteral("有功需量"))) {
        definition = QStringLiteral("%1表示当前需量周期内滚动统计得到的有功平均功率。").arg(subject);
    } else if (description.contains(QStringLiteral("无功需量"))) {
        definition = QStringLiteral("%1表示当前需量周期内滚动统计得到的无功平均功率。").arg(subject);
    } else if (description.contains(QStringLiteral("视在需量"))) {
        definition = QStringLiteral("%1表示当前需量周期内滚动统计得到的视在平均功率。").arg(subject);
    } else if (description.contains(QStringLiteral("温度"))) {
        definition = QStringLiteral("%1用于反映电表内部或端子位置的热状态，可用于设备健康监测。").arg(subject);
    } else if (description.contains(QStringLiteral("电池电压"))) {
        definition = QStringLiteral("%1表示对应备用电池当前电压，用于评估后备供电能力。").arg(subject);
    } else if (description.contains(QStringLiteral("工作时间"))) {
        definition = QStringLiteral("%1表示内部电池累计工作时长，用于评估寿命和维护周期。").arg(subject);
    } else if (description.contains(QStringLiteral("阶梯电价"))) {
        definition = QStringLiteral("%1表示当前执行的阶梯电价单价，用于计费与费率展示。").arg(subject);
    } else {
        definition = QStringLiteral("%1是变量分类下的实时测量或状态数据项，用于反映当前运行状态。").arg(subject);
    }

    if (isInstant) {
        definition += QStringLiteral(" “瞬时”表示该值反映当前采样或当前刷新时刻的状态。");
    }
    if (isTotal) {
        definition += QStringLiteral(" “总”表示该值为多相或多分量的综合结果。");
    }
    if (isPhase) {
        definition += QStringLiteral(" 该数据项仅对应指定相别。");
    }
    if (isBlock) {
        definition += QStringLiteral(" 数据块会把同一组相关变量一次性返回。");
    }

    return definition;
}

QString variableFormulaForDescription(const QString &description)
{
    QStringList parts;

    if (description.contains(QStringLiteral("电压")) && !description.contains(QStringLiteral("失真度")) && !description.contains(QStringLiteral("谐波含量"))) {
        parts << QStringLiteral("U = 电压有效值");
    } else if (description.contains(QStringLiteral("电流")) && !description.contains(QStringLiteral("失真度")) && !description.contains(QStringLiteral("谐波含量"))) {
        parts << QStringLiteral("I = 电流有效值");
    } else if (description.contains(QStringLiteral("有功功率"))) {
        parts << QStringLiteral("P = U I cosφ");
    } else if (description.contains(QStringLiteral("无功功率"))) {
        parts << QStringLiteral("Q = U I sinφ");
    } else if (description.contains(QStringLiteral("视在功率"))) {
        parts << QStringLiteral("S = U I = √(P² + Q²)");
    } else if (description.contains(QStringLiteral("功率因数"))) {
        parts << QStringLiteral("PF = P / S = cosφ");
    } else if (description.contains(QStringLiteral("相角"))) {
        parts << QStringLiteral("φ = arccos(P / S)");
    } else if (description.contains(QStringLiteral("电压波形失真度")) || description.contains(QStringLiteral("电流波形失真度"))) {
        parts << QStringLiteral("THD = √(ΣX_h²) / X_1 × 100%");
    } else if (description.contains(QStringLiteral("谐波含量"))) {
        parts << QStringLiteral("H_h = X_h / X_1 × 100%");
    } else if (description.contains(QStringLiteral("频率"))) {
        parts << QStringLiteral("f = 1 / T");
    } else if (description.contains(QStringLiteral("有功需量"))) {
        parts << QStringLiteral("D_P = E_P / T");
    } else if (description.contains(QStringLiteral("无功需量"))) {
        parts << QStringLiteral("D_Q = E_Q / T");
    } else if (description.contains(QStringLiteral("视在需量"))) {
        parts << QStringLiteral("D_S = E_S / T");
    } else if (description.contains(QStringLiteral("一分钟有功总平均功率"))) {
        parts << QStringLiteral("P_avg,1min = ∫P(t)dt / 60s");
    }

    if (description.contains(QStringLiteral("数据块"))) {
        parts << QStringLiteral("数据块 = {同组变量按协议顺序拼接}");
    }

    return parts.join(QStringLiteral("；"));
}

QString cleanedEventDescription(QString text)
{
    text.remove(QStringLiteral("（本月）"));
    text.remove(QStringLiteral("（上"));
    text.remove(QStringLiteral("次）"));
    text.remove(QStringLiteral("月）"));
    text.remove(QStringLiteral("记录"));
    text.remove(QStringLiteral("统计数据"));
    text.remove(QStringLiteral("总次数"));
    text.remove(QStringLiteral("总累计时间"));
    return text.trimmed();
}

QString eventDefinitionForDescription(const QString &description)
{
    const bool isHistoryRecord = description.contains(QStringLiteral("记录"));
    const bool isStatistical = description.contains(QStringLiteral("统计数据"));
    const bool isTotal = description.contains(QStringLiteral("总次数"));
    const bool isMonthly = description.contains(QStringLiteral("本月")) || description.contains(QStringLiteral("上")) && description.contains(QStringLiteral("月"));

    QString subject = cleanedEventDescription(description);
    if (subject.isEmpty()) {
        subject = QStringLiteral("事件");
    }

    QString definition;
    if (description.contains(QStringLiteral("电压合格率"))) {
        definition = QStringLiteral("%1用于统计监测周期内电压质量情况，通常包含监测时间、合格率、越限率以及最高/最低电压及发生时刻。").arg(subject);
    } else if (description.contains(QStringLiteral("需量超限"))) {
        definition = isTotal
            ? QStringLiteral("%1总次数用于累计记录各类需量越过阈值的发生次数。").arg(subject)
            : QStringLiteral("%1记录用于保存某次超限的起止时间、超限期间最大需量及对应发生时间。").arg(subject);
    } else if (description.contains(QStringLiteral("编程")) || description.contains(QStringLiteral("清零")) || description.contains(QStringLiteral("校时"))) {
        definition = isTotal
            ? QStringLiteral("%1总次数用于统计相关运维或管理操作累计发生的次数。").arg(subject)
            : QStringLiteral("%1记录用于保存操作发生时间、操作者及关键操作内容快照。").arg(subject);
    } else if (description.contains(QStringLiteral("购电量")) || description.contains(QStringLiteral("购电金额"))) {
        definition = QStringLiteral("%1用于保存每次购电后的关键结果，如购电时间、购电次数、购入值、购前购后余额和累计值。").arg(subject);
    } else if (description.contains(QStringLiteral("开表盖")) || description.contains(QStringLiteral("开端钮盒"))) {
        definition = isTotal
            ? QStringLiteral("%1总次数用于统计防窃电相关开盖事件的累计发生次数。").arg(subject)
            : QStringLiteral("%1事件记录用于保存开盖起止时间及事件期间的关键电能快照。").arg(subject);
    } else if (description.contains(QStringLiteral("掉电")) || description.contains(QStringLiteral("失压")) || description.contains(QStringLiteral("欠压")) ||
               description.contains(QStringLiteral("过压")) || description.contains(QStringLiteral("断相")) || description.contains(QStringLiteral("失流")) ||
               description.contains(QStringLiteral("过流")) || description.contains(QStringLiteral("断流")) || description.contains(QStringLiteral("过载")) ||
               description.contains(QStringLiteral("潮流反向")) || description.contains(QStringLiteral("逆相序")) || description.contains(QStringLiteral("不平衡")) ||
               description.contains(QStringLiteral("辅助电源失电")) || description.contains(QStringLiteral("全失压"))) {
        definition = isTotal
            ? QStringLiteral("%1总次数/总累计时间用于统计该类运行异常累计发生的次数和持续时长。").arg(subject)
            : QStringLiteral("%1记录用于保存某次异常的发生区间，以及事件发生时的关键电压、电流、功率或电能快照。").arg(subject);
    } else {
        definition = QStringLiteral("%1是事件记录分类下的数据项，用于保存异常、操作或统计类事件的累计信息或历史明细。").arg(subject);
    }

    if (isStatistical) {
        definition += QStringLiteral(" 该项属于统计类结果，通常面向月度或周期性汇总。");
    }
    if (isHistoryRecord) {
        definition += QStringLiteral(" “上N次”表示按时间倒序保存的历史明细。");
    }
    if (isMonthly) {
        definition += QStringLiteral(" 月度标签表示该值对应本月或历史月份统计窗口。");
    }

    return definition;
}

QString eventFormulaForDescription(const QString &description)
{
    QStringList parts;

    if (description.contains(QStringLiteral("总次数"))) {
        parts << QStringLiteral("总次数 = Σ 事件发生次数");
    }
    if (description.contains(QStringLiteral("总累计时间"))) {
        parts << QStringLiteral("累计时间 = Σ(结束时刻 - 开始时刻)");
    }
    if (description.contains(QStringLiteral("电压合格率"))) {
        parts << QStringLiteral("合格率 = 合格时间 / 监测总时间 × 100%");
        parts << QStringLiteral("越限率 = 越限时间 / 监测总时间 × 100%");
    } else if (description.contains(QStringLiteral("需量超限"))) {
        parts << QStringLiteral("超限判据：D > 阈值 且持续时间满足延时条件");
    } else if (description.contains(QStringLiteral("购电量"))) {
        parts << QStringLiteral("购后剩余电量 = 购前剩余电量 + 本次购电量");
    } else if (description.contains(QStringLiteral("购电金额"))) {
        parts << QStringLiteral("购后剩余金额 = 购前剩余金额 + 本次购电金额");
    } else if (description.contains(QStringLiteral("记录"))) {
        parts << QStringLiteral("记录结构 = {开始时刻, 结束时刻, 事件特征量...}");
    }

    return parts.join(QStringLiteral("；"));
}

QString cleanedParameterDescription(QString text)
{
    text.remove(QStringLiteral("第一套"));
    text.remove(QStringLiteral("第二套"));
    text.remove(QStringLiteral("第"));
    text.remove(QStringLiteral("屏"));
    text.remove(QStringLiteral("数据"));
    text.remove(QStringLiteral("数据块"));
    text.remove(QStringLiteral("(ASCII码)"));
    return text.trimmed();
}

QString parameterDefinitionForDescription(const QString &description)
{
    const bool isEventParameter = description.contains(QStringLiteral("事件")) ||
                                  description.contains(QStringLiteral("判定延时时间")) ||
                                  description.contains(QStringLiteral("触发")) ||
                                  description.contains(QStringLiteral("限值")) ||
                                  description.contains(QStringLiteral("阀值"));
    const bool isRateConfig = description.contains(QStringLiteral("费率")) ||
                              description.contains(QStringLiteral("阶梯电价")) ||
                              description.contains(QStringLiteral("时段表")) ||
                              description.contains(QStringLiteral("时区表")) ||
                              description.contains(QStringLiteral("公共假日"));
    const bool isDisplayConfig = description.contains(QStringLiteral("显示")) ||
                                 description.contains(QStringLiteral("屏"));
    const bool isCommConfig = description.contains(QStringLiteral("通信")) ||
                              description.contains(QStringLiteral("地址")) ||
                              description.contains(QStringLiteral("表号")) ||
                              description.contains(QStringLiteral("客户编号"));

    QString subject = cleanedParameterDescription(description);
    if (subject.isEmpty()) {
        subject = QStringLiteral("参变量");
    }

    QString definition;
    if (description.contains(QStringLiteral("当前日期")) || description.contains(QStringLiteral("当前时间"))) {
        definition = QStringLiteral("%1用于设置和保存电表内部时钟，是费率切换、冻结、事件记录等时序功能的基础。").arg(subject);
    } else if (description.contains(QStringLiteral("时区表数据"))) {
        definition = QStringLiteral("%1用于定义一年内不同日期区间对应的日时段表号，是多费率日历切换规则的入口配置。").arg(subject);
    } else if (description.contains(QStringLiteral("日时段表数据"))) {
        definition = QStringLiteral("%1用于定义一天内各时间段对应的费率号，决定当前时刻采用哪一档费率或电价。").arg(subject);
    } else if (description.contains(QStringLiteral("公共假日")) && description.contains(QStringLiteral("日期"))) {
        definition = QStringLiteral("%1用于定义公共假日日期及其采用的日时段表号，使节假日可以按独立计费日历执行。").arg(subject);
    } else if (description.contains(QStringLiteral("显示数据项"))) {
        definition = QStringLiteral("%1用于配置指定轮显屏展示的数据项及是否分屏显示，决定液晶轮显或按键轮显的展示内容。").arg(subject);
    } else if (description.contains(QStringLiteral("最大需量周期")) || description.contains(QStringLiteral("滑差时间"))) {
        definition = QStringLiteral("%1用于定义需量计算窗口及滑动更新方式，直接影响当前需量和最大需量统计结果。").arg(subject);
    } else if (description.contains(QStringLiteral("校表脉冲")) || description.contains(QStringLiteral("电表有功常数")) || description.contains(QStringLiteral("电表无功常数"))) {
        definition = QStringLiteral("%1属于计量校验相关参数，用于脉冲输出和标准校表换算。").arg(subject);
    } else if (description.contains(QStringLiteral("切换时间"))) {
        definition = QStringLiteral("%1用于指定两套配置生效切换的时间点，支持时区、时段、电价或梯度方案的定时替换。").arg(subject);
    } else if (description.contains(QStringLiteral("数")) || description.contains(QStringLiteral("个"))) {
        definition = QStringLiteral("%1用于描述参数结构规模，例如费率数、时段数、假日数或梯度数。").arg(subject);
    } else if (isDisplayConfig) {
        definition = QStringLiteral("%1用于配置液晶或界面的轮显方式、显示内容以及显示精度。").arg(subject);
    } else if (description.contains(QStringLiteral("互感器变比"))) {
        definition = QStringLiteral("%1用于描述 PT/CT 变比关系，影响显示值和工程量换算。").arg(subject);
    } else if (isCommConfig) {
        definition = QStringLiteral("%1属于设备身份或通信配置参数，用于唯一标识电表并约束通信访问行为。").arg(subject);
    } else if (description.contains(QStringLiteral("状态字数据块"))) {
        definition = QStringLiteral("%1用于集中返回多个电表运行状态字，便于一次读取当前运行标志与功能状态。").arg(subject);
    } else if (description.contains(QStringLiteral("状态字"))) {
        definition = QStringLiteral("%1用于描述电表运行状态或功能标志位，便于主站识别当前配置与运行状态。").arg(subject);
    } else if (description.contains(QStringLiteral("组合方式"))) {
        definition = QStringLiteral("%1用于定义有功或无功的组合统计口径，影响电能量、需量和相关事件判断。").arg(subject);
    } else if (description.contains(QStringLiteral("速率特征字"))) {
        definition = QStringLiteral("%1用于限制或声明对应通信口支持的通信速率集合。").arg(subject);
    } else if (description.contains(QStringLiteral("周休日"))) {
        definition = QStringLiteral("%1用于定义周休日规则以及周休日采用的日时段表。").arg(subject);
    } else if (description.contains(QStringLiteral("模式字"))) {
        definition = QStringLiteral("%1用于指定负荷记录或各类冻结数据的采集内容与模式。").arg(subject);
    } else if (description.contains(QStringLiteral("负荷记录"))) {
        definition = QStringLiteral("%1用于定义负荷记录的起始时间及各类记录间隔。").arg(subject);
    } else if (description.contains(QStringLiteral("结算日"))) {
        definition = QStringLiteral("%1用于定义月度结算冻结日，影响历史结算数据生成。").arg(subject);
    } else if (description.contains(QStringLiteral("冻结"))) {
        definition = QStringLiteral("%1用于定义整点冻结、日冻结等冻结任务的触发时间与间隔。").arg(subject);
    } else if (description.contains(QStringLiteral("无线通信在线")) || description.contains(QStringLiteral("信号强弱"))) {
        definition = QStringLiteral("%1用于表示无线通信链路在线状态及信号强度指示。").arg(subject);
    } else if (description.contains(QStringLiteral("跳闸"))) {
        definition = QStringLiteral("%1用于定义跳闸动作前的延时策略。").arg(subject);
    } else if (description.contains(QStringLiteral("时区表")) || description.contains(QStringLiteral("日时段表")) || description.contains(QStringLiteral("公共假日"))) {
        definition = QStringLiteral("%1用于配置多费率计费日历结构，决定不同日期和时段对应的费率表。").arg(subject);
    } else if (description.contains(QStringLiteral("费率电价"))) {
        definition = QStringLiteral("%1用于配置指定费率号对应的结算单价，是多费率电费计算的直接依据。").arg(subject);
    } else if (description.contains(QStringLiteral("阶梯电价"))) {
        definition = QStringLiteral("%1用于配置各阶梯档位的单价，与阶梯值一起决定分档电费的计算结果。").arg(subject);
    } else if (description.contains(QStringLiteral("阶梯值"))) {
        definition = QStringLiteral("%1用于配置阶梯计费各档的电量阈值，超过阈值后将切换到对应档位的电价。").arg(subject);
    } else if (description.contains(QStringLiteral("费率电价")) || description.contains(QStringLiteral("阶梯电价")) || description.contains(QStringLiteral("阶梯值"))) {
        definition = QStringLiteral("%1用于配置费率价格或阶梯阈值，是电费结算规则的重要组成部分。").arg(subject);
    } else if (isEventParameter) {
        definition = QStringLiteral("%1属于事件触发参数，用于定义异常事件的阈值、恢复条件和判定延时。").arg(subject);
    } else {
        definition = QStringLiteral("%1是参变量分类下的可配置参数项，用于决定电表的计量、显示、通信或事件行为。").arg(subject);
    }

    if (isRateConfig) {
        definition += QStringLiteral(" 该项与时段/费率计费策略直接相关。");
    }
    if (isCommConfig) {
        definition += QStringLiteral(" 该项通常需要与现场台账或主站配置保持一致。");
    }
    if (isEventParameter) {
        definition += QStringLiteral(" 修改后会影响相关事件记录的生成条件。");
    }

    return definition;
}

QString parameterFormulaForDescription(const QString &description)
{
    QStringList parts;

    if (description.contains(QStringLiteral("最大需量周期")) || description.contains(QStringLiteral("滑差时间"))) {
        parts << QStringLiteral("D = E / T");
    } else if (description.contains(QStringLiteral("时区表数据"))) {
        parts << QStringLiteral("当前日时段表号 = f(日期, 时区表配置)");
    } else if (description.contains(QStringLiteral("日时段表数据"))) {
        parts << QStringLiteral("当前费率号 = f(当前时刻, 日时段表配置)");
    } else if (description.contains(QStringLiteral("公共假日")) && description.contains(QStringLiteral("日期"))) {
        parts << QStringLiteral("假日日时段表号 = f(假日日期)");
    } else if (description.contains(QStringLiteral("显示数据项"))) {
        parts << QStringLiteral("屏显示配置 = {目标DI, 是否分屏}");
    } else if (description.contains(QStringLiteral("费率电价"))) {
        parts << QStringLiteral("电费 = 电能量 × 对应费率电价");
    } else if (description.contains(QStringLiteral("阶梯电价")) || description.contains(QStringLiteral("阶梯值"))) {
        parts << QStringLiteral("阶梯电费 = Σ(分档电量 × 分档单价)");
    } else if (description.contains(QStringLiteral("电流互感器变比"))) {
        parts << QStringLiteral("I_一次 = I_二次 × CT变比");
    } else if (description.contains(QStringLiteral("电压互感器变比"))) {
        parts << QStringLiteral("U_一次 = U_二次 × PT变比");
    } else if (description.contains(QStringLiteral("功率因数")) && description.contains(QStringLiteral("阀值"))) {
        parts << QStringLiteral("PF = P / S");
    } else if (description.contains(QStringLiteral("电压不平衡率")) || description.contains(QStringLiteral("电流不平衡率"))) {
        parts << QStringLiteral("不平衡率 = 最大偏差 / 平均值 × 100%");
    } else if (description.contains(QStringLiteral("判定延时时间"))) {
        parts << QStringLiteral("事件成立条件 = 阈值满足 且 持续时间 ≥ 延时");
    } else if (description.contains(QStringLiteral("触发上限")) || description.contains(QStringLiteral("触发下限")) ||
               description.contains(QStringLiteral("恢复下限")) || description.contains(QStringLiteral("限值")) || description.contains(QStringLiteral("阀值"))) {
        parts << QStringLiteral("事件判据 = 测量值与阈值比较");
    } else if (description.contains(QStringLiteral("时区表")) || description.contains(QStringLiteral("日时段表"))) {
        parts << QStringLiteral("当前费率 = 日历日期 + 时段表映射结果");
    } else if (description.contains(QStringLiteral("状态字数据块"))) {
        parts << QStringLiteral("数据块 = {状态字1, 状态字2, ..., 状态字7}");
    } else if (description.contains(QStringLiteral("状态字")) || description.contains(QStringLiteral("特征字")) || description.contains(QStringLiteral("模式字"))) {
        parts << QStringLiteral("参数值 = bit flags / coded fields");
    }

    return parts.join(QStringLiteral("；"));
}

QString cleanedFreezeDescription(QString text)
{
    text.remove(QStringLiteral("上1次"));
    text.remove(QStringLiteral("冻结"));
    text.remove(QStringLiteral("数据块"));
    text.remove(QStringLiteral("数据"));
    return text.trimmed();
}

QString freezeDefinitionForDescription(const QString &description)
{
    const bool isTime = description.contains(QStringLiteral("时间"));
    const bool isBlock = description.contains(QStringLiteral("数据块"));
    const bool isDemand = description.contains(QStringLiteral("最大需量"));
    const bool isVariable = description.contains(QStringLiteral("变量"));
    const bool isEnergyBlock = description.contains(QStringLiteral("电能数据块"));
    const bool isTariff = description.contains(QStringLiteral("费率"));

    QString subject = cleanedFreezeDescription(description);
    if (subject.isEmpty()) {
        subject = QStringLiteral("冻结数据");
    }

    QString definition;
    if (description.contains(QStringLiteral("整点冻结时间"))) {
        definition = QStringLiteral("%1用于记录最近一次整点冻结执行的时刻，是核对整点冻结快照是否生效的时间标记。").arg(subject);
    } else if (isTime) {
        definition = QStringLiteral("%1用于记录对应冻结快照生成的时刻，便于区分不同冻结周期或切换触发点。").arg(subject);
    } else if (isEnergyBlock) {
        definition = QStringLiteral("%1用于一次性返回该冻结时刻的总电能及各费率电能快照，便于按冻结点核对累计电能。").arg(subject);
    } else if (isDemand) {
        definition = QStringLiteral("%1用于保存冻结时刻对应的最大需量及其发生时间，可用于历史峰值追溯与结算核验。").arg(subject);
    } else if (isVariable) {
        definition = QStringLiteral("%1用于保存冻结时刻的瞬时功率等变量快照，反映该冻结点的运行状态。").arg(subject);
    } else if (isBlock) {
        definition = QStringLiteral("%1用于将同一冻结时刻的一组相关数据打包返回，便于批量读取和历史比对。").arg(subject);
    } else if (description.contains(QStringLiteral("定时冻结"))) {
        definition = QStringLiteral("%1表示按预设计划执行冻结后保存的历史快照值，常用于周期性抄表和结算。").arg(subject);
    } else if (description.contains(QStringLiteral("瞬时冻结"))) {
        definition = QStringLiteral("%1表示由命令或条件即时触发冻结后保存的快照值，用于保留某一时刻的现场状态。").arg(subject);
    } else if (description.contains(QStringLiteral("时区表切换")) || description.contains(QStringLiteral("日时段表切换")) ||
               description.contains(QStringLiteral("费率切换")) || description.contains(QStringLiteral("阶梯切换"))) {
        definition = QStringLiteral("%1表示在配置切换触发时自动保存的冻结快照，用于追踪计费日历或阶梯方案切换前后的关键数据。").arg(subject);
    } else if (description.contains(QStringLiteral("日冻结"))) {
        definition = QStringLiteral("%1表示按日冻结规则保存的日界点快照值，常用于日统计、日结算或运行分析。").arg(subject);
    } else if (description.contains(QStringLiteral("正向有功总电能"))) {
        definition = QStringLiteral("%1表示冻结时刻记录的正向有功累计总电能，用于反映截至该冻结点的正向用电量。").arg(subject);
    } else if (description.contains(QStringLiteral("反向有功总电能"))) {
        definition = QStringLiteral("%1表示冻结时刻记录的反向有功累计总电能，用于反映截至该冻结点的反向送电量。").arg(subject);
    } else if (description.contains(QStringLiteral("总电能"))) {
        definition = QStringLiteral("%1表示冻结时刻记录的累计总电能值，用于与当前累计值做历史对比。").arg(subject);
    } else {
        definition = QStringLiteral("%1是冻结分类下的数据项，用于保存特定触发时刻的历史快照，避免后续实时值变化覆盖现场状态。").arg(subject);
    }

    if (isTariff && !isBlock) {
        definition += QStringLiteral(" 费率项表示该冻结点下对应费率时段的独立快照值。");
    }
    if (isBlock) {
        definition += QStringLiteral(" 数据块通常按协议顺序拼接多个子项。");
    }

    return definition;
}

QString freezeFormulaForDescription(const QString &description)
{
    QStringList parts;

    if (description.contains(QStringLiteral("时间"))) {
        parts << QStringLiteral("t_freeze = 冻结触发时刻");
    }
    if (description.contains(QStringLiteral("总电能"))) {
        parts << QStringLiteral("E_freeze = E(t_freeze)");
    }
    if (description.contains(QStringLiteral("费率")) && description.contains(QStringLiteral("电能"))) {
        parts << QStringLiteral("E_费率n,freeze = E_费率n(t_freeze)");
    }
    if (description.contains(QStringLiteral("最大需量"))) {
        parts << QStringLiteral("D_freeze = max(E / T)");
        parts << QStringLiteral("t_occ = arg max(E / T)");
    }
    if (description.contains(QStringLiteral("变量"))) {
        parts << QStringLiteral("X_freeze = X(t_freeze)");
    }
    if (description.contains(QStringLiteral("整点冻结数据块"))) {
        parts << QStringLiteral("数据块 = {t_freeze, E_正向总, E_反向总}");
    } else if (description.contains(QStringLiteral("电能数据块"))) {
        parts << QStringLiteral("数据块 = {E_总, E_费率1, E_费率2, ...}");
    } else if (description.contains(QStringLiteral("数据块"))) {
        parts << QStringLiteral("数据块 = {同组冻结子项按协议顺序拼接}");
    }
    if (description.contains(QStringLiteral("时区表切换")) || description.contains(QStringLiteral("日时段表切换")) ||
        description.contains(QStringLiteral("费率切换")) || description.contains(QStringLiteral("阶梯切换"))) {
        parts << QStringLiteral("冻结触发条件 = 配置切换时刻到达");
    } else if (description.contains(QStringLiteral("定时冻结"))) {
        parts << QStringLiteral("冻结触发条件 = 到达预设冻结计划");
    } else if (description.contains(QStringLiteral("瞬时冻结"))) {
        parts << QStringLiteral("冻结触发条件 = 执行瞬时冻结命令");
    } else if (description.contains(QStringLiteral("日冻结"))) {
        parts << QStringLiteral("冻结触发条件 = 到达日冻结时刻");
    }

    return parts.join(QStringLiteral("；"));
}

QString cleanedLoadRecordDescription(QString text)
{
    text.remove(QStringLiteral("预约"));
    text.remove(QStringLiteral("控制"));
    text.remove(QStringLiteral("(数值+单位代码)"));
    return text.trimmed();
}

QString loadRecordDefinitionForDescription(const QString &description)
{
    const bool isRemoteTrip = description.contains(QStringLiteral("远程跳闸"));
    const bool isRemoteClose = description.contains(QStringLiteral("远程合闸"));
    const bool isTestTrip = description.contains(QStringLiteral("模拟试跳"));
    const bool hasUnitCode = description.contains(QStringLiteral("单位代码"));

    QString subject = cleanedLoadRecordDescription(description);
    if (subject.isEmpty()) {
        subject = QStringLiteral("负荷记录");
    }

    QString definition;
    if (isRemoteTrip) {
        definition = QStringLiteral("%1用于配置或表示预约远程跳闸控制参数，指明在预设延时或指定单位下执行断电动作。").arg(subject);
    } else if (isRemoteClose) {
        definition = QStringLiteral("%1用于配置或表示预约远程合闸控制参数，指明在预设延时或指定单位下执行送电恢复动作。").arg(subject);
    } else if (isTestTrip) {
        definition = QStringLiteral("%1用于配置或表示预约模拟试跳控制参数，常用于验证控制链路与动作逻辑而不直接执行正式停送电。").arg(subject);
    } else {
        definition = QStringLiteral("%1是负荷记录分类下的控制型数据项，用于描述预约执行的控制动作及其时间量纲。").arg(subject);
    }

    if (hasUnitCode) {
        definition += QStringLiteral(" 该数据项通常由数值部分和单位代码部分组合编码，用于共同表达延时长度或动作时距。");
    }

    return definition;
}

QString loadRecordFormulaForDescription(const QString &description)
{
    QStringList parts;

    if (description.contains(QStringLiteral("单位代码"))) {
        parts << QStringLiteral("控制参数 = 数值 + 单位代码");
        parts << QStringLiteral("执行时刻 = t_now + 数值 × unit(unitCode)");
    }
    if (description.contains(QStringLiteral("远程跳闸"))) {
        parts << QStringLiteral("动作 = 断开负荷开关");
    } else if (description.contains(QStringLiteral("远程合闸"))) {
        parts << QStringLiteral("动作 = 闭合负荷开关");
    } else if (description.contains(QStringLiteral("模拟试跳"))) {
        parts << QStringLiteral("动作 = 执行试跳流程");
    }

    return parts.join(QStringLiteral("；"));
}

QString energyDefinitionForDescription(const QString &description)
{
    const bool isPhase = description.contains(QString::fromUtf8("A相")) ||
                         description.contains(QString::fromUtf8("B相")) ||
                         description.contains(QString::fromUtf8("C相"));
    const bool isTariff = description.contains(QString::fromUtf8("费率"));
    const bool isBlock = description.contains(QString::fromUtf8("数据块"));

    QString subject = cleanedEnergyDescription(description);
    if (subject.isEmpty()) {
        subject = QString::fromUtf8("电能量");
    }

    QString definition;
    if (description.contains(QString::fromUtf8("铜损"))) {
        definition = QString::fromUtf8("铜损有功电能补偿量是根据电流引起的线路或绕组电阻损耗折算出的有功电能补偿累计量。");
    } else if (description.contains(QString::fromUtf8("铁损"))) {
        definition = QString::fromUtf8("铁损有功电能补偿量是根据铁芯励磁损耗折算出的有功电能补偿累计量。");
    } else if (description.contains(QString::fromUtf8("基波"))) {
        definition = QString::fromUtf8("基波有功电能是电压、电流基波分量产生的有功电能累计量，用于区分基波与谐波对电能的贡献。");
    } else if (description.contains(QString::fromUtf8("谐波"))) {
        definition = QString::fromUtf8("谐波有功电能是谐波电压、电流分量产生的有功电能累计量，用于观察非线性负载带来的谐波能量影响。");
    } else if (description.contains(QString::fromUtf8("组合有功"))) {
        definition = QString::fromUtf8("组合有功电能是电表按配置规则对正向、反向有功电能进行组合后的累计量，用于适配不同计量结算口径。");
    } else if (description.contains(QString::fromUtf8("正向有功"))) {
        definition = QString::fromUtf8("正向有功电能是用户从电网吸收并实际做功的有功电能累计量。");
    } else if (description.contains(QString::fromUtf8("反向有功"))) {
        definition = QString::fromUtf8("反向有功电能是用户侧向电网送出的有功电能累计量，常见于光伏、储能或发电回馈场景。");
    } else if (description.contains(QString::fromUtf8("组合无功1")) || description.contains(QString::fromUtf8("组合无功2"))) {
        definition = QString::fromUtf8("组合无功电能是电表按无功组合方式把指定象限的无功电能合成为一个累计量。组合1、组合2通常用于不同无功考核口径。");
    } else if (description.contains(QString::fromUtf8("第一象限"))) {
        definition = QString::fromUtf8("第一象限无功电能是在有功功率为正、无功功率为正时累计的无功电能，通常对应感性负载从电网吸收无功。");
    } else if (description.contains(QString::fromUtf8("第二象限"))) {
        definition = QString::fromUtf8("第二象限无功电能是在有功功率为负、无功功率为正时累计的无功电能，表示反向有功状态下的正向无功分量。");
    } else if (description.contains(QString::fromUtf8("第三象限"))) {
        definition = QString::fromUtf8("第三象限无功电能是在有功功率为负、无功功率为负时累计的无功电能，表示反向有功状态下的反向无功分量。");
    } else if (description.contains(QString::fromUtf8("第四象限"))) {
        definition = QString::fromUtf8("第四象限无功电能是在有功功率为正、无功功率为负时累计的无功电能，通常对应容性负载向电网送出无功。");
    } else if (description.contains(QString::fromUtf8("正向视在"))) {
        definition = QString::fromUtf8("正向视在电能是正向视在功率随时间的累计量，反映电压电流容量占用的累计效果。");
    } else if (description.contains(QString::fromUtf8("反向视在"))) {
        definition = QString::fromUtf8("反向视在电能是反向送电方向上的视在功率累计量。");
    } else if (description.contains(QString::fromUtf8("关联"))) {
        definition = QString::fromUtf8("关联电能是按电表关联关系或结算规则得到的电能累计量，通常用于把相关相别或方向的电能纳入同一统计口径。");
    } else {
        definition = QString::fromUtf8("%1是电能量分类下的累计电能数据项。").arg(subject);
    }

    if (isPhase) {
        definition += QString::fromUtf8("该数据项只统计对应相别，不代表三相总量。");
    }
    if (isTariff) {
        definition += QString::fromUtf8("费率项只累计电表运行在对应费率时段内产生的电能。");
    }
    if (isBlock) {
        definition += QString::fromUtf8("数据块一次返回总电能及各费率电能，便于批量读取。");
    }

    return definition;
}

QString energyFormulaForDescription(const QString &description)
{
    QString integral;
    if (description.contains(QString::fromUtf8("视在"))) {
        integral = QStringLiteral("E_S = ∫ S(t) dt，S = √(P² + Q²)");
    } else if (description.contains(QString::fromUtf8("无功")) || description.contains(QString::fromUtf8("象限"))) {
        integral = QStringLiteral("E_Q = ∫ Q(t) dt");
    } else if (description.contains(QString::fromUtf8("铜损"))) {
        integral = QStringLiteral("E_Cu = ∫ P_Cu(t) dt，P_Cu ≈ I²R");
    } else if (description.contains(QString::fromUtf8("铁损"))) {
        integral = QStringLiteral("E_Fe = ∫ P_Fe(t) dt，P_Fe 由电压、频率和铁芯参数决定");
    } else if (description.contains(QString::fromUtf8("基波"))) {
        integral = QStringLiteral("E_1 = ∫ P_1(t) dt");
    } else if (description.contains(QString::fromUtf8("谐波"))) {
        integral = QStringLiteral("E_h = ∫ ΣP_h(t) dt，h ≥ 2");
    } else {
        integral = QStringLiteral("E_P = ∫ P(t) dt");
    }

    QStringList parts;
    parts << integral;

    if (description.contains(QString::fromUtf8("组合有功"))) {
        parts << QString::fromUtf8("E_组合有功 = α·E_正向有功 + β·E_反向有功（α、β由电表组合方式配置）");
    } else if (description.contains(QString::fromUtf8("组合无功1")) || description.contains(QString::fromUtf8("组合无功2"))) {
        parts << QString::fromUtf8("E_组合无功 = ΣE_象限i（参与组合的象限由电表无功组合方式配置）");
    }

    if (description.contains(QString::fromUtf8("第一象限"))) {
        parts << QString::fromUtf8("第一象限条件：P > 0 且 Q > 0");
    } else if (description.contains(QString::fromUtf8("第二象限"))) {
        parts << QString::fromUtf8("第二象限条件：P < 0 且 Q > 0");
    } else if (description.contains(QString::fromUtf8("第三象限"))) {
        parts << QString::fromUtf8("第三象限条件：P < 0 且 Q < 0");
    } else if (description.contains(QString::fromUtf8("第四象限"))) {
        parts << QString::fromUtf8("第四象限条件：P > 0 且 Q < 0");
    }

    if (description.contains(QString::fromUtf8("正向"))) {
        parts << QString::fromUtf8("正向累计条件：功率方向为从电网流向用户");
    } else if (description.contains(QString::fromUtf8("反向"))) {
        parts << QString::fromUtf8("反向累计条件：功率方向为从用户流向电网");
    }

    if (description.contains(QString::fromUtf8("费率"))) {
        parts << QString::fromUtf8("E_费率n = ΣE(t)，条件为当前费率 = n");
    } else if (description.contains(QString::fromUtf8("总")) && !description.contains(QString::fromUtf8("补偿量"))) {
        parts << QString::fromUtf8("E_总 = ΣE_费率i");
    } else if (description.contains(QString::fromUtf8("数据块"))) {
        parts << QString::fromUtf8("数据块 = E_总 + E_费率1 + E_费率2 + ...");
    }

    return parts.join(QString::fromUtf8("；"));
}
}

DataItemManualDialog::DataItemManualDialog(QWidget *parent)
    : QDialog(parent),
      m_categoryTree(nullptr),
      m_dataTable(nullptr),
      m_searchBox(nullptr),
      m_termExplanationWidget(nullptr),
      m_catalogMeterModel(nullptr),
      m_currentCategory(QString::fromUtf8("全部"))
{
    setWindowFlags(windowFlags() | Qt::WindowMinimizeButtonHint);
    setupUI();
    loadDataItems();
    populateTree();
    populateTable(m_allDataItems);
    applyDarkTheme();

    setWindowTitle(QString::fromUtf8("DLT645 数据项手册"));
    resize(1000, 700);
}

DataItemManualDialog::~DataItemManualDialog()
{
}

void DataItemManualDialog::setupUI()
{
    // 主布局
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(10);

    // 顶部搜索栏
    QHBoxLayout *searchLayout = new QHBoxLayout();
    QLabel *searchLabel = new QLabel(QString::fromUtf8("搜索:"), this);
    m_searchBox = new QLineEdit(this);
    m_searchBox->setPlaceholderText(QString::fromUtf8("输入 DI 码或描述关键词..."));
    m_searchBox->setClearButtonEnabled(true);
    connect(m_searchBox, &QLineEdit::textChanged, this, &DataItemManualDialog::onSearchTextChanged);

    searchLayout->addWidget(searchLabel);
    searchLayout->addWidget(m_searchBox);
    mainLayout->addLayout(searchLayout);

    // 内容区上下分割：上方数据表、下方术语解释
    QSplitter *mainSplitter = new QSplitter(Qt::Vertical, this);

    // 上方分割器（左侧树 + 右侧表格）
    QSplitter *splitter = new QSplitter(Qt::Horizontal, mainSplitter);

    // 左侧分类树
    m_categoryTree = new QTreeWidget(this);
    m_categoryTree->setHeaderLabel(QString::fromUtf8("数据分类"));
    m_categoryTree->setMaximumWidth(250);
    connect(m_categoryTree, &QTreeWidget::itemClicked, this, &DataItemManualDialog::onCategorySelected);
    splitter->addWidget(m_categoryTree);

    // 右侧数据表格
    m_dataTable = new QTableWidget(this);
    m_dataTable->setColumnCount(5);
    m_dataTable->setHorizontalHeaderLabels({
        QString::fromUtf8("DI 码"),
        QString::fromUtf8("描述"),
        QString::fromUtf8("格式"),
        QString::fromUtf8("单位"),
        QString::fromUtf8("可写")
    });
    m_dataTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_dataTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_dataTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_dataTable->setAlternatingRowColors(false);  // 禁用交替行色
    m_dataTable->setShowGrid(true);  // 显示网格线
    m_dataTable->verticalHeader()->setVisible(false);  // 隐藏行号列
    m_dataTable->verticalHeader()->setDefaultSectionSize(30);  // 行高 30px
    m_dataTable->horizontalHeader()->setStretchLastSection(false);
    m_dataTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    m_dataTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_dataTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    m_dataTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    m_dataTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed);
    m_dataTable->setColumnWidth(0, 140);  // DI 码（增加到 140px）
    m_dataTable->setColumnWidth(2, 150);  // 格式
    m_dataTable->setColumnWidth(3, 80);   // 单位
    m_dataTable->setColumnWidth(4, 60);   // 可写
    connect(m_dataTable, &QTableWidget::cellDoubleClicked, this, &DataItemManualDialog::onTableCellDoubleClicked);
    connect(m_dataTable, &QTableWidget::currentCellChanged, this, &DataItemManualDialog::onTableCurrentCellChanged);
    splitter->addWidget(m_dataTable);

    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes(QList<int>() << 220 << 750);  // 左侧 220px，右侧 750px
    splitter->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    mainSplitter->addWidget(splitter);

    // 底部术语解释面板
    m_termExplanationWidget = new TermExplanationWidget(mainSplitter);
    m_termExplanationWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    mainSplitter->addWidget(m_termExplanationWidget);

    mainSplitter->setStretchFactor(0, 1);
    mainSplitter->setStretchFactor(1, 1);
    mainSplitter->setChildrenCollapsible(false);
    mainSplitter->setSizes(QList<int>() << 320 << 320);
    mainLayout->addWidget(mainSplitter, 1);
}

void DataItemManualDialog::loadDataItems()
{
    // 创建临时电表实例
    m_catalogMeterModel = new MeterModel(this);
    m_catalogMeterModel->initializeDefaultDataItems();

    // 提取所有数据项
    m_allDataItems = m_catalogMeterModel->getAllDataItems();
}

void DataItemManualDialog::populateTree()
{
    m_categoryTree->clear();

    // 统计各分类数量
    int totalCount = m_allDataItems.size();
    int energyCount = 0, demandCount = 0, variableCount = 0, eventCount = 0;
    int parameterCount = 0, freezeCount = 0, loadRecordCount = 0;

    for (const DataItem &item : m_allDataItems) {
        QString diCode = item.diCode;
        if (diCode.startsWith("0x00")) energyCount++;
        else if (diCode.startsWith("0x01")) demandCount++;
        else if (diCode.startsWith("0x02")) variableCount++;
        else if (diCode.startsWith("0x03")) eventCount++;
        else if (diCode.startsWith("0x04")) parameterCount++;
        else if (diCode.startsWith("0x05")) freezeCount++;
        else if (diCode.startsWith("0x06")) loadRecordCount++;
    }

    // 添加分类节点
    QTreeWidgetItem *allItem = new QTreeWidgetItem(m_categoryTree);
    allItem->setText(0, QString::fromUtf8("📁 全部 (%1)").arg(totalCount));
    allItem->setData(0, Qt::UserRole, QString::fromUtf8("全部"));

    QTreeWidgetItem *energyItem = new QTreeWidgetItem(m_categoryTree);
    energyItem->setText(0, QString::fromUtf8("📁 电能量 (%1)").arg(energyCount));
    energyItem->setData(0, Qt::UserRole, QString::fromUtf8("电能量"));

    QTreeWidgetItem *demandItem = new QTreeWidgetItem(m_categoryTree);
    demandItem->setText(0, QString::fromUtf8("📁 最大需量 (%1)").arg(demandCount));
    demandItem->setData(0, Qt::UserRole, QString::fromUtf8("最大需量"));

    QTreeWidgetItem *variableItem = new QTreeWidgetItem(m_categoryTree);
    variableItem->setText(0, QString::fromUtf8("📁 变量 (%1)").arg(variableCount));
    variableItem->setData(0, Qt::UserRole, QString::fromUtf8("变量"));

    QTreeWidgetItem *eventItem = new QTreeWidgetItem(m_categoryTree);
    eventItem->setText(0, QString::fromUtf8("📁 事件记录 (%1)").arg(eventCount));
    eventItem->setData(0, Qt::UserRole, QString::fromUtf8("事件记录"));

    QTreeWidgetItem *parameterItem = new QTreeWidgetItem(m_categoryTree);
    parameterItem->setText(0, QString::fromUtf8("📁 参变量 (%1)").arg(parameterCount));
    parameterItem->setData(0, Qt::UserRole, QString::fromUtf8("参变量"));

    QTreeWidgetItem *freezeItem = new QTreeWidgetItem(m_categoryTree);
    freezeItem->setText(0, QString::fromUtf8("📁 冻结 (%1)").arg(freezeCount));
    freezeItem->setData(0, Qt::UserRole, QString::fromUtf8("冻结"));

    QTreeWidgetItem *loadRecordItem = new QTreeWidgetItem(m_categoryTree);
    loadRecordItem->setText(0, QString::fromUtf8("📁 负荷记录 (%1)").arg(loadRecordCount));
    loadRecordItem->setData(0, Qt::UserRole, QString::fromUtf8("负荷记录"));

    // 默认选中"全部"
    m_categoryTree->setCurrentItem(allItem);
}

void DataItemManualDialog::populateTable(const QList<DataItem> &items)
{
    m_dataTable->blockSignals(true);
    m_dataTable->setRowCount(0);
    m_dataTable->setRowCount(items.size());

    for (int i = 0; i < items.size(); ++i) {
        const DataItem &item = items[i];

        // DI 码
        QTableWidgetItem *diItem = new QTableWidgetItem(item.diCode);
        m_dataTable->setItem(i, 0, diItem);

        // 描述
        QTableWidgetItem *descItem = new QTableWidgetItem(item.description);
        m_dataTable->setItem(i, 1, descItem);

        // 格式
        QTableWidgetItem *formatItem = new QTableWidgetItem(item.format);
        m_dataTable->setItem(i, 2, formatItem);

        // 单位
        QTableWidgetItem *unitItem = new QTableWidgetItem(item.unit);
        m_dataTable->setItem(i, 3, unitItem);

        // 可写
        QString writableText = item.isWritable ? QString::fromUtf8("✓") : QString::fromUtf8("✗");
        QTableWidgetItem *writableItem = new QTableWidgetItem(writableText);
        writableItem->setTextAlignment(Qt::AlignCenter);
        m_dataTable->setItem(i, 4, writableItem);
    }
    m_dataTable->blockSignals(false);
}

void DataItemManualDialog::filterTable(const QString &keyword)
{
    QList<DataItem> filtered;
    QString lowerKeyword = keyword.toLower();

    for (const DataItem &item : m_allDataItems) {
        // 分类过滤
        bool categoryMatch = true;
        if (m_currentCategory != QString::fromUtf8("全部")) {
            categoryMatch = false;
            if (m_currentCategory == QString::fromUtf8("电能量") && item.diCode.startsWith("0x00")) categoryMatch = true;
            else if (m_currentCategory == QString::fromUtf8("最大需量") && item.diCode.startsWith("0x01")) categoryMatch = true;
            else if (m_currentCategory == QString::fromUtf8("变量") && item.diCode.startsWith("0x02")) categoryMatch = true;
            else if (m_currentCategory == QString::fromUtf8("事件记录") && item.diCode.startsWith("0x03")) categoryMatch = true;
            else if (m_currentCategory == QString::fromUtf8("参变量") && item.diCode.startsWith("0x04")) categoryMatch = true;
            else if (m_currentCategory == QString::fromUtf8("冻结") && item.diCode.startsWith("0x05")) categoryMatch = true;
            else if (m_currentCategory == QString::fromUtf8("负荷记录") && item.diCode.startsWith("0x06")) categoryMatch = true;
        }

        // 关键词过滤
        bool keywordMatch = keyword.isEmpty() ||
                            item.diCode.toLower().contains(lowerKeyword) ||
                            item.description.toLower().contains(lowerKeyword);

        if (categoryMatch && keywordMatch) {
            filtered.append(item);
        }
    }

    populateTable(filtered);
}

QList<DataItem> DataItemManualDialog::getDataItemsByCategory(const QString &categoryName) const
{
    QList<DataItem> result;
    for (const DataItem &item : m_allDataItems) {
        bool match = false;
        if (categoryName == QString::fromUtf8("全部")) {
            match = true;
        } else if (categoryName == QString::fromUtf8("电能量") && item.diCode.startsWith("0x00")) {
            match = true;
        } else if (categoryName == QString::fromUtf8("最大需量") && item.diCode.startsWith("0x01")) {
            match = true;
        } else if (categoryName == QString::fromUtf8("变量") && item.diCode.startsWith("0x02")) {
            match = true;
        } else if (categoryName == QString::fromUtf8("事件记录") && item.diCode.startsWith("0x03")) {
            match = true;
        } else if (categoryName == QString::fromUtf8("参变量") && item.diCode.startsWith("0x04")) {
            match = true;
        } else if (categoryName == QString::fromUtf8("冻结") && item.diCode.startsWith("0x05")) {
            match = true;
        } else if (categoryName == QString::fromUtf8("负荷记录") && item.diCode.startsWith("0x06")) {
            match = true;
        }

        if (match) {
            result.append(item);
        }
    }
    return result;
}

void DataItemManualDialog::onCategorySelected(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column);
    if (!item) return;

    m_currentCategory = item->data(0, Qt::UserRole).toString();
    filterTable(m_searchBox->text());
}

void DataItemManualDialog::onSearchTextChanged(const QString &text)
{
    filterTable(text);

    DataItem item;
    if (findDataItemByDiCode(text, &item) && showSpecializedDataItemTerm(item)) {
        return;
    }
    if (m_dataTable->rowCount() == 1 && findDisplayedDataItem(0, &item) && showSpecializedDataItemTerm(item)) {
        m_dataTable->setCurrentCell(0, 0);
        return;
    }

    // 搜索匹配的术语
    TermDefinition term = TermCatalogService::instance()->searchTerm(text);

    qDebug() << "[TermSearch] Keyword:" << text << "| Found:" << (!term.term.isEmpty()) << "| Term:" << term.term;

    if (!term.term.isEmpty()) {
        m_termExplanationWidget->showTerm(term);
    } else {
        m_termExplanationWidget->clear();
    }
}

void DataItemManualDialog::onTableCurrentCellChanged(int currentRow, int currentColumn, int previousRow, int previousColumn)
{
    Q_UNUSED(currentColumn);
    Q_UNUSED(previousRow);
    Q_UNUSED(previousColumn);

    DataItem item;
    if (findDisplayedDataItem(currentRow, &item) && showSpecializedDataItemTerm(item)) {
        return;
    }
    if (m_searchBox->text().isEmpty()) {
        m_termExplanationWidget->clear();
    }
}

void DataItemManualDialog::onTableCellDoubleClicked(int row, int column)
{
    Q_UNUSED(column);
    if (row < 0 || row >= m_dataTable->rowCount()) return;

    // 获取 DI 码
    QTableWidgetItem *diItem = m_dataTable->item(row, 0);
    if (!diItem) return;

    QString diCode = diItem->text();
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(diCode);

    // 状态栏提示（临时）
    QMessageBox::information(this, QString::fromUtf8("已复制"),
                             QString::fromUtf8("DI 码 %1 已复制到剪贴板").arg(diCode));
}

bool DataItemManualDialog::isEnergyDataItem(const DataItem &item) const
{
    return item.diCode.startsWith(QStringLiteral("0x00"), Qt::CaseInsensitive);
}

bool DataItemManualDialog::isDemandDataItem(const DataItem &item) const
{
    return item.diCode.startsWith(QStringLiteral("0x01"), Qt::CaseInsensitive);
}

bool DataItemManualDialog::isVariableDataItem(const DataItem &item) const
{
    return item.diCode.startsWith(QStringLiteral("0x02"), Qt::CaseInsensitive);
}

bool DataItemManualDialog::isEventDataItem(const DataItem &item) const
{
    return item.diCode.startsWith(QStringLiteral("0x03"), Qt::CaseInsensitive);
}

bool DataItemManualDialog::isParameterDataItem(const DataItem &item) const
{
    return item.diCode.startsWith(QStringLiteral("0x04"), Qt::CaseInsensitive);
}

bool DataItemManualDialog::isFreezeDataItem(const DataItem &item) const
{
    return item.diCode.startsWith(QStringLiteral("0x05"), Qt::CaseInsensitive);
}

bool DataItemManualDialog::isLoadRecordDataItem(const DataItem &item) const
{
    return item.diCode.startsWith(QStringLiteral("0x06"), Qt::CaseInsensitive);
}

bool DataItemManualDialog::findDataItemByDiCode(const QString &diCode, DataItem *result) const
{
    const QString target = normalizedDiCode(diCode);
    if (target.isEmpty()) {
        return false;
    }

    for (const DataItem &item : m_allDataItems) {
        if (normalizedDiCode(item.diCode) == target) {
            if (result) {
                *result = item;
            }
            return true;
        }
    }

    return false;
}

bool DataItemManualDialog::findDisplayedDataItem(int row, DataItem *result) const
{
    if (row < 0 || row >= m_dataTable->rowCount()) {
        return false;
    }

    QTableWidgetItem *diItem = m_dataTable->item(row, 0);
    if (!diItem) {
        return false;
    }

    return findDataItemByDiCode(diItem->text(), result);
}

TermDefinition DataItemManualDialog::buildEnergyDataItemTerm(const DataItem &item) const
{
    TermDefinition term;
    const QString unit = item.unit.isEmpty() ? QString::fromUtf8("无") : item.unit;
    const QString writable = item.isWritable ? QString::fromUtf8("是") : QString::fromUtf8("否");
    QString description = item.description;
    description.remove(QString::fromUtf8("(当前)"));

    term.term = QString::fromUtf8("%1 %2").arg(item.diCode, item.description);
    term.englishTerm = QStringLiteral("DLT645 Energy Data Item");
    term.definition = energyDefinitionForDescription(item.description);
    term.formula = energyFormulaForDescription(item.description);
    term.unit = unit;
    term.range = QString::fromUtf8("累计值通常随用电过程递增；小数位数：%1").arg(item.decimalPlaces);
    term.note = QString::fromUtf8("DI：%1。可写：%2。协议数据格式：%3。")
        .arg(item.diCode, writable, item.format.isEmpty() ? QString::fromUtf8("未定义") : item.format);
    term.keywords = QStringList()
        << item.diCode
        << normalizedDiCode(item.diCode).mid(2)
        << item.description
        << description
        << QString::fromUtf8("电能量")
        << QString::fromUtf8("电能")
        << QStringLiteral("energy");

    return term;
}

TermDefinition DataItemManualDialog::buildDemandDataItemTerm(const DataItem &item) const
{
    TermDefinition term;
    const QString unit = item.unit.isEmpty() ? QStringLiteral("-") : item.unit;
    const QString writable = item.isWritable ? QStringLiteral("是") : QStringLiteral("否");
    QString description = item.description;
    description.remove(QStringLiteral("(当前)"));

    term.term = QStringLiteral("%1 %2").arg(item.diCode, item.description);
    term.englishTerm = QStringLiteral("DLT645 Demand Data Item");
    term.definition = demandDefinitionForDescription(item.description);
    term.formula = demandFormulaForDescription(item.description);
    term.unit = unit;

    if (item.description.contains(QStringLiteral("发生时间"))) {
        term.range = QStringLiteral("时间戳，按电表记录格式存储；小数位数：%1").arg(item.decimalPlaces);
    } else if (item.description.contains(QStringLiteral("数据块"))) {
        term.range = QStringLiteral("复合数据块，包含需量值与发生时间；小数位数：%1").arg(item.decimalPlaces);
    } else {
        term.range = QStringLiteral("最大平均功率记录值；小数位数：%1").arg(item.decimalPlaces);
    }

    term.note = QStringLiteral("DI：%1。可写：%2。协议数据格式：%3。")
        .arg(item.diCode, writable, item.format.isEmpty() ? QStringLiteral("未定义") : item.format);
    term.keywords = QStringList()
        << item.diCode
        << normalizedDiCode(item.diCode).mid(2)
        << item.description
        << description
        << QStringLiteral("最大需量")
        << QStringLiteral("需量")
        << QStringLiteral("demand");

    return term;
}

TermDefinition DataItemManualDialog::buildVariableDataItemTerm(const DataItem &item) const
{
    TermDefinition term;
    const QString unit = (item.unit.isEmpty() || item.unit == QStringLiteral("none")) ? QStringLiteral("-") : item.unit;
    const QString writable = item.isWritable ? QStringLiteral("是") : QStringLiteral("否");
    const QString description = item.description;

    term.term = QStringLiteral("%1 %2").arg(item.diCode, item.description);
    term.englishTerm = QStringLiteral("DLT645 Variable Data Item");
    term.definition = variableDefinitionForDescription(description);
    term.formula = variableFormulaForDescription(description);
    term.unit = unit;

    if (description.contains(QStringLiteral("数据块"))) {
        term.range = QStringLiteral("复合数据块，包含同组变量；小数位数：%1").arg(item.decimalPlaces);
    } else if (description.contains(QStringLiteral("谐波含量")) || description.contains(QStringLiteral("失真度")) || unit == QStringLiteral("%")) {
        term.range = QStringLiteral("百分比或比值型测量值；小数位数：%1").arg(item.decimalPlaces);
    } else if (description.contains(QStringLiteral("温度"))) {
        term.range = QStringLiteral("温度测量值；小数位数：%1").arg(item.decimalPlaces);
    } else if (description.contains(QStringLiteral("工作时间"))) {
        term.range = QStringLiteral("累计时长型状态值；小数位数：%1").arg(item.decimalPlaces);
    } else {
        term.range = QStringLiteral("实时测量或状态值；小数位数：%1").arg(item.decimalPlaces);
    }

    term.note = QStringLiteral("DI：%1。可写：%2。协议数据格式：%3。")
        .arg(item.diCode, writable, item.format.isEmpty() ? QStringLiteral("未定义") : item.format);
    term.keywords = QStringList()
        << item.diCode
        << normalizedDiCode(item.diCode).mid(2)
        << item.description
        << QStringLiteral("变量")
        << QStringLiteral("实时量")
        << QStringLiteral("variable");

    return term;
}

TermDefinition DataItemManualDialog::buildEventDataItemTerm(const DataItem &item) const
{
    TermDefinition term;
    const QString unit = item.unit.isEmpty() ? QStringLiteral("-") : item.unit;
    const QString writable = item.isWritable ? QStringLiteral("是") : QStringLiteral("否");
    const QString description = item.description;

    term.term = QStringLiteral("%1 %2").arg(item.diCode, item.description);
    term.englishTerm = QStringLiteral("DLT645 Event Data Item");
    term.definition = eventDefinitionForDescription(description);
    term.formula = eventFormulaForDescription(description);
    term.unit = unit;

    if (description.contains(QStringLiteral("总次数"))) {
        term.range = QStringLiteral("累计计数或累计时长类统计值；小数位数：%1").arg(item.decimalPlaces);
    } else if (description.contains(QStringLiteral("统计数据"))) {
        term.range = QStringLiteral("统计结果数据块；小数位数：%1").arg(item.decimalPlaces);
    } else if (description.contains(QStringLiteral("记录"))) {
        term.range = QStringLiteral("事件历史记录或快照结构；小数位数：%1").arg(item.decimalPlaces);
    } else {
        term.range = QStringLiteral("事件相关状态或结果值；小数位数：%1").arg(item.decimalPlaces);
    }

    term.note = QStringLiteral("DI：%1。可写：%2。协议数据格式：%3。")
        .arg(item.diCode, writable, item.format.isEmpty() ? QStringLiteral("未定义") : item.format);
    term.keywords = QStringList()
        << item.diCode
        << normalizedDiCode(item.diCode).mid(2)
        << item.description
        << QStringLiteral("事件记录")
        << QStringLiteral("事件")
        << QStringLiteral("event");

    return term;
}

TermDefinition DataItemManualDialog::buildParameterDataItemTerm(const DataItem &item) const
{
    TermDefinition term;
    const QString unit = (item.unit.isEmpty() || item.unit == QStringLiteral("-")) ? QStringLiteral("-") : item.unit;
    const QString writable = item.isWritable ? QStringLiteral("是") : QStringLiteral("否");
    const QString description = item.description;

    term.term = QStringLiteral("%1 %2").arg(item.diCode, item.description);
    term.englishTerm = QStringLiteral("DLT645 Parameter Data Item");
    term.definition = parameterDefinitionForDescription(description);
    term.formula = parameterFormulaForDescription(description);
    term.unit = unit;

    if (description.contains(QStringLiteral("数据")) || description.contains(QStringLiteral("数据块")) || item.format.contains(':')) {
        term.range = QStringLiteral("结构化配置数据；小数位数：%1").arg(item.decimalPlaces);
    } else if (description.contains(QStringLiteral("时间")) || description.contains(QStringLiteral("日期")) || description.contains(QStringLiteral("结算日"))) {
        term.range = QStringLiteral("时间/日期型配置值；小数位数：%1").arg(item.decimalPlaces);
    } else if (description.contains(QStringLiteral("特征字")) || description.contains(QStringLiteral("状态字")) || description.contains(QStringLiteral("模式字"))) {
        term.range = QStringLiteral("编码或位标志型配置值；小数位数：%1").arg(item.decimalPlaces);
    } else {
        term.range = QStringLiteral("可配置参数值；小数位数：%1").arg(item.decimalPlaces);
    }

    term.note = QStringLiteral("DI：%1。可写：%2。协议数据格式：%3。")
        .arg(item.diCode, writable, item.format.isEmpty() ? QStringLiteral("未定义") : item.format);
    term.keywords = QStringList()
        << item.diCode
        << normalizedDiCode(item.diCode).mid(2)
        << item.description
        << QStringLiteral("参变量")
        << QStringLiteral("参数")
        << QStringLiteral("parameter");

    return term;
}

TermDefinition DataItemManualDialog::buildFreezeDataItemTerm(const DataItem &item) const
{
    TermDefinition term;
    const QString unit = item.unit.isEmpty() ? QStringLiteral("-") : item.unit;
    const QString writable = item.isWritable ? QStringLiteral("是") : QStringLiteral("否");
    const QString description = item.description;

    term.term = QStringLiteral("%1 %2").arg(item.diCode, item.description);
    term.englishTerm = QStringLiteral("DLT645 Freeze Data Item");
    term.definition = freezeDefinitionForDescription(description);
    term.formula = freezeFormulaForDescription(description);
    term.unit = unit;

    if (description.contains(QStringLiteral("时间"))) {
        term.range = QStringLiteral("冻结时刻或发生时刻型历史值；小数位数：%1").arg(item.decimalPlaces);
    } else if (description.contains(QStringLiteral("数据块")) || item.format.contains(':')) {
        term.range = QStringLiteral("复合冻结数据块；小数位数：%1").arg(item.decimalPlaces);
    } else if (description.contains(QStringLiteral("最大需量"))) {
        term.range = QStringLiteral("冻结快照中的需量或发生时间值；小数位数：%1").arg(item.decimalPlaces);
    } else if (description.contains(QStringLiteral("变量"))) {
        term.range = QStringLiteral("冻结快照中的变量历史值；小数位数：%1").arg(item.decimalPlaces);
    } else {
        term.range = QStringLiteral("冻结快照中的累计值或状态值；小数位数：%1").arg(item.decimalPlaces);
    }

    term.note = QStringLiteral("DI：%1。可写：%2。协议数据格式：%3。")
        .arg(item.diCode, writable, item.format.isEmpty() ? QStringLiteral("未定义") : item.format);
    term.keywords = QStringList()
        << item.diCode
        << normalizedDiCode(item.diCode).mid(2)
        << item.description
        << QStringLiteral("冻结")
        << QStringLiteral("冻结数据")
        << QStringLiteral("freeze");

    return term;
}

TermDefinition DataItemManualDialog::buildLoadRecordDataItemTerm(const DataItem &item) const
{
    TermDefinition term;
    const QString unit = item.unit.isEmpty() ? QStringLiteral("-") : item.unit;
    const QString writable = item.isWritable ? QStringLiteral("是") : QStringLiteral("否");
    const QString description = item.description;

    term.term = QStringLiteral("%1 %2").arg(item.diCode, item.description);
    term.englishTerm = QStringLiteral("DLT645 Load Record Data Item");
    term.definition = loadRecordDefinitionForDescription(description);
    term.formula = loadRecordFormulaForDescription(description);
    term.unit = unit;

    if (description.contains(QStringLiteral("单位代码")) || item.format == QStringLiteral("AA,AA")) {
        term.range = QStringLiteral("控制参数复合编码值；小数位数：%1").arg(item.decimalPlaces);
    } else {
        term.range = QStringLiteral("控制类负荷记录参数值；小数位数：%1").arg(item.decimalPlaces);
    }

    term.note = QStringLiteral("DI：%1。可写：%2。协议数据格式：%3。")
        .arg(item.diCode, writable, item.format.isEmpty() ? QStringLiteral("未定义") : item.format);
    term.keywords = QStringList()
        << item.diCode
        << normalizedDiCode(item.diCode).mid(2)
        << item.description
        << QStringLiteral("负荷记录")
        << QStringLiteral("控制")
        << QStringLiteral("load record");

    return term;
}

bool DataItemManualDialog::showEnergyDataItemTerm(const DataItem &item)
{
    if (!isEnergyDataItem(item)) {
        return false;
    }

    m_termExplanationWidget->showTerm(buildEnergyDataItemTerm(item));
    return true;
}

bool DataItemManualDialog::showDemandDataItemTerm(const DataItem &item)
{
    if (!isDemandDataItem(item)) {
        return false;
    }

    m_termExplanationWidget->showTerm(buildDemandDataItemTerm(item));
    return true;
}

bool DataItemManualDialog::showVariableDataItemTerm(const DataItem &item)
{
    if (!isVariableDataItem(item)) {
        return false;
    }

    m_termExplanationWidget->showTerm(buildVariableDataItemTerm(item));
    return true;
}

bool DataItemManualDialog::showEventDataItemTerm(const DataItem &item)
{
    if (!isEventDataItem(item)) {
        return false;
    }

    m_termExplanationWidget->showTerm(buildEventDataItemTerm(item));
    return true;
}

bool DataItemManualDialog::showParameterDataItemTerm(const DataItem &item)
{
    if (!isParameterDataItem(item)) {
        return false;
    }

    m_termExplanationWidget->showTerm(buildParameterDataItemTerm(item));
    return true;
}

bool DataItemManualDialog::showFreezeDataItemTerm(const DataItem &item)
{
    if (!isFreezeDataItem(item)) {
        return false;
    }

    m_termExplanationWidget->showTerm(buildFreezeDataItemTerm(item));
    return true;
}

bool DataItemManualDialog::showLoadRecordDataItemTerm(const DataItem &item)
{
    if (!isLoadRecordDataItem(item)) {
        return false;
    }

    m_termExplanationWidget->showTerm(buildLoadRecordDataItemTerm(item));
    return true;
}

bool DataItemManualDialog::showSpecializedDataItemTerm(const DataItem &item)
{
    return showEnergyDataItemTerm(item) ||
           showDemandDataItemTerm(item) ||
           showVariableDataItemTerm(item) ||
           showEventDataItemTerm(item) ||
           showParameterDataItemTerm(item) ||
           showFreezeDataItemTerm(item) ||
           showLoadRecordDataItemTerm(item);
}

namespace
{
// 数据项手册对话框 QSS 模板。占位符由全局调色板（Utils/UiTheme）替换。
// 操作按钮采用实心强调色（accent / accentHover / accentPressed）。
QString dataItemManualStyleTemplate()
{
    return QStringLiteral(R"(
        QDialog {
            background-color: {{windowBg}};
            color: {{text}};
        }

        QLabel {
            color: {{text}};
            background-color: transparent;
            font-family: "Microsoft YaHei";
        }

        QTreeWidget {
            background-color: {{panelBg}};
            color: {{text}};
            border: 1px solid {{border}};
            border-radius: 4px;
        }

        QTreeWidget::item:selected {
            background-color: {{accent}};
            color: {{onAccent}};
        }

        QTreeWidget::item:hover {
            background-color: {{panelHover}};
        }

        QTreeWidget::item:selected:hover {
            background-color: {{accentHover}};
            color: {{onAccent}};
        }

        QTableWidget {
            background-color: {{windowBg}};
            color: {{text}};
            gridline-color: {{gridLine}};
            border: 1px solid {{border}};
            border-radius: 4px;
        }

        QTableWidget::item {
            padding: 6px 8px;
            color: {{text}};
            background-color: {{windowBg}};
        }

        QTableWidget::item:selected {
            background-color: {{accent}};
            color: {{onAccent}};
        }

        QTableWidget::item:hover {
            background-color: {{panelBg}};
            color: {{text}};
        }

        QTableWidget::item:selected:hover {
            background-color: {{accentHover}};
            color: {{onAccent}};
        }

        QHeaderView::section {
            background-color: {{panelBg}};
            color: {{text}};
            border: 1px solid {{border}};
            padding: 5px;
            font-weight: bold;
        }

        QLineEdit {
            background-color: {{panelBg}};
            color: {{text}};
            border: 1px solid {{border}};
            border-radius: 4px;
            padding: 5px;
        }

        QLineEdit:focus {
            border: 1px solid {{accent}};
        }

        QPushButton {
            background-color: {{accent}};
            color: {{onAccent}};
            border: none;
            border-radius: 4px;
            padding: 6px 14px;
        }

        QPushButton:hover {
            background-color: {{accentHover}};
        }

        QPushButton:pressed {
            background-color: {{accentPressed}};
        }
    )");
}
} // namespace

void DataItemManualDialog::applyDarkTheme()
{
    setStyleSheet(ui::applyThemeTokens(dataItemManualStyleTemplate(), ui::darkPalette()));

    // 同步主题到术语面板
    if (m_termExplanationWidget) {
        m_termExplanationWidget->applyDarkTheme();
    }
}

void DataItemManualDialog::applyLightTheme()
{
    setStyleSheet(ui::applyThemeTokens(dataItemManualStyleTemplate(), ui::lightPalette()));

    // 同步主题到术语面板
    if (m_termExplanationWidget) {
        m_termExplanationWidget->applyLightTheme();
    }
}
