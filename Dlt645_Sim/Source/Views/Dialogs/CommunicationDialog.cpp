#include "CommunicationDialog.h"
#include <QHeaderView>
#include <QFileDialog>
#include <QMessageBox>
#include <QTextStream>
#include <QSplitter>
#include <QDateTime>
#include <QCloseEvent>

CommunicationDialog::CommunicationDialog(const QString &connectionName, QWidget *parent)
    : QDialog(parent), m_connectionName(connectionName), m_portName(""), m_baudRate(2400), m_isConnected(false), m_isPaused(false), m_autoScroll(true), m_displayFormat(DisplayFormat::Hex), m_filterRequest(true), m_filterResponse(true), m_filterError(true), m_filterText(""), m_connectionInfoBar(nullptr), m_filterBar(nullptr)
{
    setupUI();
    setWindowTitle(QString("通信交互 - %1").arg(connectionName));
    resize(800, 600);
    // 不设置WA_DeleteOnClose，由主窗口管理窗口生命周期
}

CommunicationDialog::~CommunicationDialog()
{
}

void CommunicationDialog::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(5);
    mainLayout->setContentsMargins(10, 10, 10, 10);

    // 连接信息栏
    setupConnectionInfoBar();
    mainLayout->addWidget(m_connectionInfoBar);

    // 过滤栏
    setupFilterBar();
    mainLayout->addWidget(m_filterBar);

    // 创建分割器
    QSplitter *splitter = new QSplitter(Qt::Vertical, this);

    // 报文列表
    setupMessageTable();
    splitter->addWidget(m_tableMessages);

    // 报文详情
    setupMessageDetail();
    splitter->addWidget(m_textDetail);

    // 设置分割器比例
    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({400, 200});

    mainLayout->addWidget(splitter);

    // 统计信息栏
    setupStatisticsBar();
    mainLayout->addWidget(m_labelStatistics);
}

void CommunicationDialog::setupConnectionInfoBar()
{
    // 创建容器控件
    m_connectionInfoBar = new QWidget(this);
    QHBoxLayout *layout = new QHBoxLayout(m_connectionInfoBar);
    layout->setSpacing(10);
    layout->setContentsMargins(5, 5, 5, 5);

    // 连接信息标签
    m_labelConnectionInfo = new QLabel(m_connectionInfoBar);
    m_labelConnectionInfo->setStyleSheet("QLabel { padding: 5px; border-radius: 3px; }");
    m_labelConnectionInfo->setText("连接：未设置");

    // 状态标签
    m_labelStatus = new QLabel(m_connectionInfoBar);
    m_labelStatus->setStyleSheet("QLabel { padding: 5px; border-radius: 3px; }");
    updateConnectionStatus();

    // 清空按钮
    m_btnClear = new QPushButton("清空", m_connectionInfoBar);
    m_btnClear->setMinimumWidth(80);
    connect(m_btnClear, &QPushButton::clicked, this, &CommunicationDialog::onClearClicked);

    // 暂停按钮
    m_btnPause = new QPushButton("暂停", m_connectionInfoBar);
    m_btnPause->setMinimumWidth(80);
    connect(m_btnPause, &QPushButton::clicked, this, &CommunicationDialog::togglePause);

    // 导出按钮
    m_btnExport = new QPushButton("导出", m_connectionInfoBar);
    m_btnExport->setMinimumWidth(80);
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
    m_filterBar = new QWidget(this);
    QHBoxLayout *layout = new QHBoxLayout(m_filterBar);
    layout->setSpacing(10);
    layout->setContentsMargins(5, 5, 5, 5);

    // 过滤输入框
    m_lineEditFilter = new QLineEdit(m_filterBar);
    m_lineEditFilter->setPlaceholderText("过滤报文内容...");
    connect(m_lineEditFilter, &QLineEdit::textChanged, this, &CommunicationDialog::onFilterChanged);

    // 类型过滤复选框
    m_checkBoxRequest = new QCheckBox("请求帧", m_filterBar);
    m_checkBoxRequest->setChecked(true);
    connect(m_checkBoxRequest, &QCheckBox::stateChanged, this, &CommunicationDialog::onFilterTypeChanged);

    m_checkBoxResponse = new QCheckBox("应答帧", m_filterBar);
    m_checkBoxResponse->setChecked(true);
    connect(m_checkBoxResponse, &QCheckBox::stateChanged, this, &CommunicationDialog::onFilterTypeChanged);

    m_checkBoxError = new QCheckBox("错误帧", m_filterBar);
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
}

