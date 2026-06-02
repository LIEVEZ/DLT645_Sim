#include "MasterQueryDialog.h"

#include "Models/DataItem.h"
#include "Models/MeterModel.h"
#include "Utils/DLT645FrameFormatter.h"
#include "Views/Common/FrameParseResultWidget.h"

#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QFormLayout>
#include <QFont>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QSignalBlocker>
#include <QSplitter>
#include <QSpinBox>
#include <QStyle>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QtConcurrent/QtConcurrent>
#include <QVBoxLayout>

namespace
{
QSerialPort::DataBits dataBitsFromText(const QString &text)
{
    if (text == QStringLiteral("5"))
    {
        return QSerialPort::Data5;
    }
    if (text == QStringLiteral("6"))
    {
        return QSerialPort::Data6;
    }
    if (text == QStringLiteral("7"))
    {
        return QSerialPort::Data7;
    }
    return QSerialPort::Data8;
}

QSerialPort::Parity parityFromText(const QString &text)
{
    if (text == QStringLiteral("None"))
    {
        return QSerialPort::NoParity;
    }
    if (text == QStringLiteral("Odd"))
    {
        return QSerialPort::OddParity;
    }
    if (text == QStringLiteral("Mark"))
    {
        return QSerialPort::MarkParity;
    }
    if (text == QStringLiteral("Space"))
    {
        return QSerialPort::SpaceParity;
    }
    return QSerialPort::EvenParity;
}

QSerialPort::StopBits stopBitsFromText(const QString &text)
{
    if (text == QStringLiteral("1.5"))
    {
        return QSerialPort::OneAndHalfStop;
    }
    if (text == QStringLiteral("2"))
    {
        return QSerialPort::TwoStop;
    }
    return QSerialPort::OneStop;
}

QSerialPort::FlowControl flowControlFromText(const QString &text)
{
    if (text == QStringLiteral("Hardware"))
    {
        return QSerialPort::HardwareControl;
    }
    if (text == QStringLiteral("Software"))
    {
        return QSerialPort::SoftwareControl;
    }
    return QSerialPort::NoFlowControl;
}

QString resultSummary(const MasterQueryResult &result)
{
    if (!result.errorText.isEmpty())
    {
        return result.errorText;
    }

    if (!result.rawResponse.isEmpty())
    {
        return DLT645FrameFormatter::formatHex(result.rawResponse);
    }

    return QString();
}

struct CategorySelection
{
    QString rootCategory;
    QString leafCategory;
};

struct EventCategoryDefinition
{
    QString name;
    QStringList prefixes;
};

struct ParameterCategoryDefinition
{
    QString name;
    QStringList prefixes;
};

CategorySelection parseCategorySelection(const QString &category)
{
    const int separatorIndex = category.indexOf('/');
    if (separatorIndex < 0)
    {
        return {category, QString()};
    }

    return {category.left(separatorIndex), category.mid(separatorIndex + 1)};
}

int settlementDayFromCategoryName(const QString &categoryName)
{
    if (categoryName == QStringLiteral("当前"))
    {
        return 0;
    }

    static const QRegularExpression settlementDayPattern(QStringLiteral("^上(\\d+)结算日$"));
    const QRegularExpressionMatch match = settlementDayPattern.match(categoryName);
    if (match.hasMatch())
    {
        return match.captured(1).toInt();
    }

    return -1;
}

QList<QString> settlementDayCategories(const QString &rootCategory)
{
    QList<QString> categories;
    categories.reserve(13);
    categories.append(QStringLiteral("%1/当前").arg(rootCategory));
    for (int settlementDay = 1; settlementDay <= 12; ++settlementDay)
    {
        categories.append(QStringLiteral("%1/上%2结算日").arg(rootCategory).arg(settlementDay));
    }

    return categories;
}

QStringList eventCategoryNames()
{
    return {
        QStringLiteral("失压"),
        QStringLiteral("欠压"),
        QStringLiteral("过压"),
        QStringLiteral("断相"),
        QStringLiteral("全失压"),
        QStringLiteral("辅助电源失电"),
        QStringLiteral("电压逆相序"),
        QStringLiteral("电流逆相序"),
        QStringLiteral("电压不平衡"),
        QStringLiteral("电流不平衡"),
        QStringLiteral("失流"),
        QStringLiteral("过流"),
        QStringLiteral("断流"),
        QStringLiteral("潮流反向"),
        QStringLiteral("过载"),
        QStringLiteral("电压合格率"),
        QStringLiteral("掉电"),
        QStringLiteral("需量超限"),
        QStringLiteral("编程记录"),
        QStringLiteral("电表清零"),
        QStringLiteral("需量清零"),
        QStringLiteral("事件清零"),
        QStringLiteral("校时"),
        QStringLiteral("时段表编程"),
        QStringLiteral("时区表编程"),
        QStringLiteral("周休日编程"),
        QStringLiteral("节假日编程"),
        QStringLiteral("有功组合方式编程"),
        QStringLiteral("无功组合方式1编程"),
        QStringLiteral("无功组合方式2编程"),
        QStringLiteral("结算日编程"),
        QStringLiteral("开表盖"),
        QStringLiteral("开端钮盒"),
        QStringLiteral("购电量记录"),
        QStringLiteral("购电金额记录")};
}

QList<EventCategoryDefinition> eventCategoryDefinitions()
{
    return {
        {QStringLiteral("失压"), {QStringLiteral("0x0301")}},
        {QStringLiteral("欠压"), {QStringLiteral("0x0302")}},
        {QStringLiteral("过压"), {QStringLiteral("0x0303")}},
        {QStringLiteral("断相"), {QStringLiteral("0x0304")}},
        {QStringLiteral("全失压"), {QStringLiteral("0x0305")}},
        {QStringLiteral("辅助电源失电"), {QStringLiteral("0x0306")}},
        {QStringLiteral("电压逆相序"), {QStringLiteral("0x0307")}},
        {QStringLiteral("电流逆相序"), {QStringLiteral("0x0308")}},
        {QStringLiteral("电压不平衡"), {QStringLiteral("0x0309")}},
        {QStringLiteral("电流不平衡"), {QStringLiteral("0x030A")}},
        {QStringLiteral("失流"), {QStringLiteral("0x030B")}},
        {QStringLiteral("过流"), {QStringLiteral("0x030C")}},
        {QStringLiteral("断流"), {QStringLiteral("0x030D")}},
        {QStringLiteral("潮流反向"), {QStringLiteral("0x030E")}},
        {QStringLiteral("过载"), {QStringLiteral("0x030F")}},
        {QStringLiteral("电压合格率"), {QStringLiteral("0x0310")}},
        {QStringLiteral("掉电"), {QStringLiteral("0x0311")}},
        {QStringLiteral("需量超限"), {QStringLiteral("0x0312")}},
        {QStringLiteral("编程记录"), {QStringLiteral("0x033000")}},
        {QStringLiteral("电表清零"), {QStringLiteral("0x033001")}},
        {QStringLiteral("需量清零"), {QStringLiteral("0x033002")}},
        {QStringLiteral("事件清零"), {QStringLiteral("0x033003")}},
        {QStringLiteral("校时"), {QStringLiteral("0x033004")}},
        {QStringLiteral("时段表编程"), {QStringLiteral("0x033005")}},
        {QStringLiteral("时区表编程"), {QStringLiteral("0x033006")}},
        {QStringLiteral("周休日编程"), {QStringLiteral("0x033007")}},
        {QStringLiteral("节假日编程"), {QStringLiteral("0x033008")}},
        {QStringLiteral("有功组合方式编程"), {QStringLiteral("0x033009")}},
        {QStringLiteral("无功组合方式1编程"), {QStringLiteral("0x03300A")}},
        {QStringLiteral("无功组合方式2编程"), {QStringLiteral("0x03300B")}},
        {QStringLiteral("结算日编程"), {QStringLiteral("0x03300C")}},
        {QStringLiteral("开表盖"), {QStringLiteral("0x03300D")}},
        {QStringLiteral("开端钮盒"), {QStringLiteral("0x03300E")}},
        {QStringLiteral("购电量记录"), {QStringLiteral("0x0332")}},
        {QStringLiteral("购电金额记录"), {QStringLiteral("0x0333")}}};
}

QStringList parameterCategoryNames()
{
    return {
        QStringLiteral("基本参数"),
        QStringLiteral("第一套时区时段"),
        QStringLiteral("第二套时区时段"),
        QStringLiteral("公共假日"),
        QStringLiteral("显示参数"),
        QStringLiteral("费率电价"),
        QStringLiteral("阶梯电价"),
        QStringLiteral("事件参数")};
}

QList<ParameterCategoryDefinition> parameterCategoryDefinitions()
{
    return {
        {QStringLiteral("基本参数"), {QStringLiteral("0x0400")}},
        {QStringLiteral("第一套时区时段"), {QStringLiteral("0x0401")}},
        {QStringLiteral("第二套时区时段"), {QStringLiteral("0x0402")}},
        {QStringLiteral("公共假日"), {QStringLiteral("0x0403")}},
        {QStringLiteral("显示参数"), {QStringLiteral("0x0404")}},
        {QStringLiteral("费率电价"), {QStringLiteral("0x040501"), QStringLiteral("0x040502")}},
        {QStringLiteral("阶梯电价"), {QStringLiteral("0x040600"), QStringLiteral("0x040601"), QStringLiteral("0x040602"), QStringLiteral("0x040603")}},
        {QStringLiteral("事件参数"), {QStringLiteral("0x0409")}}};
}

QList<QString> filterCodesByPrefixes(const QList<QString> &allCodes, const QStringList &prefixes)
{
    if (prefixes.isEmpty())
    {
        return allCodes;
    }

    QList<QString> filteredCodes;
    filteredCodes.reserve(allCodes.size());
    for (const QString &diCode : allCodes)
    {
        for (const QString &prefix : prefixes)
        {
            if (diCode.startsWith(prefix))
            {
                filteredCodes.append(diCode);
                break;
            }
        }
    }

    return filteredCodes;
}

QList<QString> filterEventCodesByCategory(const MeterModel *meterModel, const QList<QString> &allCodes, const QString &categoryName)
{
    QStringList matchedPrefixes;
    for (const EventCategoryDefinition &definition : eventCategoryDefinitions())
    {
        if (definition.name == categoryName)
        {
            matchedPrefixes = definition.prefixes;
            break;
        }
    }

    QList<QString> filteredCodes = filterCodesByPrefixes(allCodes, matchedPrefixes);
    if (!filteredCodes.isEmpty() || matchedPrefixes.isEmpty() || !meterModel)
    {
        return filteredCodes;
    }

    for (const QString &diCode : allCodes)
    {
        const DataItem dataItem = meterModel->getDataItem(diCode);
        if (dataItem.description.contains(categoryName, Qt::CaseInsensitive))
        {
            filteredCodes.append(diCode);
        }
    }

    return filteredCodes;
}

QList<QString> filterParameterCodesByCategory(const QList<QString> &allCodes, const QString &categoryName)
{
    QStringList matchedPrefixes;
    for (const ParameterCategoryDefinition &definition : parameterCategoryDefinitions())
    {
        if (definition.name == categoryName)
        {
            matchedPrefixes = definition.prefixes;
            break;
        }
    }

    return filterCodesByPrefixes(allCodes, matchedPrefixes);
}
}

