#include "CommunicationDialog.h"
#include "Models/DataItem.h"
#include "Models/MeterModel.h"
#include "Services/ProtocolParser.h"
#include "Utils/DLT645FrameFormatter.h"
#include "Utils/DLT645DataHelper.h"
#include "Views/Common/FrameParseResultWidget.h"
#include <QHeaderView>
#include <QFileDialog>
#include <QMessageBox>
#include <QSplitter>
#include <QDateTime>
#include <QCloseEvent>

namespace
{
QString displayMessageDiCode(const QByteArray &fullFrame)
{
    const QByteArray protocolFrame = DLT645DataHelper::stripLeadingFE(fullFrame);
    if (protocolFrame.size() < 12)
    {
        return QStringLiteral("-");
    }

    const quint8 controlCode = static_cast<quint8>(protocolFrame[8]);
    const quint8 baseControlCode = controlCode & 0x7F;
    const bool hasDataIdentifier = baseControlCode == 0x11 ||
                                   baseControlCode == 0x12 ||
                                   baseControlCode == 0x14 ||
                                   baseControlCode == 0x18 ||
                                   baseControlCode == 0x19;
    if (!hasDataIdentifier)
    {
        return QStringLiteral("-");
    }

    const int declaredDataLength = static_cast<quint8>(protocolFrame[9]);
    const int availableDataLength = protocolFrame.size() - 12;
    const int actualDataLength = qMin(declaredDataLength, qMax(0, availableDataLength));
    if (actualDataLength < 4)
    {
        return QStringLiteral("-");
    }

    const QString diCode = ProtocolParser::extractDIFromDataField(protocolFrame.mid(10, actualDataLength));
    if (diCode.isEmpty())
    {
        return QStringLiteral("-");
    }

    QString display = diCode;
    display.remove(QStringLiteral("0x"), Qt::CaseInsensitive);
    return display.toUpper();
}
}

CommunicationDialog::CommunicationDialog(const QString &connectionName, QWidget *parent)
    : QDialog(parent), m_connectionName(connectionName), m_portName(""), m_baudRate(2400), m_isConnected(false), m_meterModel(nullptr), m_connectionInfoBar(nullptr), m_filterBar(nullptr), m_isPaused(false), m_autoScroll(true), m_displayFormat(DisplayFormat::Hex), m_filterRequest(true), m_filterResponse(true), m_filterError(true), m_filterText("")
{
    setupUI();
    setWindowTitle(QString("通信交互 - %1").arg(connectionName));
    setMinimumSize(1100, 720);
    if (parentWidget())
    {
        resize(parentWidget()->width() * 9 / 10, parentWidget()->height() * 9 / 10);
    }
    else
    {
        resize(1280, 820);
    }
    setWindowTitle(QStringLiteral("通信交互 - %1").arg(connectionName));
    // 不设置WA_DeleteOnClose，由主窗口管理窗口生命周期
}

CommunicationDialog::~CommunicationDialog()
{
}

void CommunicationDialog::setMeterModel(MeterModel *meterModel)
{
    m_meterModel = meterModel;
    refreshMessageList();
}

void CommunicationDialog::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(12, 12, 12, 12);

    // 连接信息栏
    setupConnectionInfoBar();
    mainLayout->addWidget(m_connectionInfoBar);

    // 过滤栏
    setupFilterBar();
    mainLayout->addWidget(m_filterBar);

    // 创建分割器
    QSplitter *splitter = new QSplitter(Qt::Vertical, this);
    splitter->setChildrenCollapsible(false);
    splitter->setHandleWidth(10);

    // 报文列表
    setupMessageTable();
    QGroupBox *messageGroup = new QGroupBox(QString::fromUtf8("报文列表"), this);
    messageGroup->setTitle(QStringLiteral("报文列表"));
    QVBoxLayout *messageLayout = new QVBoxLayout(messageGroup);
    messageLayout->setContentsMargins(10, 12, 10, 10);
    messageLayout->addWidget(m_tableMessages);
    splitter->addWidget(messageGroup);

    // 报文详情
    setupMessageDetail();
    QGroupBox *detailGroup = new QGroupBox(QString::fromUtf8("帧解析"), this);
    detailGroup->setTitle(QStringLiteral("帧解析"));
    QVBoxLayout *detailLayout = new QVBoxLayout(detailGroup);
    detailLayout->setContentsMargins(10, 12, 10, 10);
    detailLayout->addWidget(m_detailWidget);
    splitter->addWidget(detailGroup);

    // 设置分割器比例
    splitter->setStretchFactor(0, 5);
    splitter->setStretchFactor(1, 2);
    splitter->setSizes({500, 140});

    mainLayout->addWidget(splitter);

    // 统计信息栏
    setupStatisticsBar();
    QGroupBox *statisticsGroup = new QGroupBox(QString::fromUtf8("统计信息"), this);
    statisticsGroup->setTitle(QStringLiteral("统计信息"));
    QVBoxLayout *statisticsLayout = new QVBoxLayout(statisticsGroup);
    statisticsLayout->setContentsMargins(10, 12, 10, 10);
    statisticsLayout->addWidget(m_labelStatistics);
    mainLayout->addWidget(statisticsGroup);
}

