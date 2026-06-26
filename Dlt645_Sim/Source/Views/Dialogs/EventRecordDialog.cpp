#include "EventRecordDialog.h"
#include "Models/MeterModel.h"
#include "Models/CategoryCatalog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QWidget>

EventRecordDialog::EventRecordDialog(MeterModel *meterModel, QWidget *parent)
    : QDialog(parent),
      m_meterModel(meterModel),
      m_infoLabel(new QLabel(this)),
      m_globalCheckBox(new QCheckBox("全局：事件记录存在", this)),
      m_scrollArea(new QScrollArea(this)),
      m_okButton(new QPushButton("确定", this)),
      m_cancelButton(new QPushButton("取消", this))
{
    setupUI();
    loadMeterSettings();
}

EventRecordDialog::~EventRecordDialog()
{
}

void EventRecordDialog::setupUI()
{
    setWindowTitle("事件记录设置");
    setMinimumWidth(450);
    setMinimumHeight(500);
    resize(450, 500);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);

    m_infoLabel->setWordWrap(true);
    mainLayout->addWidget(m_infoLabel);

    m_globalCheckBox->setChecked(true);
    connect(m_globalCheckBox, &QCheckBox::toggled, this, &EventRecordDialog::onGlobalCheckBoxToggled);
    mainLayout->addWidget(m_globalCheckBox);

    QWidget *scrollWidget = new QWidget();
    QVBoxLayout *scrollLayout = new QVBoxLayout(scrollWidget);
    scrollLayout->setSpacing(5);
    scrollLayout->setContentsMargins(5, 5, 5, 5);

    QStringList eventCategories = CategoryCatalog::eventCategoryNames();
    for (const QString &categoryName : eventCategories)
    {
        QCheckBox *checkBox = new QCheckBox(categoryName, scrollWidget);
        checkBox->setChecked(true);
        connect(checkBox, &QCheckBox::toggled, this, &EventRecordDialog::onEventCheckBoxToggled);
        m_eventCheckBoxes.insert(categoryName, checkBox);
        scrollLayout->addWidget(checkBox);
    }

    scrollLayout->addStretch();
    scrollWidget->setLayout(scrollLayout);

    m_scrollArea->setWidget(scrollWidget);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setMinimumHeight(300);
    mainLayout->addWidget(m_scrollArea);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    m_okButton->setMinimumWidth(100);
    connect(m_okButton, &QPushButton::clicked, this, &EventRecordDialog::onOkClicked);
    buttonLayout->addWidget(m_okButton);

    m_cancelButton->setMinimumWidth(100);
    connect(m_cancelButton, &QPushButton::clicked, this, &EventRecordDialog::onCancelClicked);
    buttonLayout->addWidget(m_cancelButton);

    mainLayout->addLayout(buttonLayout);
}

void EventRecordDialog::loadMeterSettings()
{
    if (!m_meterModel)
    {
        m_infoLabel->setText("未选择电表");
        m_globalCheckBox->setEnabled(false);
        m_scrollArea->setEnabled(false);
        m_okButton->setEnabled(false);
        return;
    }

    QString meterAddress = m_meterModel->getMeterAddress();
    m_infoLabel->setText(QString("当前电表地址：%1").arg(meterAddress));

    bool allChecked = !m_eventCheckBoxes.isEmpty();
    for (auto it = m_eventCheckBoxes.begin(); it != m_eventCheckBoxes.end(); ++it)
    {
        const bool checked = m_meterModel->isEventCategoryEnabled(it.key());
        it.value()->setChecked(checked);
        if (!checked)
        {
            allChecked = false;
        }
    }
    m_globalCheckBox->setChecked(allChecked);
}

void EventRecordDialog::onGlobalCheckBoxToggled(bool checked)
{
    // 全局复选框切换时，直接设置所有子复选框的选中状态
    for (auto it = m_eventCheckBoxes.begin(); it != m_eventCheckBoxes.end(); ++it)
    {
        it.value()->setChecked(checked);
    }
}

void EventRecordDialog::onEventCheckBoxToggled(bool checked)
{
    Q_UNUSED(checked);

    bool allChecked = !m_eventCheckBoxes.isEmpty();
    for (auto it = m_eventCheckBoxes.cbegin(); it != m_eventCheckBoxes.cend(); ++it)
    {
        if (!it.value()->isChecked())
        {
            allChecked = false;
            break;
        }
    }

    if (m_globalCheckBox->isChecked() != allChecked)
    {
        QSignalBlocker blocker(m_globalCheckBox);
        m_globalCheckBox->setChecked(allChecked);
    }
}

void EventRecordDialog::onOkClicked()
{
    if (!m_meterModel)
    {
        QMessageBox::warning(this, "错误", "未选择电表");
        return;
    }

    bool hasCheckedItem = false;
    QMap<QString, bool> categoryEnabled;
    for (auto it = m_eventCheckBoxes.cbegin(); it != m_eventCheckBoxes.cend(); ++it)
    {
        const bool checked = it.value()->isChecked();
        categoryEnabled.insert(it.key(), checked);
        hasCheckedItem = hasCheckedItem || checked;
    }
    m_meterModel->applyEventRecordSettings(hasCheckedItem, categoryEnabled);

    accept();
}

void EventRecordDialog::onCancelClicked()
{
    reject();
}

void EventRecordDialog::applyDarkTheme()
{
    m_infoLabel->setStyleSheet("QLabel { padding: 8px; background-color: #2d2d2d; border-radius: 4px; color: #e0e0e0; }");

    m_scrollArea->widget()->setStyleSheet("background-color: #1e1e1e; color: #e0e0e0;");
    m_scrollArea->setStyleSheet("QScrollArea { background-color: #1e1e1e; border: 1px solid #3d3d3d; }");
}

void EventRecordDialog::applyLightTheme()
{
    m_infoLabel->setStyleSheet("QLabel { padding: 8px; background-color: #f0f0f0; border-radius: 4px; color: #333333; }");

    m_scrollArea->widget()->setStyleSheet("background-color: #ffffff; color: #333333;");
    m_scrollArea->setStyleSheet("QScrollArea { background-color: #ffffff; border: 1px solid #d0d0d0; }");
}