MasterQueryDialog::MasterQueryDialog(QWidget *parent)
    : QDialog(parent),
      m_controller(new MasterQueryController(this)),
      m_catalogMeterModel(new MeterModel(this)),
      m_portRefreshWatcher(new QFutureWatcher<QList<QSerialPortInfo>>(this)),
      m_darkTheme(true),
      m_portRefreshPending(false),
      m_pendingHistoryRow(-1),
      m_portCombo(nullptr),
      m_baudRateCombo(nullptr),
      m_dataBitsCombo(nullptr),
      m_parityCombo(nullptr),
      m_stopBitsCombo(nullptr),
      m_flowControlCombo(nullptr),
      m_refreshPortsButton(nullptr),
      m_connectButton(nullptr),
      m_disconnectButton(nullptr),
      m_connectionStatusLabel(nullptr),
      m_portHintLabel(nullptr),
      m_queryTypeCombo(nullptr),
      m_addressEdit(nullptr),
      m_includePrefixCheckBox(nullptr),
      m_timeoutSpinBox(nullptr),
      m_searchEdit(nullptr),
      m_categoryComboBox(nullptr),
      m_dataItemList(nullptr),
      m_selectedDiLabel(nullptr),
      m_sendButton(nullptr),
      m_resultMetaLabel(nullptr),
      m_noticeLabel(nullptr),
      m_requestHexEdit(nullptr),
      m_responseHexEdit(nullptr),
      m_parseResultWidget(nullptr),
      m_historyTable(nullptr),
      m_clearHistoryButton(nullptr)
{
    connect(m_portRefreshWatcher, &QFutureWatcher<QList<QSerialPortInfo>>::finished, this, [this]()
            {
                m_cachedPorts = m_portRefreshWatcher->result();
                refreshPorts();

                if (m_portRefreshPending)
                {
                    m_portRefreshPending = false;
                    requestPortRefresh();
                } });

    setupUi();
    populateCategories();
    populateDataItems();
    requestPortRefresh();
    applyDarkTheme();

    setWindowTitle(QString::fromUtf8("DLT645 主站查询"));
    setMinimumSize(1180, 760);
    resize(1280, 860);

    connect(m_controller, &MasterQueryController::sessionStateChanged,
            this, &MasterQueryDialog::onSessionStateChanged);
    connect(m_controller, &MasterQueryController::requestStarted,
            this, &MasterQueryDialog::onRequestStarted);
    connect(m_controller, &MasterQueryController::responseReceived,
            this, &MasterQueryDialog::onResponseReceived);
    connect(m_controller, &MasterQueryController::requestFailed,
            this, &MasterQueryDialog::onRequestFailed);
    connect(m_controller, &MasterQueryController::noticeGenerated,
            this, &MasterQueryDialog::onNoticeGenerated);
}

MasterQueryDialog::~MasterQueryDialog()
{
}

void MasterQueryDialog::setUnavailablePorts(const QStringList &ports)
{
    m_unavailablePorts = ports;
    if (!m_portRefreshWatcher->isRunning() || !m_cachedPorts.isEmpty())
    {
        refreshPorts();
    }
}

void MasterQueryDialog::setDefaultMeterAddress(const QString &address)
{
    if (m_addressEdit && m_addressEdit->text() != address)
    {
        m_addressEdit->setText(address);
    }
}

void MasterQueryDialog::setDefaultIncludePrefixFE(bool includePrefixFE)
{
    if (m_includePrefixCheckBox && m_includePrefixCheckBox->isChecked() != includePrefixFE)
    {
        m_includePrefixCheckBox->setChecked(includePrefixFE);
    }
}