void CommunicationDialog::setupConnectionInfoBar()
{
    // 创建容器控件
    m_connectionInfoBar = new QGroupBox(QString::fromUtf8("连接信息"), this);
    static_cast<QGroupBox *>(m_connectionInfoBar)->setTitle(QStringLiteral("连接信息"));
    QHBoxLayout *layout = new QHBoxLayout(m_connectionInfoBar);
    layout->setSpacing(10);
    layout->setContentsMargins(10, 12, 10, 10);

    // 连接信息标签
    m_labelConnectionInfo = new QLabel(m_connectionInfoBar);
    m_labelConnectionInfo->setObjectName(QStringLiteral("infoLabel"));
    m_labelConnectionInfo->setText(QStringLiteral("连接：未设置"));
    m_labelConnectionInfo->setText("连接：未设置");

    // 状态标签
    m_labelStatus = new QLabel(m_connectionInfoBar);
    m_labelStatus->setObjectName(QStringLiteral("statusLabel"));
    updateConnectionStatus();

    // 清空按钮
    m_btnClear = new QPushButton("清空", m_connectionInfoBar);
    m_btnClear->setMinimumWidth(80);
    m_btnClear->setText(QStringLiteral("清空"));
    connect(m_btnClear, &QPushButton::clicked, this, &CommunicationDialog::onClearClicked);

    // 暂停按钮
    m_btnPause = new QPushButton("暂停", m_connectionInfoBar);
    m_btnPause->setMinimumWidth(80);
    m_btnPause->setText(QStringLiteral("暂停"));
    connect(m_btnPause, &QPushButton::clicked, this, &CommunicationDialog::togglePause);

    // 导出按钮
    m_btnExport = new QPushButton("导出", m_connectionInfoBar);
    m_btnExport->setMinimumWidth(80);
    m_btnExport->setText(QStringLiteral("导出"));
    connect(m_btnExport, &QPushButton::clicked, this, &CommunicationDialog::onExportClicked);

    layout->addWidget(m_labelConnectionInfo);
    layout->addStretch();
    layout->addWidget(m_labelStatus);
    layout->addWidget(m_btnClear);
    layout->addWidget(m_btnPause);
    layout->addWidget(m_btnExport);
}

void CommunicationDialog::setupFilterBar()
{
    // 创建容器控件
    m_filterBar = new QGroupBox(QString::fromUtf8("过滤条件"), this);
    static_cast<QGroupBox *>(m_filterBar)->setTitle(QStringLiteral("过滤条件"));
    QHBoxLayout *layout = new QHBoxLayout(m_filterBar);
    layout->setSpacing(10);
    layout->setContentsMargins(10, 12, 10, 10);

    // 过滤输入框
    m_lineEditFilter = new QLineEdit(m_filterBar);
    m_lineEditFilter->setPlaceholderText("过滤报文内容...");
    m_lineEditFilter->setPlaceholderText(QStringLiteral("过滤报文内容..."));
    connect(m_lineEditFilter, &QLineEdit::textChanged, this, &CommunicationDialog::onFilterChanged);

    // 类型过滤复选框
    m_checkBoxRequest = new QCheckBox("请求帧", m_filterBar);
    m_checkBoxRequest->setText(QStringLiteral("请求帧"));
    m_checkBoxRequest->setChecked(true);
    connect(m_checkBoxRequest, &QCheckBox::stateChanged, this, &CommunicationDialog::onFilterTypeChanged);

    m_checkBoxResponse = new QCheckBox("应答帧", m_filterBar);
    m_checkBoxResponse->setText(QStringLiteral("应答帧"));
    m_checkBoxResponse->setChecked(true);
    connect(m_checkBoxResponse, &QCheckBox::stateChanged, this, &CommunicationDialog::onFilterTypeChanged);

    m_checkBoxError = new QCheckBox("错误帧", m_filterBar);
    m_checkBoxError->setText(QStringLiteral("错误帧"));
    m_checkBoxError->setChecked(true);
    connect(m_checkBoxError, &QCheckBox::stateChanged, this, &CommunicationDialog::onFilterTypeChanged);

    // 显示格式下拉框
    m_comboDisplayFormat = new QComboBox(m_filterBar);
    m_comboDisplayFormat->addItem("HEX");
    m_comboDisplayFormat->addItem("ASCII");
    connect(m_comboDisplayFormat, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CommunicationDialog::onDisplayFormatChanged);

    layout->addWidget(new QLabel("过滤:"));
    layout->addWidget(m_lineEditFilter);
    layout->addWidget(m_checkBoxRequest);
    layout->addWidget(m_checkBoxResponse);
    layout->addWidget(m_checkBoxError);
    layout->addStretch();
    layout->addWidget(m_comboDisplayFormat);

    if (QLabel *filterLabel = qobject_cast<QLabel *>(layout->itemAt(0)->widget()))
    {
        filterLabel->setText(QStringLiteral("过滤:"));
    }
}

void CommunicationDialog::setupMessageTable()
{
    m_tableMessages = new QTableWidget(this);
    m_tableMessages->setColumnCount(5);
    m_tableMessages->setHorizontalHeaderLabels({QString::fromUtf8("时间"),
                                                QString::fromUtf8("方向"),
                                                QString::fromUtf8("数据标识"),
                                                QString::fromUtf8("标识说明"),
                                                QString::fromUtf8("完整报文")});
    m_tableMessages->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableMessages->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableMessages->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tableMessages->verticalHeader()->setVisible(false);
    m_tableMessages->setMinimumHeight(220);

    // 设置列宽
    m_tableMessages->setColumnWidth(0, 120); // 时间
    m_tableMessages->setColumnWidth(1, 80);  // 方向
    m_tableMessages->setColumnWidth(2, 120); // 数据标识
    m_tableMessages->setColumnWidth(3, 220); // 标识说明
    m_tableMessages->setColumnWidth(4, 850); // 完整报文

    // 表头样式
    m_tableMessages->horizontalHeader()->setStretchLastSection(true);

    connect(m_tableMessages, &QTableWidget::itemSelectionChanged,
            this, &CommunicationDialog::onMessageSelectionChanged);
}

