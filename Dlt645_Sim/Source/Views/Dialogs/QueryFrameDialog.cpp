#include "QueryFrameDialog.h"

#include "FrameParseDialog.h"
#include "Models/DataItem.h"
#include "Models/MeterModel.h"
#include "Services/ProtocolParser.h"
#include "Utils/DLT645FrameFormatter.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QFont>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QSignalBlocker>
#include <QVBoxLayout>

QueryFrameDialog::QueryFrameDialog(QWidget *parent)
    : QDialog(parent),
      m_meterModel(nullptr),
      m_catalogMeterModel(new MeterModel(this)),
      m_frameParseDialog(nullptr),
      m_isDarkTheme(true),
      m_addressEdit(nullptr),
      m_includePrefixCheckBox(nullptr),
      m_modeComboBox(nullptr),
      m_categoryComboBox(nullptr),
      m_searchEdit(nullptr),
      m_dataItemList(nullptr),
      m_selectedDiLabel(nullptr),
      m_writeValueLabel(nullptr),
      m_writeValueEdit(nullptr),
      m_writeHintLabel(nullptr),
      m_spacedHexEdit(nullptr),
      m_compactHexEdit(nullptr),
      m_copySpacedButton(nullptr),
      m_copyCompactButton(nullptr),
      m_parseFrameButton(nullptr),
      m_statusLabel(nullptr)
{
    setupUI();
    populateCategories();
    populateDataItems();
    applyDarkTheme();

    setWindowTitle(QString::fromUtf8("DLT645 帧生成"));
    resize(860, 680);
}

QueryFrameDialog::~QueryFrameDialog()
{
}

void QueryFrameDialog::setMeterModel(MeterModel *meterModel)
{
    m_meterModel = meterModel;
    if (m_frameParseDialog)
    {
        m_frameParseDialog->setMeterModel(m_meterModel);
    }

    populateCategories();
    populateDataItems();
}

void QueryFrameDialog::setDefaultMeterAddress(const QString &address)
{
    if (m_addressEdit && m_addressEdit->text() != address)
    {
        m_addressEdit->setText(address);
    }
}

void QueryFrameDialog::setIncludePrefixFE(bool include)
{
    if (m_includePrefixCheckBox && m_includePrefixCheckBox->isChecked() != include)
    {
        m_includePrefixCheckBox->setChecked(include);
    }
}