void MasterQueryDialog::applyDarkTheme()
{
    m_darkTheme = true;
    setStyleSheet(QStringLiteral(R"(
        QDialog {
            background-color: #2B2B2B;
        }

        QGroupBox {
            color: #FFFFFF;
            border: 1px solid #555555;
            border-radius: 6px;
            margin-top: 10px;
            padding-top: 10px;
            font-weight: 600;
        }

        QGroupBox::title {
            subcontrol-origin: margin;
            left: 12px;
            padding: 0 6px;
            color: #19D3AE;
        }

        QLabel {
            color: #FFFFFF;
            font-family: "Microsoft YaHei";
        }

        QLabel#masterStatusLabel[state="connected"] {
            color: #22C55E;
        }

        QLabel#masterStatusLabel[state="disconnected"] {
            color: #F87171;
        }

        QLabel#masterNoticeLabel {
            color: #FCD34D;
        }

        QLineEdit, QListWidget, QComboBox, QSpinBox, QTableWidget {
            background-color: #3C3F41;
            color: #FFFFFF;
            border: 1px solid #555555;
            border-radius: 4px;
            padding: 5px;
        }

        QLineEdit:focus, QListWidget:focus, QComboBox:focus, QSpinBox:focus {
            border: 1px solid #094771;
        }

        QLineEdit[readOnly="true"] {
            background-color: #35383A;
            color: #C9D1D9;
        }

        QComboBox QAbstractItemView {
            background-color: #3C3F41;
            color: #FFFFFF;
            selection-background-color: #094771;
        }

        QListWidget::item {
            padding: 6px 4px;
            border-bottom: 1px solid #444444;
        }

        QListWidget::item:selected {
            background-color: #094771;
            color: #FFFFFF;
        }

        QCheckBox {
            color: #FFFFFF;
            spacing: 4px;
        }

        QCheckBox::indicator {
            width: 16px;
            height: 16px;
            border: 1px solid #555555;
            border-radius: 3px;
            background-color: #3C3F41;
        }

        QCheckBox::indicator:checked {
            background-color: #094771;
            border: 1px solid #094771;
        }

        QPushButton {
            background-color: #3C3F41;
            color: #FFFFFF;
            border: 1px solid #555555;
            border-radius: 4px;
            padding: 6px 14px;
        }

        QPushButton:hover {
            background-color: #4A4E51;
        }

        QPushButton:disabled {
            color: #888888;
        }

        QTableWidget {
            gridline-color: #444444;
        }

        QHeaderView::section {
            background-color: #35383A;
            color: #FFFFFF;
            border: none;
            border-bottom: 1px solid #555555;
            padding: 6px;
        }
    )"));

    if (m_parseResultWidget)
    {
        m_parseResultWidget->applyDarkTheme();
    }
}

void MasterQueryDialog::applyLightTheme()
{
    m_darkTheme = false;
    setStyleSheet(QStringLiteral(R"(
        QDialog {
            background-color: #FFFFFF;
        }

        QGroupBox {
            color: #000000;
            border: 1px solid #CCCCCC;
            border-radius: 6px;
            margin-top: 10px;
            padding-top: 10px;
            font-weight: 600;
        }

        QGroupBox::title {
            subcontrol-origin: margin;
            left: 12px;
            padding: 0 6px;
            color: #007A66;
        }

        QLabel {
            color: #000000;
            font-family: "Microsoft YaHei";
        }

        QLabel#masterStatusLabel[state="connected"] {
            color: #15803D;
        }

        QLabel#masterStatusLabel[state="disconnected"] {
            color: #DC2626;
        }

        QLabel#masterNoticeLabel {
            color: #B45309;
        }

        QLineEdit, QListWidget, QComboBox, QSpinBox, QTableWidget {
            background-color: #FFFFFF;
            color: #000000;
            border: 1px solid #CCCCCC;
            border-radius: 4px;
            padding: 5px;
        }

        QLineEdit:focus, QListWidget:focus, QComboBox:focus, QSpinBox:focus {
            border: 1px solid #0078D7;
        }

        QLineEdit[readOnly="true"] {
            background-color: #F6F6F6;
            color: #444444;
        }

        QComboBox QAbstractItemView {
            background-color: #FFFFFF;
            color: #000000;
            selection-background-color: #0078D7;
        }

        QListWidget::item {
            padding: 6px 4px;
            border-bottom: 1px solid #E5E7EB;
        }

        QListWidget::item:selected {
            background-color: #0078D7;
            color: #FFFFFF;
        }

        QCheckBox {
            color: #000000;
            spacing: 4px;
        }

        QCheckBox::indicator {
            width: 16px;
            height: 16px;
            border: 1px solid #CCCCCC;
            border-radius: 3px;
            background-color: #FFFFFF;
        }

        QCheckBox::indicator:checked {
            background-color: #0078D7;
            border: 1px solid #0078D7;
        }

        QPushButton {
            background-color: #F0F0F0;
            color: #000000;
            border: 1px solid #CCCCCC;
            border-radius: 4px;
            padding: 6px 14px;
        }

        QPushButton:hover {
            background-color: #E4E4E4;
        }

        QPushButton:disabled {
            color: #999999;
        }

        QTableWidget {
            gridline-color: #E5E7EB;
        }

        QHeaderView::section {
            background-color: #F6F6F6;
            color: #000000;
            border: none;
            border-bottom: 1px solid #CCCCCC;
            padding: 6px;
        }
    )"));

    if (m_parseResultWidget)
    {
        m_parseResultWidget->applyLightTheme();
    }
}

void MasterQueryDialog::onRefreshPortsClicked()
{
    requestPortRefresh();
}

void MasterQueryDialog::onConnectClicked()
{
    const QString portName = currentPortName();
    if (portName.isEmpty())
    {
        QMessageBox::warning(this, QString::fromUtf8("提示"), QString::fromUtf8("请选择一个串口。"));
        return;
    }

    if (m_unavailablePorts.contains(portName))
    {
        QMessageBox::warning(this,
                             QString::fromUtf8("串口占用"),
                             QString::fromUtf8("该串口当前已被模拟器链路占用，请先关闭模拟器连接。"));
        return;
    }

    if (!m_controller->openPort(currentSerialConfig()))
    {
        QMessageBox::critical(this,
                              QString::fromUtf8("连接失败"),
                              QString::fromUtf8("串口打开失败，请检查端口是否被占用或参数是否正确。"));
    }
}

void MasterQueryDialog::onDisconnectClicked()
{
    m_controller->closePort();
}

void MasterQueryDialog::onQueryTypeChanged(int index)
{
    Q_UNUSED(index);
    updateQueryUiState();
}

void MasterQueryDialog::onCategoryChanged(int index)
{
    Q_UNUSED(index);
    populateDataItems();
}

void MasterQueryDialog::onSearchTextChanged(const QString &text)
{
    Q_UNUSED(text);
    populateDataItems();
}

void MasterQueryDialog::onCurrentItemChanged(QListWidgetItem *current, QListWidgetItem *previous)
{
    Q_UNUSED(previous);
    if (!current)
    {
        m_selectedDiLabel->setText(QString::fromUtf8("当前未选择数据标识"));
    }
    else
    {
        m_selectedDiLabel->setText(QString::fromUtf8("当前选择：%1").arg(current->text()));
    }
    updateQueryUiState();
}

void MasterQueryDialog::onSendClicked()
{
    if (!m_controller->isConnected())
    {
        QMessageBox::warning(this, QString::fromUtf8("提示"), QString::fromUtf8("请先连接串口。"));
        return;
    }

    if (m_queryTypeCombo->currentIndex() == 0 && selectedDiCode().isEmpty())
    {
        QMessageBox::warning(this, QString::fromUtf8("提示"), QString::fromUtf8("请选择要读取的数据标识。"));
        return;
    }

    m_noticeLabel->clear();
    m_controller->sendReadRequest(currentRequestSpec());
}

void MasterQueryDialog::onClearHistoryClicked()
{
    m_historyResults.clear();
    m_historyTable->setRowCount(0);
    m_pendingHistoryRow = -1;
    m_requestHexEdit->clear();
    m_responseHexEdit->clear();
    m_resultMetaLabel->clear();
    m_noticeLabel->clear();
    m_parseResultWidget->clearResult();
}