void CommunicationDialog::setupMessageDetail()
{
    m_detailWidget = new FrameParseResultWidget(this);
    m_detailWidget->setObjectName(QStringLiteral("detailResultWidget"));
    m_detailWidget->setMinimumHeight(100);
}

void CommunicationDialog::setupStatisticsBar()
{
    m_labelStatistics = new QLabel(this);
    m_labelStatistics->setObjectName(QStringLiteral("statisticsLabel"));
    m_labelStatistics->setWordWrap(true);
    updateStatistics();
}

void CommunicationDialog::setConnectionInfo(const QString &portName, int baudRate)
{
    m_portName = portName;
    m_baudRate = baudRate;

    QString info = QString("连接：%1  %2：%3")
                       .arg(m_connectionName)
                       .arg(portName)
                       .arg(baudRate);

    // 更新连接信息标签
    info = QStringLiteral("连接：%1  %2：%3")
               .arg(m_connectionName)
               .arg(portName)
               .arg(baudRate);
    m_labelConnectionInfo->setText(info);
}

void CommunicationDialog::setConnectionStatus(bool connected)
{
    m_isConnected = connected;
    updateConnectionStatus();
}

void CommunicationDialog::updateConnectionStatus()
{
    if (m_isConnected)
    {
        m_labelStatus->setText("状态：🟢 已连接");
        m_labelStatus->setText(QStringLiteral("状态：🟢 已连接"));
        m_labelStatus->setStyleSheet(QStringLiteral("QLabel { padding: 5px 10px; border-radius: 3px; color: #22C55E; background-color: rgba(34, 197, 94, 0.12); }"));
    }
    else
    {
        m_labelStatus->setText("状态：🔴 未连接");
        m_labelStatus->setText(QStringLiteral("状态：🔴 未连接"));
        m_labelStatus->setStyleSheet(QStringLiteral("QLabel { padding: 5px 10px; border-radius: 3px; color: #EF4444; background-color: rgba(239, 68, 68, 0.12); }"));
    }
}

void CommunicationDialog::addMessage(const CommunicationMessage &message)
{
    if (m_isPaused)
    {
        return;
    }

    m_messages.append(message);
    const int messageIndex = m_messages.size() - 1;

    // 更新统计信息
    if (message.direction == MessageDirection::Transmit)
    {
        m_statistics.sentFrames++;
    }
    else
    {
        m_statistics.receivedFrames++;
    }

    if (message.type == MessageType::Error)
    {
        m_statistics.errorFrames++;
    }

    // 计算成功率
    int totalFrames = m_statistics.sentFrames + m_statistics.receivedFrames;
    if (totalFrames > 0)
    {
        m_statistics.successRate = 100.0 * (totalFrames - m_statistics.errorFrames) / totalFrames;
    }

    // 添加到表格
    if (shouldShowMessage(message))
    {
        int row = m_tableMessages->rowCount();
        m_tableMessages->insertRow(row);

        // 时间
        QTableWidgetItem *timeItem = new QTableWidgetItem(
            message.timestamp.toString("HH:mm:ss.zzz"));
        timeItem->setTextAlignment(Qt::AlignCenter);
        timeItem->setData(Qt::UserRole, messageIndex);
        m_tableMessages->setItem(row, 0, timeItem);

        // 方向
        QTableWidgetItem *dirItem = new QTableWidgetItem(getDirectionIcon(message.direction));
        dirItem->setTextAlignment(Qt::AlignCenter);
        m_tableMessages->setItem(row, 1, dirItem);

        QTableWidgetItem *diItem = new QTableWidgetItem(displayMessageDiCode(message.fullFrame));
        diItem->setTextAlignment(Qt::AlignCenter);
        diItem->setFont(QFont("Consolas", 9));
        m_tableMessages->setItem(row, 2, diItem);

        QTableWidgetItem *descriptionItem = new QTableWidgetItem(getMessageDiDescription(message.fullFrame));
        descriptionItem->setTextAlignment(Qt::AlignCenter);
        m_tableMessages->setItem(row, 3, descriptionItem);

        // 完整报文
        QTableWidgetItem *frameItem = new QTableWidgetItem(
            formatMessage(message, m_displayFormat));
        frameItem->setFont(QFont("Consolas", 9));
        m_tableMessages->setItem(row, 4, frameItem);

        // 设置行颜色
        if (message.type == MessageType::Error)
        {
            for (int col = 0; col < m_tableMessages->columnCount(); ++col)
            {
                m_tableMessages->item(row, col)->setBackground(QColor(255, 200, 200));
            }
        }

        // 自动滚动
        if (m_autoScroll)
        {
            m_tableMessages->scrollToBottom();
        }
    }

    updateStatistics();
    emit messageAdded(message);
}

void CommunicationDialog::clearAllMessages()
{
    m_messages.clear();
    m_tableMessages->setRowCount(0);
    m_detailWidget->clearResult();

    // 重置统计信息
    m_statistics = CommunicationStatistics();
    updateStatistics();
}

