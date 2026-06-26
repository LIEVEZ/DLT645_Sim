#include "OperationLogWidget.h"

#include "Utils/UiTheme.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QColor>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QIODevice>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QTextStream>
#include <QVariant>
#include <QVBoxLayout>

OperationLogWidget::OperationLogWidget(QWidget *parent)
    : QWidget(parent), m_logListWidget(new QListWidget(this)), m_filterEdit(new QLineEdit(this)), m_autoScrollCheckBox(new QCheckBox("自动滚动", this)), m_clearButton(new QPushButton("清空", this)), m_exportButton(new QPushButton("导出", this)), m_maxLogLines(10000), m_autoScrollEnabled(true), m_darkTheme(true)
{
    setupUI();
}

OperationLogWidget::~OperationLogWidget()
{
}

void OperationLogWidget::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(5, 5, 5, 5);
    mainLayout->setSpacing(5);

    // 顶部工具栏
    QHBoxLayout *toolBarLayout = new QHBoxLayout();
    toolBarLayout->setSpacing(10);

    m_filterEdit->setPlaceholderText("过滤日志...");
    m_filterEdit->setClearButtonEnabled(true);
    connect(m_filterEdit, &QLineEdit::textChanged, this, &OperationLogWidget::onFilterTextChanged);
    toolBarLayout->addWidget(m_filterEdit);

    m_autoScrollCheckBox->setChecked(true);
    connect(m_autoScrollCheckBox, &QCheckBox::toggled, this, &OperationLogWidget::onAutoScrollToggled);
    toolBarLayout->addWidget(m_autoScrollCheckBox);

    toolBarLayout->addStretch();

    m_clearButton->setMaximumWidth(80);
    connect(m_clearButton, &QPushButton::clicked, this, &OperationLogWidget::onClearClicked);
    toolBarLayout->addWidget(m_clearButton);

    m_exportButton->setMaximumWidth(80);
    connect(m_exportButton, &QPushButton::clicked, this, &OperationLogWidget::onExportClicked);
    toolBarLayout->addWidget(m_exportButton);

    mainLayout->addLayout(toolBarLayout);

    // 日志列表
    m_logListWidget->setSelectionMode(QAbstractItemView::NoSelection);
    m_logListWidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    mainLayout->addWidget(m_logListWidget);
}

void OperationLogWidget::addLogEntry(const LogEntry &entry)
{
    addLogItem(entry);

    // 检查是否超过最大行数
    while (m_logListWidget->count() > m_maxLogLines)
    {
        delete m_logListWidget->takeItem(0);
    }

    // 自动滚动到底部
    if (m_autoScrollEnabled)
    {
        m_logListWidget->scrollToBottom();
    }

    emit logEntryAdded(entry);
}

void OperationLogWidget::addInfo(const QString &message)
{
    LogEntry entry(QDateTime::currentDateTime(), OperationLogLevel::Info, "系统", message);
    addLogEntry(entry);
}

void OperationLogWidget::addSuccess(const QString &message)
{
    LogEntry entry(QDateTime::currentDateTime(), OperationLogLevel::Success, "数据", message);
    addLogEntry(entry);
}

void OperationLogWidget::addCommunication(const QString &message)
{
    LogEntry entry(QDateTime::currentDateTime(), OperationLogLevel::Communication, "通信", message);
    addLogEntry(entry);
}

void OperationLogWidget::addWarning(const QString &message)
{
    LogEntry entry(QDateTime::currentDateTime(), OperationLogLevel::Warning, "错误", message);
    addLogEntry(entry);
}

void OperationLogWidget::addError(const QString &message)
{
    LogEntry entry(QDateTime::currentDateTime(), OperationLogLevel::Error, "错误", message);
    addLogEntry(entry);
}

void OperationLogWidget::clearLogs()
{
    m_logListWidget->clear();
}

void OperationLogWidget::setMaxLogLines(int maxLines)
{
    m_maxLogLines = maxLines;
}

void OperationLogWidget::setAutoScroll(bool enabled)
{
    m_autoScrollEnabled = enabled;
    m_autoScrollCheckBox->setChecked(enabled);
}

void OperationLogWidget::onClearClicked()
{
    clearLogs();
}