void MasterQueryDialog::onHistorySelectionChanged()
{
    const int row = m_historyTable->currentRow();
    if (row < 0 || row >= m_historyResults.size())
    {
        return;
    }

    displayResult(m_historyResults.at(row));
}

void MasterQueryDialog::onSessionStateChanged(bool connected)
{
    applyConnectionUiState(connected);
    if (connected)
    {
        m_noticeLabel->setText(QString::fromUtf8("串口已连接，可以发送查询请求。"));
    }
    else
    {
        m_noticeLabel->setText(QString::fromUtf8("串口未连接。"));
    }
}

void MasterQueryDialog::onRequestStarted(const MasterQueryResult &result)
{
    displayResult(result);
    m_pendingHistoryRow = appendHistoryRow(result);
}

void MasterQueryDialog::onResponseReceived(const MasterQueryResult &result)
{
    displayResult(result);

    if (m_pendingHistoryRow >= 0 && m_pendingHistoryRow < m_historyResults.size())
    {
        m_historyResults[m_pendingHistoryRow] = result;
        updateHistoryRow(m_pendingHistoryRow, result);
        m_historyTable->selectRow(m_pendingHistoryRow);
    }

    m_pendingHistoryRow = -1;
}

void MasterQueryDialog::onRequestFailed(const MasterQueryResult &result)
{
    displayResult(result);

    if (m_pendingHistoryRow >= 0 && m_pendingHistoryRow < m_historyResults.size())
    {
        m_historyResults[m_pendingHistoryRow] = result;
        updateHistoryRow(m_pendingHistoryRow, result);
        m_historyTable->selectRow(m_pendingHistoryRow);
    }
    else
    {
        appendHistoryRow(result);
    }

    m_pendingHistoryRow = -1;
}

void MasterQueryDialog::onNoticeGenerated(const QString &message)
{
    m_noticeLabel->setText(message);
}

void MasterQueryDialog::closeEvent(QCloseEvent *event)
{
    m_controller->closePort();
    emit dialogClosed();
    QDialog::closeEvent(event);
}

void MasterQueryDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(10);

    mainLayout->addWidget(setupConnectionGroup());

    QSplitter *contentSplitter = new QSplitter(Qt::Horizontal, this);
    contentSplitter->setChildrenCollapsible(false);

    QSplitter *rightSplitter = new QSplitter(Qt::Vertical, contentSplitter);
    rightSplitter->setChildrenCollapsible(false);

    contentSplitter->addWidget(setupQueryGroup());
    rightSplitter->addWidget(setupResultGroup());
    rightSplitter->addWidget(setupHistoryGroup());
    contentSplitter->addWidget(rightSplitter);

    contentSplitter->setStretchFactor(0, 3);
    contentSplitter->setStretchFactor(1, 4);
    contentSplitter->setSizes(QList<int>() << 480 << 700);

    rightSplitter->setStretchFactor(0, 3);
    rightSplitter->setStretchFactor(1, 2);
    rightSplitter->setSizes(QList<int>() << 430 << 290);

    mainLayout->addWidget(contentSplitter, 1);

    applyConnectionUiState(false);
}