CommunicationStatistics CommunicationDialog::getStatistics() const
{
    return m_statistics;
}

bool CommunicationDialog::exportMessages(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        QMessageBox::warning(this, "导出失败", "无法打开文件进行写入");
        return false;
    }

    auto exportField = [](QString value) {
        value.replace(QChar('\t'), QChar(' '));
        value.replace(QChar('\r'), QChar(' '));
        value.replace(QChar('\n'), QChar(' '));
        return value;
    };
    auto excelTextField = [&exportField](QString value) {
        value = exportField(value);
        value.replace(QStringLiteral("\""), QStringLiteral("\"\""));
        return QStringLiteral("=\"%1\"").arg(value);
    };

    const QString separator = QStringLiteral("\t");
    QString exportText;
    exportText += QStringLiteral("时间\t方向\t数据标识\t标识说明\t控制码\t数据长度\t完整报文\t类型\r\n");
    for (const auto &message : m_messages)
    {
        const QString directionText = message.direction == MessageDirection::Transmit
                                          ? QStringLiteral("发送")
                                          : QStringLiteral("接收");
        const QString typeText = message.type == MessageType::Request
                                     ? QStringLiteral("请求")
                                     : (message.type == MessageType::Response
                                            ? QStringLiteral("应答")
                                            : QStringLiteral("错误"));

        exportText += excelTextField(message.timestamp.toString(QStringLiteral("HH:mm:ss.zzz"))) + separator +
                      exportField(directionText) + separator +
                      exportField(displayMessageDiCode(message.fullFrame)) + separator +
                      exportField(getMessageDiDescription(message.fullFrame)) + separator +
                      exportField(QStringLiteral("0x%1").arg(message.controlCode, 2, 16, QChar('0'))) + separator +
                      exportField(QString::number(message.dataLength)) + separator +
                      exportField(formatHex(message.fullFrame)) + separator +
                      exportField(typeText) + QStringLiteral("\r\n");
    }

    file.write("\xFF\xFE", 2);
    file.write(reinterpret_cast<const char *>(exportText.utf16()), exportText.size() * static_cast<int>(sizeof(ushort)));
    file.close();
    return true;
}

void CommunicationDialog::togglePause()
{
    m_isPaused = !m_isPaused;
    m_btnPause->setText(m_isPaused ? QStringLiteral("恢复") : QStringLiteral("暂停"));
}

void CommunicationDialog::refreshMessageList()
{
    m_tableMessages->setRowCount(0);

    for (int index = 0; index < m_messages.size(); ++index)
    {
        const auto &message = m_messages[index];
        if (shouldShowMessage(message))
        {
            int row = m_tableMessages->rowCount();
            m_tableMessages->insertRow(row);

            QTableWidgetItem *timeItem = new QTableWidgetItem(
                message.timestamp.toString("HH:mm:ss.zzz"));
            timeItem->setTextAlignment(Qt::AlignCenter);
            timeItem->setData(Qt::UserRole, index);
            m_tableMessages->setItem(row, 0, timeItem);

            QTableWidgetItem *dirItem = new QTableWidgetItem(getDirectionIcon(message.direction));
            dirItem->setTextAlignment(Qt::AlignCenter);
            m_tableMessages->setItem(row, 1, dirItem);

            QTableWidgetItem *diItem = new QTableWidgetItem(displayMessageDiCode(message.fullFrame));
            diItem->setTextAlignment(Qt::AlignCenter);
            diItem->setFont(QFont("Consolas", 9));
            m_tableMessages->setItem(row, 2, diItem);

            QTableWidgetItem *descriptionItem = new QTableWidgetItem(getMessageDiDescription(message.fullFrame));
            descriptionItem->setTextAlignment(Qt::AlignCenter);
            m_tableMessages->setItem(row, 3, descriptionItem);

            QTableWidgetItem *frameItem = new QTableWidgetItem(
                formatMessage(message, m_displayFormat));
            frameItem->setFont(QFont("Consolas", 9));
            m_tableMessages->setItem(row, 4, frameItem);

            if (message.type == MessageType::Error)
            {
                for (int col = 0; col < m_tableMessages->columnCount(); ++col)
                {
                    m_tableMessages->item(row, col)->setBackground(QColor(255, 200, 200));
                }
            }
        }
    }
}

void CommunicationDialog::onClearClicked()
{
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "确认清空", "确定要清空所有报文记录吗？",
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes)
    {
        clearAllMessages();
    }
}

void CommunicationDialog::onExportClicked()
{
    QString filePath = QFileDialog::getSaveFileName(
        this, "导出报文记录", "", "CSV文件 (*.csv);;文本文件 (*.txt)");

    if (!filePath.isEmpty())
    {
        if (exportMessages(filePath))
        {
            QMessageBox::information(this, "导出成功", "报文记录已成功导出");
        }
    }
}

void CommunicationDialog::onFilterChanged(const QString &text)
{
    m_filterText = text;
    refreshMessageList();
}

void CommunicationDialog::onFilterTypeChanged()
{
    m_filterRequest = m_checkBoxRequest->isChecked();
    m_filterResponse = m_checkBoxResponse->isChecked();
    m_filterError = m_checkBoxError->isChecked();
    refreshMessageList();
}