void CommunicationDialog::setupMessageTable()
{
    m_tableMessages = new QTableWidget(this);
    m_tableMessages->setColumnCount(3);
    m_tableMessages->setHorizontalHeaderLabels({QString::fromUtf8("时间"),
                                                QString::fromUtf8("方向"),
                                                QString::fromUtf8("完整报文")});
    m_tableMessages->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableMessages->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableMessages->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tableMessages->verticalHeader()->setVisible(false);

    // 设置列宽
    m_tableMessages->setColumnWidth(0, 120); // 时间
    m_tableMessages->setColumnWidth(1, 80);  // 方向
    m_tableMessages->setColumnWidth(2, 400); // 完整报文

    // 表头样式
    m_tableMessages->horizontalHeader()->setStretchLastSection(true);

    connect(m_tableMessages, &QTableWidget::itemSelectionChanged,
            this, &CommunicationDialog::onMessageSelectionChanged);
}

void CommunicationDialog::setupMessageDetail()
{
    m_textDetail = new QTextEdit(this);
    m_textDetail->setReadOnly(true);
    m_textDetail->setFont(QFont("Consolas", 9));
}

void CommunicationDialog::setupStatisticsBar()
{
    m_labelStatistics = new QLabel(this);
    m_labelStatistics->setStyleSheet(
        "QLabel { padding: 5px; border-radius: 3px; }");
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
        m_labelStatus->setStyleSheet("QLabel { padding: 5px; border-radius: 3px; color: green; }");
    }
    else
    {
        m_labelStatus->setText("状态：🔴 未连接");
        m_labelStatus->setStyleSheet("QLabel { padding: 5px; border-radius: 3px; color: red; }");
    }
}

void CommunicationDialog::addMessage(const CommunicationMessage &message)
{
    if (m_isPaused)
    {
        return;
    }

    m_messages.append(message);

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
        m_tableMessages->setItem(row, 0, timeItem);

        // 方向
        QTableWidgetItem *dirItem = new QTableWidgetItem(getDirectionIcon(message.direction));
        dirItem->setTextAlignment(Qt::AlignCenter);
        m_tableMessages->setItem(row, 1, dirItem);

        // 完整报文
        QTableWidgetItem *frameItem = new QTableWidgetItem(
            formatMessage(message, m_displayFormat));
        frameItem->setFont(QFont("Consolas", 9));
        m_tableMessages->setItem(row, 2, frameItem);

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
    m_textDetail->clear();

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
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QMessageBox::warning(this, "导出失败", "无法打开文件进行写入");
        return false;
    }

    QTextStream out(&file);
    out.setCodec("UTF-8");

    // 写入表头
    out << "时间,方向,控制码,数据长度,完整报文,类型\n";

    // 写入数据
    for (const auto &message : m_messages)
    {
        out << message.timestamp.toString("HH:mm:ss.zzz") << ","
            << (message.direction == MessageDirection::Transmit ? "发送" : "接收") << ","
            << QString("0x%1").arg(message.controlCode, 2, 16, QChar('0')) << ","
            << message.dataLength << ","
            << formatHex(message.fullFrame) << ","
            << (message.type == MessageType::Request ? "请求" : message.type == MessageType::Response ? "应答"
                                                                                                      : "错误")
            << "\n";
    }

    file.close();
    return true;
}

void CommunicationDialog::togglePause()
{
    m_isPaused = !m_isPaused;
    m_btnPause->setText(m_isPaused ? "恢复" : "暂停");
}