QGroupBox *MasterQueryDialog::setupConnectionGroup()
{
#if 1
    QGroupBox *group = new QGroupBox(QStringLiteral("串口连接"), this);
    group->setObjectName(QStringLiteral("masterConnectionGroup"));

    QVBoxLayout *rootLayout = new QVBoxLayout(group);
    rootLayout->setContentsMargins(10, 10, 10, 10);
    rootLayout->setSpacing(8);

    m_portCombo = new QComboBox(group);
    m_portCombo->setMinimumWidth(240);
    m_portCombo->setMaximumWidth(360);

    m_refreshPortsButton = new QPushButton(QStringLiteral("刷新"), group);
    m_refreshPortsButton->setMinimumWidth(72);

    m_baudRateCombo = new QComboBox(group);
    m_baudRateCombo->addItems({QStringLiteral("1200"), QStringLiteral("2400"), QStringLiteral("4800"), QStringLiteral("9600"),
                               QStringLiteral("19200"), QStringLiteral("38400"), QStringLiteral("57600"), QStringLiteral("115200")});
    m_baudRateCombo->setCurrentText(QStringLiteral("2400"));
    m_baudRateCombo->setFixedWidth(110);

    m_dataBitsCombo = new QComboBox(group);
    m_dataBitsCombo->addItems({QStringLiteral("5"), QStringLiteral("6"), QStringLiteral("7"), QStringLiteral("8")});
    m_dataBitsCombo->setCurrentText(QStringLiteral("8"));
    m_dataBitsCombo->setFixedWidth(72);

    m_parityCombo = new QComboBox(group);
    m_parityCombo->addItems({QStringLiteral("None"), QStringLiteral("Odd"), QStringLiteral("Even"), QStringLiteral("Mark"), QStringLiteral("Space")});
    m_parityCombo->setCurrentText(QStringLiteral("Even"));
    m_parityCombo->setFixedWidth(130);

    m_stopBitsCombo = new QComboBox(group);
    m_stopBitsCombo->addItems({QStringLiteral("1"), QStringLiteral("1.5"), QStringLiteral("2")});
    m_stopBitsCombo->setFixedWidth(88);

    m_flowControlCombo = new QComboBox(group);
    m_flowControlCombo->addItems({QStringLiteral("None"), QStringLiteral("Hardware"), QStringLiteral("Software")});
    m_flowControlCombo->setFixedWidth(130);

    m_connectionStatusLabel = new QLabel(QStringLiteral("串口未连接"), group);
    m_connectionStatusLabel->setObjectName(QStringLiteral("masterStatusLabel"));
    m_connectionStatusLabel->setProperty("state", QStringLiteral("disconnected"));

    m_portHintLabel = new QLabel(group);
    m_portHintLabel->setWordWrap(true);
    m_portHintLabel->hide();

    m_connectButton = new QPushButton(QStringLiteral("连接"), group);
    m_disconnectButton = new QPushButton(QStringLiteral("断开"), group);

    QWidget *row1 = new QWidget(group);
    QHBoxLayout *row1Layout = new QHBoxLayout(row1);
    row1Layout->setContentsMargins(0, 0, 0, 0);
    row1Layout->setSpacing(10);
    row1Layout->addWidget(new QLabel(QStringLiteral("串口"), row1));
    row1Layout->addWidget(m_portCombo);
    row1Layout->addWidget(m_refreshPortsButton);
    row1Layout->addSpacing(12);
    row1Layout->addWidget(new QLabel(QStringLiteral("波特率"), row1));
    row1Layout->addWidget(m_baudRateCombo);
    row1Layout->addSpacing(10);
    row1Layout->addWidget(new QLabel(QStringLiteral("数据位"), row1));
    row1Layout->addWidget(m_dataBitsCombo);
    row1Layout->addStretch();
    rootLayout->addWidget(row1);

    QWidget *row2 = new QWidget(group);
    QHBoxLayout *row2Layout = new QHBoxLayout(row2);
    row2Layout->setContentsMargins(0, 0, 0, 0);
    row2Layout->setSpacing(10);
    row2Layout->addWidget(new QLabel(QStringLiteral("校验位"), row2));
    row2Layout->addWidget(m_parityCombo);
    row2Layout->addWidget(new QLabel(QStringLiteral("停止位"), row2));
    row2Layout->addWidget(m_stopBitsCombo);
    row2Layout->addWidget(new QLabel(QStringLiteral("流控"), row2));
    row2Layout->addWidget(m_flowControlCombo);
    row2Layout->addSpacing(12);
    row2Layout->addWidget(m_connectionStatusLabel);
    row2Layout->addStretch();
    row2Layout->addWidget(m_connectButton);
    row2Layout->addWidget(m_disconnectButton);
    rootLayout->addWidget(row2);

    connect(m_refreshPortsButton, &QPushButton::clicked, this, &MasterQueryDialog::onRefreshPortsClicked);
    connect(m_connectButton, &QPushButton::clicked, this, &MasterQueryDialog::onConnectClicked);
    connect(m_disconnectButton, &QPushButton::clicked, this, &MasterQueryDialog::onDisconnectClicked);

    return group;
#else
    QGroupBox *group = new QGroupBox(QString::fromUtf8("串口连接"), this);
    group->setObjectName(QStringLiteral("masterConnectionGroup"));
    QVBoxLayout *rootLayout = new QVBoxLayout(group);

    rootLayout->setSpacing(8);

    QGridLayout *gridLayout = new QGridLayout();
    gridLayout->setHorizontalSpacing(10);
    gridLayout->setVerticalSpacing(8);

    QWidget *portRow = new QWidget(group);
    QHBoxLayout *portLayout = new QHBoxLayout(portRow);
    portLayout->setContentsMargins(0, 0, 0, 0);
    m_portCombo = new QComboBox(portRow);
    portLayout->setSpacing(8);
    m_refreshPortsButton = new QPushButton(QString::fromUtf8("刷新"), portRow);
    portLayout->addWidget(m_portCombo, 1);
    m_refreshPortsButton->setMinimumWidth(72);
    portLayout->addWidget(m_refreshPortsButton);
    gridLayout->addWidget(new QLabel(QString::fromUtf8("串口"), group), 0, 0);
    gridLayout->addWidget(portRow, 0, 1, 1, 5);

    m_baudRateCombo = new QComboBox(group);
    m_baudRateCombo->addItems({QStringLiteral("1200"), QStringLiteral("2400"), QStringLiteral("4800"), QStringLiteral("9600"),
                               QStringLiteral("19200"), QStringLiteral("38400"), QStringLiteral("57600"), QStringLiteral("115200")});
    m_baudRateCombo->setCurrentText(QStringLiteral("2400"));
    gridLayout->addWidget(new QLabel(QString::fromUtf8("波特率"), group), 1, 0);
    gridLayout->addWidget(m_baudRateCombo, 0, 7);

    m_dataBitsCombo = new QComboBox(group);
    m_dataBitsCombo->addItems({QStringLiteral("5"), QStringLiteral("6"), QStringLiteral("7"), QStringLiteral("8")});
    m_dataBitsCombo->setCurrentText(QStringLiteral("8"));
    gridLayout->addWidget(new QLabel(QString::fromUtf8("数据位"), group), 1, 2);
    gridLayout->addWidget(m_dataBitsCombo, 0, 9);

    m_parityCombo = new QComboBox(group);
    m_parityCombo->addItems({QStringLiteral("None"), QStringLiteral("Odd"), QStringLiteral("Even"), QStringLiteral("Mark"), QStringLiteral("Space")});
    m_parityCombo->setCurrentText(QStringLiteral("Even"));
    gridLayout->addWidget(new QLabel(QString::fromUtf8("校验位"), group), 1, 4);
    gridLayout->addWidget(m_parityCombo, 1, 1);

    m_stopBitsCombo = new QComboBox(group);
    m_stopBitsCombo->addItems({QStringLiteral("1"), QStringLiteral("1.5"), QStringLiteral("2")});
    gridLayout->addWidget(new QLabel(QString::fromUtf8("停止位"), group), 1, 6);
    gridLayout->addWidget(m_stopBitsCombo, 1, 3);

    m_flowControlCombo = new QComboBox(group);
    m_flowControlCombo->addItems({QStringLiteral("None"), QStringLiteral("Hardware"), QStringLiteral("Software")});
    gridLayout->addWidget(new QLabel(QString::fromUtf8("流控"), group), 1, 8);
    gridLayout->addWidget(m_flowControlCombo, 1, 5);

    gridLayout->setColumnStretch(1, 3);
    gridLayout->setColumnStretch(3, 2);
    gridLayout->setColumnStretch(5, 2);
    gridLayout->setColumnStretch(7, 2);
    gridLayout->setColumnStretch(9, 2);
    gridLayout->setColumnStretch(10, 1);
    rootLayout->addLayout(gridLayout);

    m_connectionStatusLabel = new QLabel(QString::fromUtf8("未连接"), group);
    m_connectionStatusLabel->setObjectName(QStringLiteral("masterStatusLabel"));
    m_connectionStatusLabel->setProperty("state", QStringLiteral("disconnected"));
    gridLayout->addWidget(m_connectionStatusLabel, 1, 10);

    m_portHintLabel = new QLabel(group);
    m_portHintLabel->setWordWrap(true);
    m_portHintLabel->hide();

    QWidget *buttonRow = new QWidget(group);
    QHBoxLayout *buttonLayout = new QHBoxLayout(buttonRow);
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->setSpacing(8);
    m_connectButton = new QPushButton(QString::fromUtf8("连接"), group);
    m_disconnectButton = new QPushButton(QString::fromUtf8("断开"), group);
    buttonLayout->addWidget(m_connectButton);
    buttonLayout->addWidget(m_disconnectButton);
    gridLayout->addWidget(buttonRow, 1, 11);

    rootLayout->removeItem(gridLayout);

    for (QLabel *label : group->findChildren<QLabel *>(QString(), Qt::FindDirectChildrenOnly))
    {
        if (label != m_connectionStatusLabel && label != m_portHintLabel)
        {
            label->hide();
        }
    }

    QWidget *compactRow1 = new QWidget(group);
    QHBoxLayout *compactRow1Layout = new QHBoxLayout(compactRow1);
    compactRow1Layout->setContentsMargins(0, 0, 0, 0);
    compactRow1Layout->setSpacing(10);
    compactRow1Layout->addWidget(new QLabel(QStringLiteral("串口"), compactRow1));
    compactRow1Layout->addWidget(portRow, 5);
    compactRow1Layout->addWidget(new QLabel(QStringLiteral("波特率"), compactRow1));
    compactRow1Layout->addWidget(m_baudRateCombo, 1);
    compactRow1Layout->addWidget(new QLabel(QStringLiteral("数据位"), compactRow1));
    compactRow1Layout->addWidget(m_dataBitsCombo, 1);
    rootLayout->addWidget(compactRow1);

    QWidget *compactRow2 = new QWidget(group);
    QHBoxLayout *compactRow2Layout = new QHBoxLayout(compactRow2);
    compactRow2Layout->setContentsMargins(0, 0, 0, 0);
    compactRow2Layout->setSpacing(10);
    compactRow2Layout->addWidget(new QLabel(QStringLiteral("校验位"), compactRow2));
    compactRow2Layout->addWidget(m_parityCombo, 1);
    compactRow2Layout->addWidget(new QLabel(QStringLiteral("停止位"), compactRow2));
    compactRow2Layout->addWidget(m_stopBitsCombo, 1);
    compactRow2Layout->addWidget(new QLabel(QStringLiteral("流控"), compactRow2));
    compactRow2Layout->addWidget(m_flowControlCombo, 1);
    compactRow2Layout->addWidget(m_connectionStatusLabel, 1);
    compactRow2Layout->addWidget(buttonRow);
    rootLayout->addWidget(compactRow2);

    delete gridLayout;

    connect(m_refreshPortsButton, &QPushButton::clicked, this, &MasterQueryDialog::onRefreshPortsClicked);
    connect(m_connectButton, &QPushButton::clicked, this, &MasterQueryDialog::onConnectClicked);
    connect(m_disconnectButton, &QPushButton::clicked, this, &MasterQueryDialog::onDisconnectClicked);

    return group;
#endif
}