void CommunicationDialog::onDisplayFormatChanged(int index)
{
    m_displayFormat = static_cast<DisplayFormat>(index);
    refreshMessageList();
}

void CommunicationDialog::onMessageSelectionChanged()
{
    const int currentRow = m_tableMessages->currentRow();
    if (currentRow < 0)
    {
        m_detailWidget->clearResult();
        return;
    }

    QTableWidgetItem *timeItem = m_tableMessages->item(currentRow, 0);
    if (!timeItem)
    {
        m_detailWidget->clearResult();
        return;
    }

    const int messageIndex = timeItem->data(Qt::UserRole).toInt();
    if (messageIndex >= 0 && messageIndex < m_messages.size())
    {
        updateMessageDetail(messageIndex);
    }
}

void CommunicationDialog::onAutoScrollChanged(int state)
{
    m_autoScroll = (state == Qt::Checked);
}

void CommunicationDialog::updateStatistics()
{
    QString stats = QStringLiteral("发送帧数：%1  |  接收帧数：%2  |  错误帧数：%3  |  成功率：%4%")
                        .arg(m_statistics.sentFrames)
                        .arg(m_statistics.receivedFrames)
                        .arg(m_statistics.errorFrames)
                        .arg(m_statistics.successRate, 0, 'f', 1);
    m_labelStatistics->setText(stats);
}

void CommunicationDialog::parseFrame(const QByteArray &frame, QString &parsedResult)
{
    parsedResult.clear();

    if (frame.isEmpty())
    {
        return;
    }

    int leadingCount = 0;
    const QByteArray protocolFrame = DLT645DataHelper::stripLeadingFE(frame, &leadingCount);

    parsedResult += QString::fromUtf8("帧结构解析：\n");
    if (leadingCount > 0)
    {
        parsedResult += QString::fromUtf8("前导码：") + formatHex(frame.left(leadingCount)) + "\n";
    }

    if (protocolFrame.size() < 12)
    {
        parsedResult += QString::fromUtf8("报文长度不足，无法继续解析。\n");
        return;
    }

    const quint8 startFlag = static_cast<quint8>(protocolFrame[0]);
    const QByteArray address = protocolFrame.mid(1, 6);
    const quint8 secondStartFlag = static_cast<quint8>(protocolFrame[7]);
    const quint8 controlCode = static_cast<quint8>(protocolFrame[8]);
    const quint8 dataLength = static_cast<quint8>(protocolFrame[9]);
    const int availableDataLength = qMin(static_cast<int>(dataLength), protocolFrame.size() - 12);
    const QByteArray dataField = protocolFrame.mid(10, qMax(availableDataLength, 0));
    const quint8 checksum = static_cast<quint8>(protocolFrame[10 + availableDataLength]);
    const quint8 endFlag = static_cast<quint8>(protocolFrame[11 + availableDataLength]);

    parsedResult += QString::fromUtf8("帧起始符：0x%1\n").arg(startFlag, 2, 16, QChar('0')).toUpper();
    parsedResult += QString::fromUtf8("地址域：") + formatHex(address) + "\n";
    parsedResult += QString::fromUtf8("电表号：") + ProtocolParser::bytesToAddress(address) + "\n";
    parsedResult += QString::fromUtf8("第二起始符：0x%1\n").arg(secondStartFlag, 2, 16, QChar('0')).toUpper();
    parsedResult += QString::fromUtf8("控制码：0x%1 (%2)\n")
                        .arg(controlCode, 2, 16, QChar('0'))
                        .arg(getControlCodeName(controlCode));
    parsedResult += QString::fromUtf8("数据长度：0x%1 (%2 字节)\n")
                        .arg(dataLength, 2, 16, QChar('0'))
                        .arg(dataLength);
    parsedResult += QString::fromUtf8("数据域(原始)：") + formatHex(dataField) + "\n";
    if (!dataField.isEmpty())
    {
        parsedResult += QString::fromUtf8("数据域(减0x33)：") + formatHex(DLT645DataHelper::subtract33(dataField)) + "\n";
    }
    parsedResult += QString::fromUtf8("校验和：0x%1\n").arg(checksum, 2, 16, QChar('0')).toUpper();
    parsedResult += QString::fromUtf8("结束符：0x%1\n").arg(endFlag, 2, 16, QChar('0')).toUpper();
    return;

    parsedResult += QString::fromUtf8("帧结构解析：\n");
    parsedResult += QString::fromUtf8("+----------------------------------------------------+\n");

    int pos = 0;

    // 帧起始符
    if (pos < frame.size())
    {
        parsedResult += QString::fromUtf8("| 帧起始符：") + QString("0x%1").arg((quint8)frame[pos], 2, 16, QChar('0')).toUpper() + QString::fromUtf8("                                              |\n");
        pos++;
    }

    // 地址域 (6字节)
    if (pos + 6 <= frame.size())
    {
        QByteArray addr = frame.mid(pos, 6);
        QString addrHex = formatHex(addr);
        parsedResult += QString::fromUtf8("| 地址域：") + addrHex + QString::fromUtf8(" (电表地址：") + addrHex.remove(' ').remove("0x") + QString::fromUtf8(")         |\n");
        pos += 6;
    }

    // 帧起始符
    if (pos < frame.size())
    {
        parsedResult += QString::fromUtf8("| 帧起始符：") + QString("0x%1").arg((quint8)frame[pos], 2, 16, QChar('0')).toUpper() + QString::fromUtf8("                                              |\n");
        pos++;
    }

    // 控制码
    if (pos < frame.size())
    {
        quint8 code = (quint8)frame[pos];
        parsedResult += "| 控制码：" + QString("0x%1").arg(code, 2, 16, QChar('0')).toUpper() + " (" + getControlCodeName(code) + ")                                       |\n";
        pos++;
    }

    // 数据长度
    if (pos < frame.size())
    {
        quint8 len = (quint8)frame[pos];
        parsedResult += "| 数据长度：" + QString("0x%1").arg(len, 2, 16, QChar('0')).toUpper() + " (" + QString::number(len) + " 字节)                                      |\n";
        pos++;
    }

    // 数据域
    if (pos < frame.size())
    {
        int dataLen = qMin((int)frame[pos], frame.size() - pos - 2); // 减去校验和和结束符
        if (dataLen > 0)
        {
            QByteArray data = frame.mid(pos + 1, dataLen);
            parsedResult += QString::fromUtf8("| 数据域：") + formatHex(data) + QString::fromUtf8("                       |\n");
            pos += 1 + dataLen;
        }
        else
        {
            // 数据长度为0，跳过数据域
            pos += 1;
        }
    }

    // 校验和
    if (pos < frame.size())
    {
        quint8 checksum = (quint8)frame[pos];
        parsedResult += QString::fromUtf8("| 校验和：") + QString("0x%1").arg(checksum, 2, 16, QChar('0')).toUpper() + QString::fromUtf8(" OK                                              |\n");
        pos++;
    }

    // 结束符
    if (pos < frame.size())
    {
        parsedResult += QString::fromUtf8("| 结束符：") + QString("0x%1").arg((quint8)frame[pos], 2, 16, QChar('0')).toUpper() + QString::fromUtf8("                                                |\n");
    }

    parsedResult += QString::fromUtf8("+----------------------------------------------------+\n");
}