void QueryFrameDialog::applyDarkTheme()
{
    m_isDarkTheme = true;
    setStyleSheet(R"(
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

        QLineEdit, QListWidget, QComboBox {
            background-color: #3C3F41;
            color: #FFFFFF;
            border: 1px solid #555555;
            border-radius: 3px;
            padding: 5px;
        }

        QLineEdit:focus, QListWidget:focus, QComboBox:focus {
            border: 1px solid #094771;
        }

        QListWidget::item {
            padding: 6px 4px;
            border-bottom: 1px solid #3C3F41;
        }

        QListWidget::item:selected {
            background-color: #094771;
            color: #FFFFFF;
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

        QPushButton:disabled {
            color: #888888;
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
    )");

    if (m_frameParseDialog)
    {
        m_frameParseDialog->applyDarkTheme();
    }
}

void QueryFrameDialog::applyLightTheme()
{
    m_isDarkTheme = false;
    setStyleSheet(R"(
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

        QLineEdit, QListWidget, QComboBox {
            background-color: #FFFFFF;
            color: #000000;
            border: 1px solid #CCCCCC;
            border-radius: 3px;
            padding: 5px;
        }

        QLineEdit:focus, QListWidget:focus, QComboBox:focus {
            border: 1px solid #0078D7;
        }

        QListWidget::item {
            padding: 6px 4px;
            border-bottom: 1px solid #E0E0E0;
        }

        QListWidget::item:selected {
            background-color: #0078D7;
            color: #FFFFFF;
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

        QPushButton:disabled {
            color: #999999;
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
    )");

    if (m_frameParseDialog)
    {
        m_frameParseDialog->applyLightTheme();
    }
}

void QueryFrameDialog::onModeChanged(int index)
{
    Q_UNUSED(index);
    populateCategories();
    populateDataItems();
    refreshWriteValueState();
}

void QueryFrameDialog::onCategoryChanged(int index)
{
    Q_UNUSED(index);
    populateDataItems();
}

void QueryFrameDialog::onSearchTextChanged(const QString &text)
{
    Q_UNUSED(text);
    populateDataItems();
}

void QueryFrameDialog::onCurrentItemChanged(QListWidgetItem *current, QListWidgetItem *previous)
{
    Q_UNUSED(previous);

    if (!current)
    {
        m_selectedDiLabel->setText(QString::fromUtf8("当前未选择数据标识"));
    }
    else
    {
        const QString matchedCategory = current->data(Qt::UserRole + 2).toString();
        if (!matchedCategory.isEmpty())
        {
            const QSignalBlocker blocker(m_categoryComboBox);
            const int categoryIndex = m_categoryComboBox->findText(matchedCategory);
            if (categoryIndex >= 0)
            {
                m_categoryComboBox->setCurrentIndex(categoryIndex);
            }
        }

        m_selectedDiLabel->setText(QString::fromUtf8("当前选择：%1").arg(current->text()));
    }

    refreshWriteValueState();
    refreshGeneratedFrame();
}

void QueryFrameDialog::onInputChanged()
{
    refreshGeneratedFrame();
}

void QueryFrameDialog::onCopySpacedClicked()
{
    if (!m_spacedHexEdit->text().isEmpty())
    {
        QApplication::clipboard()->setText(m_spacedHexEdit->text());
    }
}

void QueryFrameDialog::onCopyCompactClicked()
{
    if (!m_compactHexEdit->text().isEmpty())
    {
        QApplication::clipboard()->setText(m_compactHexEdit->text());
    }
}

void QueryFrameDialog::onParseFrameClicked()
{
    if (!m_spacedHexEdit->text().isEmpty())
    {
        showFrameParseDialog(m_spacedHexEdit->text());
    }
}

void QueryFrameDialog::closeEvent(QCloseEvent *event)
{
    if (m_frameParseDialog)
    {
        m_frameParseDialog->hide();
    }

    emit dialogClosed();
    QDialog::closeEvent(event);
}

void QueryFrameDialog::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(10);

    QGroupBox *configGroup = new QGroupBox(QString::fromUtf8("帧配置"), this);
    QVBoxLayout *configLayout = new QVBoxLayout(configGroup);

    QFormLayout *formLayout = new QFormLayout();
    formLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_addressEdit = new QLineEdit(configGroup);
    m_addressEdit->setPlaceholderText(QString::fromUtf8("请输入 12 位电表号"));
    m_addressEdit->setClearButtonEnabled(true);
    m_addressEdit->setValidator(new QRegularExpressionValidator(QRegularExpression(QStringLiteral("\\d{0,12}")), m_addressEdit));
    formLayout->addRow(QString::fromUtf8("电表号:"), m_addressEdit);

    m_includePrefixCheckBox = new QCheckBox(QString::fromUtf8("包含前导 FE"), configGroup);
    formLayout->addRow(QString::fromUtf8("帧选项:"), m_includePrefixCheckBox);

    m_modeComboBox = new QComboBox(configGroup);
    m_modeComboBox->addItem(QString::fromUtf8("读数据帧"));
    m_modeComboBox->addItem(QString::fromUtf8("写数据帧"));
    formLayout->addRow(QString::fromUtf8("操作类型:"), m_modeComboBox);

    m_searchEdit = new QLineEdit(configGroup);
    m_searchEdit->setPlaceholderText(QString::fromUtf8("全局搜索 DI 或描述，类型将自动匹配"));
    m_searchEdit->setClearButtonEnabled(true);
    formLayout->addRow(QString::fromUtf8("筛选:"), m_searchEdit);

    m_categoryComboBox = new QComboBox(configGroup);
    formLayout->addRow(QString::fromUtf8("数据类型:"), m_categoryComboBox);

    configLayout->addLayout(formLayout);
    mainLayout->addWidget(configGroup);

    QGroupBox *dataItemGroup = new QGroupBox(QString::fromUtf8("数据标识"), this);
    QVBoxLayout *dataItemLayout = new QVBoxLayout(dataItemGroup);

    m_dataItemList = new QListWidget(dataItemGroup);
    m_dataItemList->setSelectionMode(QAbstractItemView::SingleSelection);
    dataItemLayout->addWidget(m_dataItemList);

    m_selectedDiLabel = new QLabel(QString::fromUtf8("当前未选择数据标识"), dataItemGroup);
    m_selectedDiLabel->setWordWrap(true);
    dataItemLayout->addWidget(m_selectedDiLabel);

    mainLayout->addWidget(dataItemGroup, 1);

    QGroupBox *writeGroup = new QGroupBox(QString::fromUtf8("写入数据"), this);
    QVBoxLayout *writeLayout = new QVBoxLayout(writeGroup);

    QHBoxLayout *writeValueLayout = new QHBoxLayout();
    m_writeValueLabel = new QLabel(QString::fromUtf8("写入值:"), writeGroup);
    m_writeValueEdit = new QLineEdit(writeGroup);
    m_writeValueEdit->setClearButtonEnabled(true);
    m_writeValueEdit->setPlaceholderText(QString::fromUtf8("输入写入值，非数字字符会自动忽略"));
    writeValueLayout->addWidget(m_writeValueLabel);
    writeValueLayout->addWidget(m_writeValueEdit, 1);
    writeLayout->addLayout(writeValueLayout);

    m_writeHintLabel = new QLabel(writeGroup);
    m_writeHintLabel->setWordWrap(true);
    writeLayout->addWidget(m_writeHintLabel);

    mainLayout->addWidget(writeGroup);

    QGroupBox *resultGroup = new QGroupBox(QString::fromUtf8("帧生成结果"), this);
    QVBoxLayout *resultLayout = new QVBoxLayout(resultGroup);

    QHBoxLayout *spacedLayout = new QHBoxLayout();
    QLabel *spacedLabel = new QLabel(QString::fromUtf8("带空格 HEX:"), resultGroup);
    m_spacedHexEdit = new QLineEdit(resultGroup);
    m_spacedHexEdit->setReadOnly(true);
    m_spacedHexEdit->setFont(QFont(QStringLiteral("Consolas"), 10));
    m_copySpacedButton = new QPushButton(QString::fromUtf8("复制"), resultGroup);
    spacedLayout->addWidget(spacedLabel);
    spacedLayout->addWidget(m_spacedHexEdit, 1);
    spacedLayout->addWidget(m_copySpacedButton);
    resultLayout->addLayout(spacedLayout);

    QHBoxLayout *compactLayout = new QHBoxLayout();
    QLabel *compactLabel = new QLabel(QString::fromUtf8("紧凑 HEX:"), resultGroup);
    m_compactHexEdit = new QLineEdit(resultGroup);
    m_compactHexEdit->setReadOnly(true);
    m_compactHexEdit->setFont(QFont(QStringLiteral("Consolas"), 10));
    m_copyCompactButton = new QPushButton(QString::fromUtf8("复制"), resultGroup);
    compactLayout->addWidget(compactLabel);
    compactLayout->addWidget(m_compactHexEdit, 1);
    compactLayout->addWidget(m_copyCompactButton);
    resultLayout->addLayout(compactLayout);

    QHBoxLayout *actionLayout = new QHBoxLayout();
    actionLayout->addStretch();
    m_parseFrameButton = new QPushButton(QString::fromUtf8("解析帧"), resultGroup);
    actionLayout->addWidget(m_parseFrameButton);
    resultLayout->addLayout(actionLayout);

    m_statusLabel = new QLabel(resultGroup);
    m_statusLabel->setWordWrap(true);
    resultLayout->addWidget(m_statusLabel);

    mainLayout->addWidget(resultGroup);

    connect(m_modeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &QueryFrameDialog::onModeChanged);
    connect(m_categoryComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &QueryFrameDialog::onCategoryChanged);
    connect(m_searchEdit, &QLineEdit::textChanged,
            this, &QueryFrameDialog::onSearchTextChanged);
    connect(m_dataItemList, &QListWidget::currentItemChanged,
            this, &QueryFrameDialog::onCurrentItemChanged);
    connect(m_addressEdit, &QLineEdit::textChanged,
            this, &QueryFrameDialog::onInputChanged);
    connect(m_includePrefixCheckBox, &QCheckBox::toggled,
            this, &QueryFrameDialog::onInputChanged);
    connect(m_writeValueEdit, &QLineEdit::textChanged,
            this, &QueryFrameDialog::onInputChanged);
    connect(m_copySpacedButton, &QPushButton::clicked,
            this, &QueryFrameDialog::onCopySpacedClicked);
    connect(m_copyCompactButton, &QPushButton::clicked,
            this, &QueryFrameDialog::onCopyCompactClicked);
    connect(m_parseFrameButton, &QPushButton::clicked,
            this, &QueryFrameDialog::onParseFrameClicked);

    refreshWriteValueState();
    updateActionButtons();
}

void QueryFrameDialog::populateCategories()
{
    const QSignalBlocker blocker(m_categoryComboBox);
    const QString previousCategory = m_categoryComboBox->currentText();

    m_categoryComboBox->clear();
    for (const QString &category : allCategories())
    {
        const QList<QString> diCodes = dataCodesForCategory(category);
        bool hasSupportedCode = false;
        for (const QString &diCode : diCodes)
        {
            if (supportsCurrentMode(dataSourceModel()->getDataItem(diCode)))
            {
                hasSupportedCode = true;
                break;
            }
        }

        if (hasSupportedCode)
        {
            m_categoryComboBox->addItem(category);
        }
    }

    const int previousIndex = m_categoryComboBox->findText(previousCategory);
    if (previousIndex >= 0)
    {
        m_categoryComboBox->setCurrentIndex(previousIndex);
    }
    else if (m_categoryComboBox->count() > 0)
    {
        m_categoryComboBox->setCurrentIndex(0);
    }
}

void QueryFrameDialog::populateDataItems()
{
    const QSignalBlocker blocker(m_dataItemList);
    m_dataItemList->clear();

    const MeterModel *sourceModel = dataSourceModel();
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
            const DataItem item = sourceModel->getDataItem(diCode);
            if (!supportsCurrentMode(item))
            {
                continue;
            }

            const QString description = item.description.isEmpty() ? QString::fromUtf8("未命名数据项") : item.description;
            const QString matchText = QStringLiteral("%1 %2").arg(diCode, description);
            if (hasFilter && !matchText.contains(filterText, Qt::CaseInsensitive))
            {
                continue;
            }

            const QString itemText = hasFilter
                                         ? QStringLiteral("[%1] %2  |  %3").arg(category, diCode, description)
                                         : QStringLiteral("%1  |  %2").arg(diCode, description);
            QListWidgetItem *listItem = new QListWidgetItem(itemText);
            listItem->setData(Qt::UserRole, diCode);
            listItem->setData(Qt::UserRole + 1, description);
            listItem->setData(Qt::UserRole + 2, category);
            m_dataItemList->addItem(listItem);
        }
    }

    m_categoryComboBox->setEnabled(!hasFilter);

    if (m_dataItemList->count() > 0)
    {
        m_dataItemList->setCurrentRow(0);
        if (QListWidgetItem *currentItem = m_dataItemList->currentItem())
        {
            m_selectedDiLabel->setText(QString::fromUtf8("当前选择：%1").arg(currentItem->text()));
        }
    }
    else
    {
        m_selectedDiLabel->setText(QString::fromUtf8("当前未选择数据标识"));
        if (hasFilter)
        {
            m_statusLabel->setText(QString::fromUtf8("未找到匹配的数据标识。"));
        }
        else if (isWriteMode())
        {
            m_statusLabel->setText(QString::fromUtf8("当前分类下没有可写数据标识。"));
        }
        else
        {
            m_statusLabel->setText(QString::fromUtf8("当前分类下没有可读数据标识。"));
        }
    }

    refreshWriteValueState();
    refreshGeneratedFrame();
}

void QueryFrameDialog::refreshGeneratedFrame()
{
    const QString address = m_addressEdit->text().trimmed();
    const QString diCode = selectedDiCode();
    const bool includePrefixFE = m_includePrefixCheckBox->isChecked();

    m_spacedHexEdit->clear();
    m_compactHexEdit->clear();

    if (address.isEmpty())
    {
        m_statusLabel->setText(QString::fromUtf8("请输入 12 位电表号后生成帧。"));
        updateActionButtons();
        return;
    }

    if (!ProtocolParser::isValidAddress(address))
    {
        m_statusLabel->setText(QString::fromUtf8("电表号必须是 12 位数字。"));
        updateActionButtons();
        return;
    }

    if (diCode.isEmpty())
    {
        m_statusLabel->setText(QString::fromUtf8("请选择一个数据标识。"));
        updateActionButtons();
        return;
    }

    ProtocolParser parser(nullptr);
    ProtocolFrame frame;

    if (isWriteMode())
    {
        const QString writeValue = m_writeValueEdit->text().trimmed();
        if (writeValue.isEmpty())
        {
            m_statusLabel->setText(QString::fromUtf8("请输入写入值后生成写数据帧。"));
            updateActionButtons();
            return;
        }

        frame = parser.buildWriteRequest(ProtocolParser::addressToBytes(address), diCode, QVariant(writeValue), includePrefixFE);
    }
    else
    {
        frame = parser.buildReadRequest(ProtocolParser::addressToBytes(address), diCode, includePrefixFE);
    }

    const QByteArray rawFrame = ProtocolParser::frameToRawBytes(frame, includePrefixFE);
    m_spacedHexEdit->setText(DLT645FrameFormatter::formatHex(rawFrame, true));
    m_compactHexEdit->setText(DLT645FrameFormatter::formatHex(rawFrame, false));

    if (isWriteMode())
    {
        m_statusLabel->setText(QString::fromUtf8("已生成写数据帧：%1").arg(selectedDiDescription()));
    }
    else
    {
        m_statusLabel->setText(QString::fromUtf8("已生成读数据帧：%1").arg(selectedDiDescription()));
    }

    updateActionButtons();
}

void QueryFrameDialog::refreshWriteValueState()
{
    const bool writeMode = isWriteMode();
    const QString currentDiCode = selectedDiCode();

    m_writeValueLabel->setVisible(writeMode);
    m_writeValueEdit->setVisible(writeMode);
    m_writeHintLabel->setVisible(writeMode);
    m_writeValueEdit->setEnabled(writeMode && !currentDiCode.isEmpty());

    if (!writeMode)
    {
        m_writeHintLabel->clear();
        m_writeValueEdit->setProperty("diCode", QString());
        return;
    }

    if (currentDiCode.isEmpty())
    {
        m_writeHintLabel->setText(QString::fromUtf8("选择可写 DI 后，会自动带出当前值和格式提示。"));
        m_writeValueEdit->clear();
        m_writeValueEdit->setProperty("diCode", QString());
        return;
    }

    const QString defaultValue = selectedDiDefaultValue();
    if (m_writeValueEdit->property("diCode").toString() != currentDiCode)
    {
        m_writeValueEdit->setText(defaultValue);
        m_writeValueEdit->setProperty("diCode", currentDiCode);
    }

    const DataItem item = dataSourceModel()->getDataItem(currentDiCode);
    const QString unitText = item.unit.isEmpty() ? QString::fromUtf8("-") : item.unit;
    const QString formatText = item.format.isEmpty() ? QString::fromUtf8("-") : item.format;
    m_writeHintLabel->setText(QString::fromUtf8("当前值：%1    单位：%2    格式：%3")
                                  .arg(defaultValue.isEmpty() ? QString::fromUtf8("-") : defaultValue,
                                       unitText,
                                       formatText));
}

void QueryFrameDialog::updateActionButtons()
{
    const bool hasFrame = !m_spacedHexEdit->text().isEmpty();
    m_copySpacedButton->setEnabled(hasFrame);
    m_copyCompactButton->setEnabled(!m_compactHexEdit->text().isEmpty());
    m_parseFrameButton->setEnabled(hasFrame);
}

QList<QString> QueryFrameDialog::allCategories() const
{
    QList<QString> categories = {
        QString::fromUtf8("电能量"),
        QString::fromUtf8("最大需量及发生时间"),
        QString::fromUtf8("变量"),
        QString::fromUtf8("事件记录"),
        QString::fromUtf8("参变量"),
        QString::fromUtf8("定时冻结"),
        QString::fromUtf8("瞬时冻结"),
        QString::fromUtf8("时区切换冻结"),
        QString::fromUtf8("日时段切换冻结"),
        QString::fromUtf8("整点冻结"),
        QString::fromUtf8("分时费率切换冻结"),
        QString::fromUtf8("日冻结"),
        QString::fromUtf8("阶梯切换冻结"),
        QString::fromUtf8("负荷记录")};

    if (isWriteMode())
    {
        categories.append(QString::fromUtf8("控制命令"));
    }

    return categories;
}

QList<QString> QueryFrameDialog::dataCodesForCategory(const QString &category) const
{
    const MeterModel *sourceModel = dataSourceModel();
    if (!sourceModel)
    {
        return {};
    }

    if (category == QString::fromUtf8("电能量"))
    {
        return sourceModel->getEnergyDataCodes();
    }
    if (category == QString::fromUtf8("最大需量及发生时间"))
    {
        QList<QString> codes = sourceModel->getDemandDataCodes();
        const QList<QString> maxDemandCodes = sourceModel->getMaxDemandDataCodes();
        for (const QString &diCode : maxDemandCodes)
        {
            if (!codes.contains(diCode))
            {
                codes.append(diCode);
            }
        }
        return codes;
    }
    if (category == QString::fromUtf8("变量"))
    {
        return sourceModel->getVariableDataCodes();
    }
    if (category == QString::fromUtf8("事件记录"))
    {
        return sourceModel->getEventDataCodes();
    }
    if (category == QString::fromUtf8("参变量"))
    {
        return sourceModel->getParameterDataCodes();
    }
    if (category == QString::fromUtf8("定时冻结") ||
        category == QString::fromUtf8("瞬时冻结") ||
        category == QString::fromUtf8("时区切换冻结") ||
        category == QString::fromUtf8("日时段切换冻结") ||
        category == QString::fromUtf8("整点冻结") ||
        category == QString::fromUtf8("分时费率切换冻结") ||
        category == QString::fromUtf8("日冻结") ||
        category == QString::fromUtf8("阶梯切换冻结"))
    {
        return sourceModel->getFreezeDataCodes(category);
    }
    if (category == QString::fromUtf8("负荷记录"))
    {
        return sourceModel->getLoadRecordDataCodes();
    }
    if (category == QString::fromUtf8("控制命令"))
    {
        QList<QString> codes;
        const QList<DataItem> allItems = sourceModel->getAllDataItems();
        for (const DataItem &item : allItems)
        {
            if (item.diCode.startsWith(QStringLiteral("0x06")))
            {
                codes.append(item.diCode);
            }
        }
        return codes;
    }

    return {};
}

const MeterModel *QueryFrameDialog::dataSourceModel() const
{
    return m_meterModel ? m_meterModel : m_catalogMeterModel;
}

QueryFrameDialog::FrameOperationMode QueryFrameDialog::currentOperationMode() const
{
    return (m_modeComboBox && m_modeComboBox->currentIndex() == 1)
               ? FrameOperationMode::WriteData
               : FrameOperationMode::ReadData;
}

bool QueryFrameDialog::isWriteMode() const
{
    return currentOperationMode() == FrameOperationMode::WriteData;
}

bool QueryFrameDialog::supportsCurrentMode(const DataItem &item) const
{
    return isWriteMode() ? item.isWritable : true;
}

QString QueryFrameDialog::selectedDiCode() const
{
    QListWidgetItem *item = m_dataItemList->currentItem();
    return item ? item->data(Qt::UserRole).toString() : QString();
}

QString QueryFrameDialog::selectedDiDescription() const
{
    QListWidgetItem *item = m_dataItemList->currentItem();
    return item ? item->data(Qt::UserRole + 1).toString() : QString();
}

QString QueryFrameDialog::selectedDiDefaultValue() const
{
    const QString diCode = selectedDiCode();
    if (diCode.isEmpty())
    {
        return QString();
    }

    const DataItem item = dataSourceModel()->getDataItem(diCode);
    const QString formattedValue = item.getFormattedValue().trimmed();
    if (!formattedValue.isEmpty())
    {
        return formattedValue;
    }

    return item.value.toString().trimmed();
}

void QueryFrameDialog::showFrameParseDialog(const QString &frameHex)
{
    if (!m_frameParseDialog)
    {
        m_frameParseDialog = new FrameParseDialog(this);
        m_frameParseDialog->setMeterModel(m_meterModel);
    }

    if (m_isDarkTheme)
    {
        m_frameParseDialog->applyDarkTheme();
    }
    else
    {
        m_frameParseDialog->applyLightTheme();
    }

    m_frameParseDialog->setMeterModel(m_meterModel);
    m_frameParseDialog->setFrameHex(frameHex);
    m_frameParseDialog->show();
    m_frameParseDialog->raise();
    m_frameParseDialog->activateWindow();
}