QGroupBox *MasterQueryDialog::setupQueryGroup()
{
    QGroupBox *group = new QGroupBox(QString::fromUtf8("查询配置"), this);
    group->setObjectName(QStringLiteral("masterQueryGroup"));
    QVBoxLayout *rootLayout = new QVBoxLayout(group);

    QFormLayout *formLayout = new QFormLayout();
    formLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_queryTypeCombo = new QComboBox(group);
    m_queryTypeCombo->addItems({QString::fromUtf8("读数据"), QString::fromUtf8("读地址")});
    formLayout->addRow(QString::fromUtf8("查询类型"), m_queryTypeCombo);

    m_addressEdit = new QLineEdit(group);
    m_addressEdit->setPlaceholderText(QString::fromUtf8("请输入 12 位电表地址"));
    m_addressEdit->setValidator(new QRegularExpressionValidator(QRegularExpression(QStringLiteral("\\d{0,12}")), m_addressEdit));
    formLayout->addRow(QString::fromUtf8("目标地址"), m_addressEdit);

    m_includePrefixCheckBox = new QCheckBox(QString::fromUtf8("包含前导 FE"), group);
    formLayout->addRow(QString::fromUtf8("帧选项"), m_includePrefixCheckBox);

    m_timeoutSpinBox = new QSpinBox(group);
    m_timeoutSpinBox->setRange(500, 10000);
    m_timeoutSpinBox->setValue(3000);
    m_timeoutSpinBox->setSuffix(QStringLiteral(" ms"));
    formLayout->addRow(QString::fromUtf8("超时时间"), m_timeoutSpinBox);

    rootLayout->addLayout(formLayout);

    QHBoxLayout *filterLayout = new QHBoxLayout();
    m_searchEdit = new QLineEdit(group);
    m_searchEdit->setPlaceholderText(QString::fromUtf8("搜索 DI 或描述"));
    m_categoryComboBox = new QComboBox(group);
    filterLayout->addWidget(m_searchEdit, 2);
    filterLayout->addWidget(m_categoryComboBox, 1);
    rootLayout->addLayout(filterLayout);

    m_dataItemList = new QListWidget(group);
    rootLayout->addWidget(m_dataItemList, 1);

    QHBoxLayout *footerLayout = new QHBoxLayout();
    m_selectedDiLabel = new QLabel(QString::fromUtf8("当前未选择数据标识"), group);
    m_selectedDiLabel->setWordWrap(true);
    footerLayout->addWidget(m_selectedDiLabel, 1);
    m_sendButton = new QPushButton(QString::fromUtf8("发送读数据"), group);
    footerLayout->addWidget(m_sendButton);
    rootLayout->addLayout(footerLayout);

    connect(m_queryTypeCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &MasterQueryDialog::onQueryTypeChanged);
    connect(m_addressEdit, &QLineEdit::textChanged, this, [this]() { updateQueryUiState(); });
    connect(m_categoryComboBox,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &MasterQueryDialog::onCategoryChanged);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &MasterQueryDialog::onSearchTextChanged);
    connect(m_dataItemList, &QListWidget::currentItemChanged, this, &MasterQueryDialog::onCurrentItemChanged);
    connect(m_sendButton, &QPushButton::clicked, this, &MasterQueryDialog::onSendClicked);
    return group;
}

QGroupBox *MasterQueryDialog::setupResultGroup()
{
    QGroupBox *group = new QGroupBox(QString::fromUtf8("结果与解析"), this);
    group->setObjectName(QStringLiteral("masterResultGroup"));
    QVBoxLayout *rootLayout = new QVBoxLayout(group);

    m_resultMetaLabel = new QLabel(group);
    m_resultMetaLabel->setWordWrap(true);
    rootLayout->addWidget(m_resultMetaLabel);

    m_noticeLabel = new QLabel(group);
    m_noticeLabel->setObjectName(QStringLiteral("masterNoticeLabel"));
    m_noticeLabel->setWordWrap(true);
    rootLayout->addWidget(m_noticeLabel);

    QFormLayout *hexLayout = new QFormLayout();
    m_requestHexEdit = new QLineEdit(group);
    m_requestHexEdit->setReadOnly(true);
    m_requestHexEdit->setFont(QFont(QStringLiteral("Consolas"), 10));
    hexLayout->addRow(QString::fromUtf8("发送 HEX"), m_requestHexEdit);

    m_responseHexEdit = new QLineEdit(group);
    m_responseHexEdit->setReadOnly(true);
    m_responseHexEdit->setFont(QFont(QStringLiteral("Consolas"), 10));
    hexLayout->addRow(QString::fromUtf8("接收 HEX"), m_responseHexEdit);
    rootLayout->addLayout(hexLayout);

    m_parseResultWidget = new FrameParseResultWidget(group);
    rootLayout->addWidget(m_parseResultWidget, 1);
    return group;
}

QGroupBox *MasterQueryDialog::setupHistoryGroup()
{
    QGroupBox *group = new QGroupBox(QString::fromUtf8("历史记录"), this);
    group->setObjectName(QStringLiteral("masterHistoryGroup"));
    QVBoxLayout *rootLayout = new QVBoxLayout(group);

    QHBoxLayout *toolbarLayout = new QHBoxLayout();
    toolbarLayout->addStretch();
    m_clearHistoryButton = new QPushButton(QString::fromUtf8("清空历史"), group);
    toolbarLayout->addWidget(m_clearHistoryButton);
    rootLayout->addLayout(toolbarLayout);

    m_historyTable = new QTableWidget(group);
    m_historyTable->setColumnCount(5);
    m_historyTable->setHorizontalHeaderLabels({QString::fromUtf8("时间"),
                                               QString::fromUtf8("类型"),
                                               QString::fromUtf8("地址"),
                                               QString::fromUtf8("状态"),
                                               QString::fromUtf8("摘要")});
    m_historyTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_historyTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_historyTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_historyTable->verticalHeader()->setVisible(false);
    m_historyTable->horizontalHeader()->setStretchLastSection(true);
    m_historyTable->setColumnWidth(0, 150);
    m_historyTable->setColumnWidth(1, 90);
    m_historyTable->setColumnWidth(2, 140);
    m_historyTable->setColumnWidth(3, 90);
    rootLayout->addWidget(m_historyTable);

    connect(m_clearHistoryButton, &QPushButton::clicked, this, &MasterQueryDialog::onClearHistoryClicked);
    connect(m_historyTable, &QTableWidget::itemSelectionChanged, this, &MasterQueryDialog::onHistorySelectionChanged);
    return group;
}

void MasterQueryDialog::refreshPorts()
{
    applyPortList(m_cachedPorts);
    m_refreshPortsButton->setEnabled(!m_controller->isConnected());
}

void MasterQueryDialog::requestPortRefresh()
{
    if (m_portRefreshWatcher->isRunning())
    {
        m_portRefreshPending = true;
        return;
    }

    m_portRefreshPending = false;
    m_refreshPortsButton->setEnabled(false);
    m_portRefreshWatcher->setFuture(QtConcurrent::run(&MasterQueryController::getAvailablePorts));
}