QString CommunicationDialog::formatMessage(const CommunicationMessage &message, DisplayFormat format) const
{
    if (format == DisplayFormat::Hex)
    {
        return formatHex(message.fullFrame);
    }
    else
    {
        return formatAscii(message.fullFrame);
    }
}

QString CommunicationDialog::formatHex(const QByteArray &data) const
{
    return DLT645FrameFormatter::formatHex(data);
}

QString CommunicationDialog::formatAscii(const QByteArray &data) const
{
    QString result;
    for (int i = 0; i < data.size(); ++i)
    {
        quint8 byte = (quint8)data[i];
        if (byte >= 32 && byte <= 126)
        {
            result += QChar(byte);
        }
        else
        {
            result += ".";
        }
    }
    return result;
}

QString CommunicationDialog::getMessageDiDescription(const QByteArray &fullFrame) const
{
    QString diCode = displayMessageDiCode(fullFrame);
    if (diCode == QStringLiteral("-") || diCode.isEmpty())
    {
        return QStringLiteral("-");
    }

    diCode.remove(QStringLiteral("0x"), Qt::CaseInsensitive);
    diCode = QStringLiteral("0x") + diCode.toUpper().rightJustified(8, QChar('0'));

    QString description;
    if (m_meterModel)
    {
        description = m_meterModel->getDataName(diCode);
    }

    if (description.isEmpty())
    {
        static MeterModel defaultMeterModel;
        description = defaultMeterModel.getDataName(diCode);
    }

    return description.isEmpty() ? QStringLiteral("-") : description;
}

QString CommunicationDialog::getControlCodeName(quint8 code) const
{
    switch (code)
    {
    case 0x11:
        return "读数据";
    case 0x12:
        return "读后续数据";
    case 0x13:
        return "读通信地址";
    case 0x14:
        return "写数据";
    case 0x15:
        return "广播校时";
    case 0x16:
        return "写设备地址";
    case 0x17:
        return "更改通信速率";
    case 0x18:
        return "修改密码";
    case 0x19:
        return "最大需量清零";
    case 0x1A:
        return "电表清零";
    case 0x1B:
        return "事件清零";
    case 0x91:
        return "正常应答";
    case 0x92:
        return "后续应答";
    case 0x94:
        return "Write Response";
    case 0xD1:
        return "异常应答";
    case 0xD2:
        return "不支持";
    default:
        return "未知";
    }
}

QString CommunicationDialog::getMessageTypeIcon(MessageType type) const
{
    switch (type)
    {
    case MessageType::Request:
        return "📤";
    case MessageType::Response:
        return "📥";
    case MessageType::Error:
        return "❌";
    default:
        return "";
    }
}

QString CommunicationDialog::getDirectionIcon(MessageDirection direction) const
{
    switch (direction)
    {
    case MessageDirection::Transmit:
        return "T ->";
    case MessageDirection::Receive:
        return "R <-";
    default:
        return "";
    }
}

bool CommunicationDialog::shouldShowMessage(const CommunicationMessage &message) const
{
    // 类型过滤
    if (message.type == MessageType::Request && !m_filterRequest)
    {
        return false;
    }
    if (message.type == MessageType::Response && !m_filterResponse)
    {
        return false;
    }
    if (message.type == MessageType::Error && !m_filterError)
    {
        return false;
    }

    // 文本过滤
    if (!m_filterText.isEmpty())
    {
        QString frameText = formatHex(message.fullFrame);
        if (!frameText.contains(m_filterText, Qt::CaseInsensitive))
        {
            return false;
        }
    }

    return true;
}