void OperationLogWidget::onExportClicked()
{
    QString fileName = QFileDialog::getSaveFileName(this, "导出日志",
                                                    "文本文件 (*.txt);;所有文件 (*.*)",
                                                    "operation_log.txt");

    if (fileName.isEmpty())
    {
        return;
    }

    QFile file(fileName);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QTextStream out(&file);
        out.setCodec("UTF-8");

        for (int i = 0; i < m_logListWidget->count(); ++i)
        {
            QListWidgetItem *item = m_logListWidget->item(i);
            LogEntry entry = item->data(Qt::UserRole).value<LogEntry>();

            QString line = QString("[%1] [%2] %3")
                               .arg(entry.timestamp.toString("HH:mm:ss"))
                               .arg(getLevelIcon(entry.level))
                               .arg(entry.message);

            out << line << "\n";
        }

        file.close();
    }
}

void OperationLogWidget::onFilterTextChanged(const QString &text)
{
    QString filterText = text.toLower();

    for (int i = 0; i < m_logListWidget->count(); ++i)
    {
        QListWidgetItem *item = m_logListWidget->item(i);
        LogEntry entry = item->data(Qt::UserRole).value<LogEntry>();

        bool match = filterText.isEmpty() ||
                     entry.message.toLower().contains(filterText) ||
                     entry.category.toLower().contains(filterText);

        item->setHidden(!match);
    }
}

void OperationLogWidget::onAutoScrollToggled(bool checked)
{
    m_autoScrollEnabled = checked;
}

void OperationLogWidget::addLogItem(const LogEntry &entry)
{
    QString icon = getLevelIcon(entry.level);
    QString color = getLevelColor(entry.level);

    QString text = QString("[%1] [%2] %3")
                       .arg(icon)
                       .arg(entry.category)
                       .arg(entry.message);

    QListWidgetItem *item = new QListWidgetItem();
    item->setText(text);
    item->setData(Qt::UserRole, QVariant::fromValue(entry));
    item->setForeground(QColor(color));

    m_logListWidget->addItem(item);
}

QString OperationLogWidget::getLevelIcon(OperationLogLevel level) const
{
    switch (level)
    {
    case OperationLogLevel::Info:
        return "ℹ️";
    case OperationLogLevel::Success:
        return "✅";
    case OperationLogLevel::Communication:
        return "📤";
    case OperationLogLevel::Warning:
        return "⚠️";
    default:
        return "❌";
    }
}

QString OperationLogWidget::getLevelColor(OperationLogLevel level) const
{
    // 日志级别色取自全局调色板（Utils/UiTheme），随暗/亮主题切换。
    const ui::ThemePalette p = m_darkTheme ? ui::darkPalette() : ui::lightPalette();
    switch (level)
    {
    case OperationLogLevel::Info:
        return p.logInfo;
    case OperationLogLevel::Success:
        return p.logSuccess;
    case OperationLogLevel::Communication:
        return p.logComm;
    case OperationLogLevel::Warning:
        return p.logWarning;
    default:
        return p.logError;
    }
}

void OperationLogWidget::applyDarkTheme()
{
    m_darkTheme = true;
    applyTheme(ui::darkPalette());
}

void OperationLogWidget::applyLightTheme()
{
    m_darkTheme = false;
    applyTheme(ui::lightPalette());
}

void OperationLogWidget::applyTheme(const ui::ThemePalette &p)
{
    // 列表容器底色 / 文字随主题切换；具体级别色在 recolorAllItems 中按条目套用。
    m_logListWidget->setStyleSheet(
        QString("QListWidget { background-color: %1; border: none; outline: none; }"
                "QListWidget::item { padding: 2px 4px; }")
            .arg(p.windowBg));
    recolorAllItems();
}

void OperationLogWidget::recolorAllItems()
{
    // 主题切换后，用新调色板重新着色已有日志条目（颜色不再随首次写入固定）。
    for (int i = 0; i < m_logListWidget->count(); ++i)
    {
        QListWidgetItem *item = m_logListWidget->item(i);
        const LogEntry entry = item->data(Qt::UserRole).value<LogEntry>();
        item->setForeground(QColor(getLevelColor(entry.level)));
    }
}