void MasterQueryDialog::applyPortList(const QList<QSerialPortInfo> &ports)
{
    const QString selectedPort = currentPortName();

    const QSignalBlocker blocker(m_portCombo);
    m_portCombo->clear();

    for (const QSerialPortInfo &info : ports)
    {
        QString text = info.portName();
        if (m_unavailablePorts.contains(info.portName()))
        {
            text += QString::fromUtf8(" (模拟器占用)");
        }
        m_portCombo->addItem(text, info.portName());
    }

    if (!selectedPort.isEmpty())
    {
        const int index = m_portCombo->findData(selectedPort);
        if (index >= 0)
        {
            m_portCombo->setCurrentIndex(index);
        }
    }
    else if (m_portCombo->count() > 0)
    {
        m_portCombo->setCurrentIndex(0);
    }

    if (m_portCombo->count() == 0)
    {
        m_portHintLabel->clear();
    }
    else if (!m_unavailablePorts.isEmpty())
    {
        m_portHintLabel->clear();
    }
    else
    {
        m_portHintLabel->clear();
    }
}

void MasterQueryDialog::populateCategories()
{
    const QSignalBlocker blocker(m_categoryComboBox);
    m_categoryComboBox->clear();
    for (const QString &category : allCategories())
    {
        m_categoryComboBox->addItem(category);
    }
}

void MasterQueryDialog::populateDataItems()
{
    const QSignalBlocker blocker(m_dataItemList);
    m_dataItemList->clear();

    const QString filterText = m_searchEdit->text().trimmed();
    const bool hasFilter = !filterText.isEmpty();
    const QList<QString> categoriesToLoad = hasFilter
                                                ? allCategories()
                                                : QList<QString>{m_categoryComboBox->currentText()};

    for (const QString &category : categoriesToLoad)
    {
        const QList<QString> diCodes = dataCodesForCategory(category);
        for (const QString &diCode : diCodes)
        {
            const DataItem item = m_catalogMeterModel->getDataItem(diCode);
            const QString description = item.description.isEmpty() ? QString::fromUtf8("未命名数据项") : item.description;
            const QString matchText = QStringLiteral("%1 %2").arg(diCode, description);
            if (hasFilter && !matchText.contains(filterText, Qt::CaseInsensitive))
            {
                continue;
            }

            const QString itemText = hasFilter
                                         ? QStringLiteral("[%1] %2  |  %3").arg(category, diCode, description)
                                         : QStringLiteral("%1  |  %2").arg(diCode, description);
            QListWidgetItem *listItem = new QListWidgetItem(itemText, m_dataItemList);
            listItem->setData(Qt::UserRole, diCode);
            listItem->setData(Qt::UserRole + 1, description);
            listItem->setData(Qt::UserRole + 2, category);
        }
    }

    m_categoryComboBox->setEnabled(!hasFilter);

    if (m_dataItemList->count() > 0)
    {
        m_dataItemList->setCurrentRow(0);
    }
    else
    {
        m_selectedDiLabel->setText(QString::fromUtf8("当前未选择数据标识"));
    }

    updateQueryUiState();
}

QList<QString> MasterQueryDialog::allCategories() const
{
    QList<QString> categories;
    categories.append(settlementDayCategories(QStringLiteral("电能量")));
    categories.append(settlementDayCategories(QStringLiteral("最大需量及发生时间")));
    categories.append(QStringLiteral("变量"));

    for (const QString &category : eventCategoryNames())
    {
        categories.append(QStringLiteral("事件记录/%1").arg(category));
    }

    for (const QString &category : parameterCategoryNames())
    {
        categories.append(QStringLiteral("参变量/%1").arg(category));
    }

    categories << QStringLiteral("定时冻结")
               << QStringLiteral("瞬时冻结")
               << QStringLiteral("时区切换冻结")
               << QStringLiteral("日时段切换冻结")
               << QStringLiteral("整点冻结")
               << QStringLiteral("分时费率切换冻结")
               << QStringLiteral("日冻结")
               << QStringLiteral("阶梯切换冻结")
               << QStringLiteral("负荷记录");

    return categories;
}

QList<QString> MasterQueryDialog::dataCodesForCategory(const QString &category) const
{
    const CategorySelection selection = parseCategorySelection(category);
    const QString rootCategory = selection.rootCategory;
    const QString leafCategory = selection.leafCategory;
    const bool hasRootCategory = !leafCategory.isEmpty();

    if ((hasRootCategory && rootCategory == QStringLiteral("电能量")) || category == QStringLiteral("当前"))
    {
        const int settlementDay = hasRootCategory ? settlementDayFromCategoryName(leafCategory)
                                                  : settlementDayFromCategoryName(category);
        if (settlementDay == 0)
        {
            return m_catalogMeterModel->getEnergyDataCodes();
        }
        if (settlementDay > 0)
        {
            return m_catalogMeterModel->getSettlementDayEnergyDataCodes(settlementDay);
        }
        return {};
    }

    if ((hasRootCategory && rootCategory == QStringLiteral("最大需量及发生时间")) || category == QStringLiteral("最大需量及发生时间"))
    {
        if (hasRootCategory)
        {
            const int settlementDay = settlementDayFromCategoryName(leafCategory);
            return settlementDay >= 0 ? m_catalogMeterModel->getSettlementDayDemandDataCodes(settlementDay) : QList<QString>();
        }

        QList<QString> codes = m_catalogMeterModel->getDemandDataCodes();
        const QList<QString> maxDemandCodes = m_catalogMeterModel->getMaxDemandDataCodes();
        for (const QString &diCode : maxDemandCodes)
        {
            if (!codes.contains(diCode))
            {
                codes.append(diCode);
            }
        }
        return codes;
    }
    if (category == QStringLiteral("变量"))
    {
        return m_catalogMeterModel->getVariableDataCodes();
    }
    if ((hasRootCategory && rootCategory == QStringLiteral("事件记录")) || category == QStringLiteral("事件记录"))
    {
        const QList<QString> eventCodes = m_catalogMeterModel->getEventDataCodes();
        return hasRootCategory ? filterEventCodesByCategory(m_catalogMeterModel, eventCodes, leafCategory) : eventCodes;
    }
    if ((hasRootCategory && rootCategory == QStringLiteral("参变量")) || category == QStringLiteral("参变量"))
    {
        const QList<QString> parameterCodes = m_catalogMeterModel->getParameterDataCodes();
        return hasRootCategory ? filterParameterCodesByCategory(parameterCodes, leafCategory) : parameterCodes;
    }
    if (category == QStringLiteral("定时冻结") ||
        category == QStringLiteral("瞬时冻结") ||
        category == QStringLiteral("时区切换冻结") ||
        category == QStringLiteral("日时段切换冻结") ||
        category == QStringLiteral("整点冻结") ||
        category == QStringLiteral("分时费率切换冻结") ||
        category == QStringLiteral("日冻结") ||
        category == QStringLiteral("阶梯切换冻结"))
    {
        return m_catalogMeterModel->getFreezeDataCodes(category);
    }
    if (category == QStringLiteral("负荷记录"))
    {
        return m_catalogMeterModel->getLoadRecordDataCodes();
    }

    return {};
}

QString MasterQueryDialog::selectedDiCode() const
{
    QListWidgetItem *item = m_dataItemList->currentItem();
    return item ? item->data(Qt::UserRole).toString() : QString();
}

QString MasterQueryDialog::selectedDiDescription() const
{
    QListWidgetItem *item = m_dataItemList->currentItem();
    return item ? item->data(Qt::UserRole + 1).toString() : QString();
}

QString MasterQueryDialog::currentPortName() const
{
    return m_portCombo->currentData().toString();
}

MasterSerialConfig MasterQueryDialog::currentSerialConfig() const
{
    MasterSerialConfig config;
    config.portName = currentPortName();
    config.baudRate = m_baudRateCombo->currentText().toInt();
    config.dataBits = dataBitsFromText(m_dataBitsCombo->currentText());
    config.parity = parityFromText(m_parityCombo->currentText());
    config.stopBits = stopBitsFromText(m_stopBitsCombo->currentText());
    config.flowControl = flowControlFromText(m_flowControlCombo->currentText());
    return config;
}