void CommunicationDialog::updateMessageDetail(int row)
{
    if (row < 0 || row >= m_messages.size())
    {
        m_detailWidget->clearResult();
        return;
    }

    const CommunicationMessage &message = m_messages[row];

    m_detailWidget->setParseResult(
        DLT645FrameFormatter::buildParseDisplayResult(message.fullFrame, m_meterModel, message.checksumValid));
    return;

#if 0

    QString detail;

    // 帧结构解析
    QString frameParse;
    parseFrame(message.fullFrame, frameParse);
    detail += frameParse + "\n";

    detail += QString::fromUtf8("数据解析：\n");

    const QByteArray protocolFrame = DLT645DataHelper::stripLeadingFE(message.fullFrame);
    if (protocolFrame.size() >= 12)
    {
        const quint8 dataLength = static_cast<quint8>(protocolFrame[9]);
        const int actualDataLength = qMin(static_cast<int>(dataLength), protocolFrame.size() - 12);
        const QByteArray dataField = protocolFrame.mid(10, qMax(actualDataLength, 0));
        const quint8 baseControlCode = message.controlCode & 0x7F;
        const bool isResponse = (message.controlCode & 0x80) != 0;

        QString diCode;
        QString description;
        QString unit;
        QByteArray decodedPayload;
        QString payloadLabel;

        if (dataField.size() >= 4 && (baseControlCode == 0x11 || baseControlCode == 0x14))
        {
            diCode = ProtocolParser::extractDIFromDataField(dataField);
        }

        if (baseControlCode == 0x11)
        {
            if (isResponse)
            {
                decodedPayload = DLT645DataHelper::subtract33(dataField.mid(4));
                payloadLabel = QString::fromUtf8("应答数据");
            }
            else
            {
                decodedPayload = dataField.mid(4);
                payloadLabel = QString::fromUtf8("请求参数");
            }
        }
        else if (baseControlCode == 0x14)
        {
            if (isResponse)
            {
                decodedPayload = dataField.mid(5);
                payloadLabel = QString::fromUtf8("写入状态");
            }
            else if (dataField.size() > 5)
            {
                const int payloadLength = static_cast<quint8>(dataField[4]);
                decodedPayload = DLT645DataHelper::subtract33(dataField.mid(5, payloadLength > 0 ? payloadLength : -1));
                payloadLabel = QString::fromUtf8("写入数据");
            }
        }

        if (!diCode.isEmpty())
        {
            detail += QString::fromUtf8("数据项标识：") + diCode + "\n";
            if (m_meterModel)
            {
                const DataItem dataItem = m_meterModel->getDataItem(diCode);
                description = dataItem.description;
                unit = dataItem.unit;

                if (!description.isEmpty())
                {
                    detail += QString::fromUtf8("数据项名称：") + description + "\n";
                }

                if (!decodedPayload.isEmpty())
                {
                    const QString decodedValue = DLT645DataHelper::decodePayloadForItem(m_meterModel, diCode, decodedPayload, &description, &unit);
                    detail += payloadLabel + QString::fromUtf8("(解码字节)：") + formatHex(decodedPayload) + "\n";
                    detail += QString::fromUtf8("解析值：") + decodedValue;
                    if (!unit.isEmpty() && unit != "-")
                    {
                        detail += " " + unit;
                    }
                    detail += "\n";
                }

                // 只在应答帧(Response)时显示MeterModel当前值，请求帧(Request)不显示
                if (message.type == MessageType::Response)
                {
                    const QVariant modelValue = m_meterModel->getDataValue(diCode);
                    if (modelValue.isValid())
                    {
                        DataItem currentItem = dataItem;
                        currentItem.value = modelValue;
                        const QString modelText = currentItem.getFormattedValue();
                        if (!modelText.isEmpty())
                        {
                            detail += QString::fromUtf8("MeterModel当前值：") + modelText;
                            if (!unit.isEmpty() && unit != "-")
                            {
                                detail += " " + unit;
                            }
                            detail += "\n";
                        }
                    }
                }
            }
        }
        else if (!decodedPayload.isEmpty())
        {
            detail += payloadLabel + QString::fromUtf8("：") + formatHex(decodedPayload) + "\n";
        }
    }

    detail += QString::fromUtf8("\n校验和状态：") + (message.checksumValid ? QString::fromUtf8("OK 有效") : QString::fromUtf8("X 无效")) + "\n";
    m_textDetail->setText(detail);
    return;

    // 数据解析
    detail += "数据解析：\n";

    // 从数据域中提取数据标识（前4个字节）
    // 帧结构：68(1) + 地址(6) + 68(1) + 控制码(1) + 长度(1) + 数据域(N) + 校验和(1) + 16(1)
    // 数据域起始位置：1 + 6 + 1 + 1 + 1 = 10
    int dataFieldStart = 10;
    if (message.fullFrame.size() >= dataFieldStart + 4)
    {
        QByteArray diBytes = message.fullFrame.mid(dataFieldStart, 4);
        QString diHex = formatHex(diBytes).remove(' ').remove("0x");
        detail += "数据标识：0x" + diHex + "\n";
        detail += "数据项：" + getControlCodeName(message.controlCode) + "\n";
    }
    else
    {
        detail += "数据标识：N/A\n";
        detail += "数据项：" + getControlCodeName(message.controlCode) + "\n";
    }
    detail += "数据值：" + message.parsedData + "\n";

    // 校验和状态
    detail += QString::fromUtf8("\n校验和状态：") + (message.checksumValid ? QString::fromUtf8("OK 有效") : QString::fromUtf8("X 无效")) + QString::fromUtf8("\n");

    m_textDetail->setText(detail);
#endif
}