void CommunicationDialog::refreshMessageList()
{
    m_tableMessages->setRowCount(0);

    for (const auto &message : m_messages)
    {
        if (shouldShowMessage(message))
        {
            int row = m_tableMessages->rowCount();
            m_tableMessages->insertRow(row);

            QTableWidgetItem *timeItem = new QTableWidgetItem(
                message.timestamp.toString("HH:mm:ss.zzz"));
            timeItem->setTextAlignment(Qt::AlignCenter);
            m_tableMessages->setItem(row, 0, timeItem);

            QTableWidgetItem *dirItem = new QTableWidgetItem(getDirectionIcon(message.direction));
            dirItem->setTextAlignment(Qt::AlignCenter);
            m_tableMessages->setItem(row, 1, dirItem);

            QTableWidgetItem *codeItem = new QTableWidgetItem(
                QString("%1H").arg(message.controlCode, 2, 16, QChar('0')).toUpper());
            codeItem->setTextAlignment(Qt::AlignCenter);
            m_tableMessages->setItem(row, 2, codeItem);

            QTableWidgetItem *lenItem = new QTableWidgetItem(
                QString("%1").arg(message.dataLength));
            lenItem->setTextAlignment(Qt::AlignCenter);
            m_tableMessages->setItem(row, 3, lenItem);

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
    int currentRow = m_tableMessages->currentRow();
    if (currentRow >= 0 && currentRow < m_messages.size())
    {
        updateMessageDetail(currentRow);
    }
}

void CommunicationDialog::onAutoScrollChanged(int state)
{
    m_autoScroll = (state == Qt::Checked);
}

void CommunicationDialog::updateStatistics()
{
    QString stats = QString("发送帧数：%1  |  接收帧数：%2  |  错误帧数：%3  |  成功率：%4%")
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
        parsedResult += QString::fromUtf8("| 地址域：") + addrHex + QString::fromUtf8(" (电表地址：") + addrHex.remove(' ').remove('0x') + QString::fromUtf8(")         |\n");
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
    QString result;
    for (int i = 0; i < data.size(); ++i)
    {
        result += QString("%1 ").arg((quint8)data[i], 2, 16, QChar('0')).toUpper();
    }
    return result.trimmed();
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
        m_textDetail->clear();
        return;
    }

    const CommunicationMessage &message = m_messages[row];

    QString detail;

    // 帧结构解析
    QString frameParse;
    parseFrame(message.fullFrame, frameParse);
    detail += frameParse + "\n";

    // 数据解析
    detail += "数据解析：\n";

    // 从数据域中提取数据标识（前4个字节）
    // 帧结构：68(1) + 地址(6) + 68(1) + 控制码(1) + 长度(1) + 数据域(N) + 校验和(1) + 16(1)
    // 数据域起始位置：1 + 6 + 1 + 1 + 1 = 10
    int dataFieldStart = 10;
    if (message.fullFrame.size() >= dataFieldStart + 4)
    {
        QByteArray diBytes = message.fullFrame.mid(dataFieldStart, 4);
        QString diHex = formatHex(diBytes).remove(' ').remove('0x');
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
        
        QLabel {
            color: #FFFFFF;
            font-family: "Microsoft YaHei";
        }
        
        QPushButton {
            background-color: #3C3F41;
            color: #FFFFFF;
            border: 1px solid #555555;
            border-radius: 3px;
            padding: 5px 15px;
        }
        
        QPushButton:hover {
            background-color: #4E5254;
        }
        
        QPushButton:pressed {
            background-color: #2B2B2B;
        }
        
        QLineEdit {
            background-color: #3C3F41;
            color: #FFFFFF;
            border: 1px solid #555555;
            border-radius: 3px;
            padding: 5px;
        }
        
        QLineEdit:focus {
            border: 1px solid #094771;
        }
        
        QComboBox {
            background-color: #3C3F41;
            color: #FFFFFF;
            border: 1px solid #555555;
            border-radius: 3px;
            padding: 5px;
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
            background-color: #2B2B2B;
            color: #FFFFFF;
            border: none;
            gridline-color: #3C3F41;
        }
        
        QTableWidget::item {
            padding: 5px;
            border-bottom: 1px solid #3C3F41;
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
        
        QTextEdit {
            background-color: #2B2B2B;
            color: #FFFFFF;
            padding: 10px;
        }
        
        QSplitter::handle {
            background-color: #555555;
        }
    )";

    setStyleSheet(styleSheet);
}

void CommunicationDialog::applyLightTheme()
{
    QString styleSheet = R"(
        QDialog {
            background-color: #FFFFFF;
        }
        
        QLabel {
            color: #000000;
            font-family: "Microsoft YaHei";
        }
        
        QPushButton {
            background-color: #F0F0F0;
            color: #000000;
            border: 1px solid #CCCCCC;
            border-radius: 3px;
            padding: 5px 15px;
        }
        
        QPushButton:hover {
            background-color: #E0E0E0;
        }
        
        QPushButton:pressed {
            background-color: #D0D0D0;
        }
        
        QLineEdit {
            background-color: #FFFFFF;
            color: #000000;
            border: 1px solid #CCCCCC;
            border-radius: 3px;
            padding: 5px;
        }
        
        QLineEdit:focus {
            border: 1px solid #0078D7;
        }
        
        QComboBox {
            background-color: #FFFFFF;
            color: #000000;
            border: 1px solid #CCCCCC;
            border-radius: 3px;
            padding: 5px;
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
            border: none;
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
        
        QTextEdit {
            background-color: #FFFFFF;
            color: #000000;
            padding: 10px;
        }
        
        QSplitter::handle {
            background-color: #CCCCCC;
        }
    )";

    setStyleSheet(styleSheet);
}