MasterRequestSpec MasterQueryDialog::currentRequestSpec() const
{
    MasterRequestSpec spec;
    spec.meterAddress = m_addressEdit->text().trimmed();
    spec.diCode = (m_queryTypeCombo->currentIndex() == 0) ? selectedDiCode() : QString();
    spec.includePrefixFE = m_includePrefixCheckBox->isChecked();
    spec.timeoutMs = m_timeoutSpinBox->value();
    return spec;
}

void MasterQueryDialog::applyConnectionUiState(bool connected)
{
    if (!m_connectionStatusLabel ||
        !m_connectButton ||
        !m_disconnectButton ||
        !m_portCombo ||
        !m_baudRateCombo ||
        !m_dataBitsCombo ||
        !m_parityCombo ||
        !m_stopBitsCombo ||
        !m_flowControlCombo ||
        !m_refreshPortsButton)
    {
        return;
    }
    m_connectionStatusLabel->setText(connected ? QString::fromUtf8("串口已连接") : QString::fromUtf8("串口未连接"));
    m_connectionStatusLabel->setProperty("state", connected ? QStringLiteral("connected") : QStringLiteral("disconnected"));
    style()->unpolish(m_connectionStatusLabel);
    style()->polish(m_connectionStatusLabel);

    m_connectButton->setEnabled(!connected);
    m_disconnectButton->setEnabled(connected);

    m_portCombo->setEnabled(!connected);
    m_baudRateCombo->setEnabled(!connected);
    m_dataBitsCombo->setEnabled(!connected);
    m_parityCombo->setEnabled(!connected);
    m_stopBitsCombo->setEnabled(!connected);
    m_flowControlCombo->setEnabled(!connected);
    m_refreshPortsButton->setEnabled(!connected && !m_portRefreshWatcher->isRunning());

    updateQueryUiState();
}

void MasterQueryDialog::updateQueryUiState()
{
    if (!m_queryTypeCombo ||
        !m_searchEdit ||
        !m_categoryComboBox ||
        !m_dataItemList ||
        !m_sendButton ||
        !m_selectedDiLabel ||
        !m_addressEdit)
    {
        return;
    }

    const bool isReadData = (m_queryTypeCombo->currentIndex() == 0);
    const bool connected = m_controller->isConnected();

    m_searchEdit->setEnabled(isReadData);
    m_categoryComboBox->setEnabled(isReadData && m_searchEdit->text().trimmed().isEmpty());
    m_dataItemList->setEnabled(isReadData);

    if (isReadData)
    {
        m_sendButton->setText(QString::fromUtf8("发送读数据"));
        if (m_dataItemList->currentItem())
        {
            m_selectedDiLabel->setText(QString::fromUtf8("当前选择：%1").arg(m_dataItemList->currentItem()->text()));
        }
        else
        {
            m_selectedDiLabel->setText(QString::fromUtf8("当前未选择数据标识"));
        }
    }
    else
    {
        m_sendButton->setText(QString::fromUtf8("发送读地址"));
        m_selectedDiLabel->setText(QString::fromUtf8("当前选择：读取通信地址"));
    }

    const bool canSend = connected &&
                         ProtocolParser::isValidAddress(m_addressEdit->text().trimmed()) &&
                         (!isReadData || !selectedDiCode().isEmpty()) &&
                         (m_controller->currentStatus() == MasterQueryStatus::Idle);
    m_sendButton->setEnabled(canSend);
}

void MasterQueryDialog::displayResult(const MasterQueryResult &result)
{
    const QString timeText = result.timestamp.isValid()
                                 ? result.timestamp.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"))
                                 : QString::fromUtf8("未记录时间");

    m_resultMetaLabel->setText(QString::fromUtf8("时间：%1\n类型：%2\n目标：%3\n状态：%4")
                                   .arg(timeText,
                                        requestTypeText(result),
                                        requestTargetText(result),
                                        statusText(result.status)));

    m_requestHexEdit->setText(DLT645FrameFormatter::formatHex(result.rawRequest));
    m_responseHexEdit->setText(DLT645FrameFormatter::formatHex(result.rawResponse));

    if (!result.errorText.isEmpty())
    {
        m_noticeLabel->setText(result.errorText);
    }

    if (result.rawResponse.isEmpty())
    {
        m_parseResultWidget->clearResult();
    }
    else
    {
        FrameParseDisplayResult display = (result.status == MasterQueryStatus::Completed)
                                              ? DLT645FrameFormatter::buildParseDisplayResult(result.rawResponse, m_catalogMeterModel)
                                              : DLT645FrameFormatter::buildParseDisplayResult(result.rawResponse, m_catalogMeterModel, false);

        if (!result.errorText.isEmpty())
        {
            display.errorText = result.errorText;
        }

        m_parseResultWidget->setParseResult(display);
    }

    updateQueryUiState();
}

int MasterQueryDialog::appendHistoryRow(const MasterQueryResult &result)
{
    const int row = m_historyTable->rowCount();
    m_historyTable->insertRow(row);
    m_historyResults.append(result);
    updateHistoryRow(row, result);
    return row;
}

void MasterQueryDialog::updateHistoryRow(int row, const MasterQueryResult &result)
{
    if (row < 0)
    {
        return;
    }

    const QString timeText = result.timestamp.isValid()
                                 ? result.timestamp.toString(QStringLiteral("HH:mm:ss.zzz"))
                                 : QString::fromUtf8("--");

    auto setCell = [this, row](int column, const QString &text)
    {
        QTableWidgetItem *item = m_historyTable->item(row, column);
        if (!item)
        {
            item = new QTableWidgetItem();
            m_historyTable->setItem(row, column, item);
        }
        item->setText(text);
    };

    setCell(0, timeText);
    setCell(1, requestTypeText(result));
    setCell(2, requestTargetText(result));
    setCell(3, statusText(result.status));
    setCell(4, requestSummaryText(result));
}

QString MasterQueryDialog::requestTypeText(const MasterQueryResult &result) const
{
    return result.requestSpec.isReadAddressRequest()
               ? QString::fromUtf8("读地址")
               : QString::fromUtf8("读数据");
}

QString MasterQueryDialog::requestTargetText(const MasterQueryResult &result) const
{
    return result.requestSpec.meterAddress.isEmpty()
               ? QString::fromUtf8("未指定")
               : result.requestSpec.meterAddress;
}

QString MasterQueryDialog::requestSummaryText(const MasterQueryResult &result) const
{
    if (result.requestSpec.isReadAddressRequest())
    {
        return resultSummary(result);
    }

    QString diSummary;
    if (!result.requestSpec.diCode.isEmpty())
    {
        const DataItem item = m_catalogMeterModel->getDataItem(result.requestSpec.diCode);
        const QString description = item.description.trimmed();
        diSummary = description.isEmpty()
                        ? result.requestSpec.diCode
                        : QStringLiteral("%1 (%2)").arg(description, result.requestSpec.diCode);
        diSummary += QStringLiteral(" | ");
    }

    return diSummary + resultSummary(result);
}

QString MasterQueryDialog::statusText(MasterQueryStatus status) const
{
    switch (status)
    {
    case MasterQueryStatus::Idle:
        return QString::fromUtf8("空闲");
    case MasterQueryStatus::Sending:
        return QString::fromUtf8("发送中");
    case MasterQueryStatus::WaitingResponse:
        return QString::fromUtf8("等待应答");
    case MasterQueryStatus::Completed:
        return QString::fromUtf8("完成");
    case MasterQueryStatus::Timeout:
        return QString::fromUtf8("超时");
    case MasterQueryStatus::Error:
        return QString::fromUtf8("错误");
    }

    return QString::fromUtf8("未知");
}