void CommunicationDialog::closeEvent(QCloseEvent *event)
{
    emit dialogClosed();
    QDialog::closeEvent(event);
}

void CommunicationDialog::applyDarkTheme()
{
    QString styleSheet = R"(
        QDialog {
            background-color: #2B2B2B;
        }

        QGroupBox {
            color: #FFFFFF;
            border: 1px solid #555555;
            border-radius: 4px;
            margin-top: 10px;
            padding-top: 10px;
        }

        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 4px;
        }

        QLabel {
            color: #FFFFFF;
            font-family: "Microsoft YaHei";
        }

        QLabel#infoLabel,
        QLabel#statisticsLabel {
            padding: 5px 0;
        }

        QPushButton {
            background-color: #3C3F41;
            color: #FFFFFF;
            border: 1px solid #555555;
            border-radius: 3px;
            padding: 6px 14px;
        }

        QPushButton:hover {
            background-color: #4E5254;
        }

        QPushButton:pressed {
            background-color: #2B2B2B;
        }

        QLineEdit,
        QComboBox {
            background-color: #3C3F41;
            color: #FFFFFF;
            border: 1px solid #555555;
            border-radius: 3px;
            padding: 6px 8px;
        }

        QLineEdit:focus,
        QComboBox:focus {
            border: 1px solid #094771;
        }

        QComboBox::drop-down {
            border: none;
        }

        QComboBox QAbstractItemView {
            background-color: #3C3F41;
            color: #FFFFFF;
            selection-background-color: #094771;
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

        QTableWidget {
            background-color: #3C3F41;
            color: #FFFFFF;
            border: 1px solid #555555;
            border-radius: 3px;
            gridline-color: #4B4F51;
        }

        QTableWidget::item {
            padding: 5px;
            border-bottom: 1px solid #4B4F51;
        }

        QTableWidget::item:selected {
            background-color: #094771;
            color: #FFFFFF;
        }

        QHeaderView::section {
            background-color: #3C3F41;
            color: #FFFFFF;
            padding: 5px;
            border: none;
            border-bottom: 1px solid #555555;
        }

        QTextEdit,
        QTextEdit#detailTextEdit {
            background-color: #3C3F41;
            color: #FFFFFF;
            border: 1px solid #555555;
            border-radius: 3px;
            padding: 8px;
        }

        QTextEdit:focus,
        QTextEdit#detailTextEdit:focus {
            border: 1px solid #094771;
        }

        QSplitter::handle {
            background-color: #555555;
        }
    )";

    setStyleSheet(styleSheet);

    if (m_detailWidget)
    {
        m_detailWidget->applyDarkTheme();
    }
}

void CommunicationDialog::applyLightTheme()
{
    QString styleSheet = R"(
        QDialog {
            background-color: #FFFFFF;
        }

        QGroupBox {
            color: #000000;
            border: 1px solid #CCCCCC;
            border-radius: 4px;
            margin-top: 10px;
            padding-top: 10px;
        }

        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 4px;
        }

        QLabel {
            color: #000000;
            font-family: "Microsoft YaHei";
        }

        QLabel#infoLabel,
        QLabel#statisticsLabel {
            padding: 5px 0;
        }

        QPushButton {
            background-color: #F0F0F0;
            color: #000000;
            border: 1px solid #CCCCCC;
            border-radius: 3px;
            padding: 6px 14px;
        }

        QPushButton:hover {
            background-color: #E0E0E0;
        }

        QPushButton:pressed {
            background-color: #D0D0D0;
        }

        QLineEdit,
        QComboBox {
            background-color: #FFFFFF;
            color: #000000;
            border: 1px solid #CCCCCC;
            border-radius: 3px;
            padding: 6px 8px;
        }

        QLineEdit:focus,
        QComboBox:focus {
            border: 1px solid #0078D7;
        }

        QComboBox::drop-down {
            border: none;
        }

        QComboBox QAbstractItemView {
            background-color: #FFFFFF;
            color: #000000;
            selection-background-color: #0078D7;
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

        QTableWidget {
            background-color: #FFFFFF;
            color: #000000;
            border: 1px solid #CCCCCC;
            border-radius: 3px;
            gridline-color: #E0E0E0;
        }

        QTableWidget::item {
            padding: 5px;
            border-bottom: 1px solid #E0E0E0;
        }

        QTableWidget::item:selected {
            background-color: #0078D7;
            color: #FFFFFF;
        }

        QHeaderView::section {
            background-color: #F0F0F0;
            color: #000000;
            padding: 5px;
            border: none;
            border-bottom: 1px solid #CCCCCC;
        }

        QTextEdit,
        QTextEdit#detailTextEdit {
            background-color: #FFFFFF;
            color: #000000;
            border: 1px solid #CCCCCC;
            border-radius: 3px;
            padding: 8px;
        }

        QTextEdit:focus,
        QTextEdit#detailTextEdit:focus {
            border: 1px solid #0078D7;
        }

        QSplitter::handle {
            background-color: #CCCCCC;
        }
    )";

    setStyleSheet(styleSheet);

    if (m_detailWidget)
    {
        m_detailWidget->applyLightTheme();
    }
}
