#include "mainwindow.h"
#include "Controllers/ProtocolController.h"
#include "ViewModels/MainWindowViewModel.h"

#include "Services/MeterDataStore.h"
#include "Utils/Logger.h"
#include "Views/DataTable/DataTableWidget.h"
#include "Views/DeviceTree/DeviceTreeWidget.h"
#include "Views/Dialogs/CommunicationDialog.h"
#include "Views/Dialogs/BatchNewMeterDialog.h"
#include "Views/Dialogs/EventRecordDialog.h"
#include "Views/Dialogs/NewMeterDialog.h"
#include "Views/OperationLog/OperationLogWidget.h"

#include <QLabel>
#include <QMessageBox>
#include <QStatusBar>
#include <QVariant>
void MainWindow::onNewMeterDialog()
{
    // 确保当前选中了连接节点
    DeviceTreeWidget *deviceTree = deviceTreeWidget();
    if (!deviceTree || !deviceTree->canAddMeter())
    {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请先选择一个连接节点"));
        return;
    }

    QString connectionAlias = deviceTree->getSelectedConnectionAlias();
    if (connectionAlias.isEmpty())
    {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请先选择一个连接节点"));
        return;
    }

    NewMeterDialog dialog(this);

    // 设置当前电表配置
    MeterConfig currentConfig;
    dialog.setMeterConfig(currentConfig);

    if (dialog.exec() == QDialog::Accepted)
    {
        MeterConfig config = dialog.getMeterConfig();

        // 创建 MeterInfo 结构体
        MeterInfo meterInfo;
        meterInfo.alias = config.alias;
        meterInfo.address = config.address;
        meterInfo.operatorCode = config.operatorCode;
        meterInfo.password = config.password;
        meterInfo.includeLeadingFE = config.includePrefixFE;
        meterInfo.eventRecordPresent = config.eventRecordPresent;
        meterInfo.phaseType = config.phaseType;

        // 添加电表节点到设备树（第 2 层级）
        if (!deviceTree->addMeter(connectionAlias, meterInfo))
        {
            return;
        }

        refreshMeterPresence();
        applyCurrentMeter(meterInfo);

        // 更新状态栏显示
        QStatusBar *statusBar = this->statusBar();
        QList<QLabel *> labels = statusBar->findChildren<QLabel *>();
        if (labels.size() >= 2)
        {
            labels[1]->setText(QStringLiteral("地址: ") + config.address);
        }

        LOG_INFO(QString("Meter added: Alias=%1, Address=%2, OperatorCode=%3, IncludePrefixFE=%4")
                     .arg(config.alias)
                     .arg(config.address)
                     .arg(config.operatorCode)
                     .arg(config.includePrefixFE ? "true" : "false"));
    }
}

void MainWindow::onBatchNewMeterDialog()
{
    DeviceTreeWidget *deviceTree = deviceTreeWidget();
    if (!deviceTree || !deviceTree->canAddMeter())
    {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请先选择一个连接节点"));
        return;
    }

    const QString connectionAlias = deviceTree->getSelectedConnectionAlias();
    if (connectionAlias.isEmpty())
    {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请先选择一个连接节点"));
        return;
    }

    onBatchNewMeterRequested(connectionAlias);
}

void MainWindow::onBatchNewMeterRequested(const QString &connectionAlias)
{
    m_viewModel->selectConnection(connectionAlias);

    DeviceTreeWidget *deviceTree = deviceTreeWidget();
    if (!deviceTree)
    {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("未找到设备树"));
        return;
    }

    BatchNewMeterDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    const BatchMeterConfig config = dialog.getBatchMeterConfig();
    const QStringList existingAddresses = collectMeterAddresses();
    QStringList newAddresses;
    newAddresses.reserve(config.count);

    for (int i = 0; i < config.count; ++i)
    {
        bool ok = false;
        const quint64 startAddress = config.startAddress.toULongLong(&ok);
        const QString address = ok
                                    ? QString("%1").arg(startAddress + static_cast<quint64>(i), 12, 10, QLatin1Char('0'))
                                    : config.startAddress;

        if (existingAddresses.contains(address) || newAddresses.contains(address))
        {
            QMessageBox::warning(this, QStringLiteral("错误"),
                                 QStringLiteral("通信地址 %1 已存在，不能批量创建重复电表。").arg(address));
            return;
        }
        newAddresses.append(address);
    }

    MeterInfo lastMeterInfo;
    int addedCount = 0;
    for (int i = 0; i < newAddresses.size(); ++i)
    {
        MeterInfo meterInfo;
        meterInfo.alias = QStringLiteral("%1%2").arg(config.aliasPrefix).arg(config.aliasStartNumber + i);
        meterInfo.address = newAddresses.at(i);
        meterInfo.operatorCode = config.operatorCode;
        meterInfo.password = config.password;
        meterInfo.includeLeadingFE = config.includePrefixFE;
        meterInfo.eventRecordPresent = config.eventRecordPresent;
        meterInfo.phaseType = config.phaseType;

        if (!deviceTree->addMeter(connectionAlias, meterInfo))
        {
            break;
        }

        createMeterModel(connectionAlias, meterInfo);
        lastMeterInfo = meterInfo;
        ++addedCount;
    }

    if (addedCount <= 0)
    {
        return;
    }

    refreshMeterPresence();
    applyCurrentMeter(lastMeterInfo);

    QStatusBar *statusBar = this->statusBar();
    QList<QLabel *> labels = statusBar->findChildren<QLabel *>();
    if (labels.size() >= 2)
    {
        labels[1]->setText(QStringLiteral("地址: ") + lastMeterInfo.address);
    }

    LOG_INFO(QString("Batch meters added: Connection=%1, Count=%2, StartAddress=%3")
                 .arg(connectionAlias)
                 .arg(addedCount)
                 .arg(config.startAddress));
    QMessageBox::information(this, QStringLiteral("成功"),
                             QStringLiteral("已批量新建 %1 个电表").arg(addedCount));
}

QStringList MainWindow::collectMeterAddresses() const
{
    QStringList addresses;
    DeviceTreeWidget *deviceTree = deviceTreeWidget();
    if (!deviceTree)
    {
        return addresses;
    }

    const QList<MeterInfo> meters = deviceTree->allMeterInfos();
    for (const MeterInfo &meterInfo : meters)
    {
        if (!meterInfo.address.isEmpty())
        {
            addresses.append(meterInfo.address);
        }
    }

    addresses.removeDuplicates();
    return addresses;
}

QStringList MainWindow::collectMeterAddressesForConnection(const QString &connectionAlias) const
{
    QStringList addresses;
    DeviceTreeWidget *deviceTree = deviceTreeWidget();
    if (!deviceTree)
    {
        return addresses;
    }

    const QList<MeterInfo> meters = deviceTree->meterInfosForConnection(connectionAlias);
    for (const MeterInfo &meterInfo : meters)
    {
        if (!meterInfo.address.isEmpty())
        {
            addresses.append(meterInfo.address);
        }
    }

    addresses.removeDuplicates();
    return addresses;
}

MeterInfo MainWindow::findMeterInfo(const QString &connectionAlias, const QString &meterAlias) const
{
    MeterInfo meterInfo;
    DeviceTreeWidget *deviceTree = deviceTreeWidget();
    if (!deviceTree)
    {
        return meterInfo;
    }

    return deviceTree->meterInfo(connectionAlias, meterAlias);
}

void MainWindow::removeMeterModel(const QString &address)
{
    if (address.isEmpty())
    {
        return;
    }

    MeterModel *meterModel = m_meterModelsByAddress.take(address);
    for (ProtocolController *controller : m_protocolControllers)
    {
        if (controller)
        {
            controller->unregisterMeterModel(address);
        }
    }
    if (meterModel)
    {
        meterModel->deleteLater();
    }
}

void MainWindow::clearMeterModels(const QStringList &addresses)
{
    for (const QString &address : addresses)
    {
        removeMeterModel(address);
    }
}

void MainWindow::attachMeterPersistence(MeterModel *meterModel)
{
    if (!meterModel || !m_meterDataStore || !m_meterDataStore->isReady())
    {
        return;
    }

    connect(meterModel, &MeterModel::dataChanged, this, [this, meterModel](const QString &diCode, const QVariant &value)
            {
                if (m_meterDataStore->saveMeterData(meterModel))
                {
                    LOG_INFO(QString("保存当前数据: 电表地址=%1, DI=%2, 值=%3")
                                 .arg(meterModel->getMeterAddress(), diCode, value.toString()));
                } });
    connect(meterModel, &MeterModel::dataBatchChanged, this, [this, meterModel]()
            {
                if (m_meterDataStore->saveMeterData(meterModel))
                {
                    LOG_INFO(QString("当前电表数据已批量保存到数据库: 电表地址=%1")
                                 .arg(meterModel->getMeterAddress()));
                } });
    connect(meterModel, &MeterModel::addressChanged, this, [this, meterModel](const QString &newAddress)
            {
                if (m_meterDataStore->saveMeterData(meterModel))
                {
                    LOG_INFO(QString("电表地址更改后已保存到数据库: 电表地址=%1")
                                 .arg(newAddress));
                } });
}

MeterModel *MainWindow::createMeterModel(const QString &connectionAlias,
                                         const MeterInfo &meterInfo)
{
    if (meterInfo.address.isEmpty())
    {
        return nullptr;
    }

    MeterModel *meterModel = m_meterModelsByAddress.value(meterInfo.address, nullptr);
    bool loadedPersistedData = false;
    if (!meterModel)
    {
        meterModel = new MeterModel(this);
        meterModel->setMeterPhaseType(meterInfo.phaseType);
        meterModel->setMeterAddress(meterInfo.address);
        meterModel->setOperatorCode(meterInfo.operatorCode);
        meterModel->setPassword(meterInfo.password);
        meterModel->setIncludePrefixFE(meterInfo.includeLeadingFE);
        meterModel->setEventRecordPresent(meterInfo.eventRecordPresent);

        if (m_meterDataStore && m_meterDataStore->isReady())
        {
            loadedPersistedData = m_meterDataStore->loadMeterData(meterModel);
            if (!loadedPersistedData)
            {
                m_meterDataStore->saveMeterData(meterModel);
            }
        }

        attachMeterPersistence(meterModel);
        m_meterModelsByAddress.insert(meterInfo.address, meterModel);
    }

    meterModel->setMeterPhaseType(meterInfo.phaseType);
    meterModel->setMeterAddress(meterInfo.address);
    meterModel->setOperatorCode(meterInfo.operatorCode);
    meterModel->setPassword(meterInfo.password);
    meterModel->setIncludePrefixFE(meterInfo.includeLeadingFE);
    if (!loadedPersistedData)
    {
        meterModel->setEventRecordPresent(meterInfo.eventRecordPresent);
    }

    ProtocolController *controller = ensureProtocolController(connectionAlias);
    if (controller)
    {
        controller->registerMeterModel(meterInfo.address, meterModel);
    }
    return meterModel;
}

void MainWindow::applyCurrentMeter(const MeterInfo &meterInfo)
{
    const QString connectionAlias = m_viewModel->currentConnectionAlias();
    MeterModel *meterModel = createMeterModel(connectionAlias, meterInfo);
    m_currentMeterModel = meterModel;
    m_realTimePhaseIndex = 0;

    if (m_currentMeterModel)
    {
        ProtocolController *controller = protocolController(connectionAlias);
        if (controller)
        {
            controller->setMeterModel(m_currentMeterModel);
        }
    }

    if (m_dataTableWidget)
    {
        m_dataTableWidget->setMeterModel(m_currentMeterModel);
        m_dataTableWidget->setMeterInfo(meterInfo.alias, meterInfo.address, meterInfo.phaseType);
    }

    if (m_communicationDialog)
    {
        m_communicationDialog->setMeterModel(m_currentMeterModel);
    }
    updateQueryFrameDialogState();
    updateFrameParseDialogState();

    m_viewModel->selectMeter(meterInfo.address);
    updateConnectionStatus(m_viewModel->isConnected());
    updateRealTimeDataDisplay(false);
}

void MainWindow::clearCurrentMeter()
{
    m_currentMeterModel = nullptr;
    m_realTimePhaseIndex = 0;
    for (ProtocolController *controller : m_protocolControllers)
    {
        if (controller)
        {
            controller->setMeterModel(nullptr);
        }
    }

    if (m_dataTableWidget)
    {
        m_dataTableWidget->setMeterModel(nullptr);
        m_dataTableWidget->setMeterInfo(QString(), QString(), MeterPhaseType::ThreePhase);
    }

    if (m_communicationDialog)
    {
        m_communicationDialog->setMeterModel(nullptr);
    }
    updateQueryFrameDialogState();
    updateFrameParseDialogState();

    m_viewModel->clearMeter();
    updateConnectionStatus(m_viewModel->isConnected());
    updateRealTimeDataDisplay(false);
}

void MainWindow::refreshMeterPresence()
{
    const QStringList meterAddresses = collectMeterAddresses();
    const bool hasMeter = !meterAddresses.isEmpty();

    refreshProtocolMeters();
    updateCommunicationDialogState();
    m_viewModel->setKnownMeterPresence(hasMeter);
    if (!hasMeter)
    {
        clearCurrentMeter();
    }
    else if (!m_viewModel->currentMeterAddress().isEmpty() &&
             !meterAddresses.contains(m_viewModel->currentMeterAddress()))
    {
        clearCurrentMeter();
    }
}

void MainWindow::onMeterSelected(const QString &meterAlias)
{
    Q_UNUSED(meterAlias);
    // TODO: 处理电表选中事件
}

void MainWindow::onMeterInfoSelected(const MeterInfo &meterInfo)
{
    if (meterInfo.address.isEmpty())
    {
        clearCurrentMeter();
        return;
    }

    applyCurrentMeter(meterInfo);
}

void MainWindow::onDeleteAllMetersRequested(const QString &connectionAlias)
{
    DeviceTreeWidget *deviceTree = deviceTreeWidget();
    if (!deviceTree)
    {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("未找到设备树"));
        return;
    }

    const QList<MeterInfo> metersToDelete = deviceTree->meterInfosForConnection(connectionAlias);

    if (metersToDelete.isEmpty())
    {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("该连接下没有电表"));
        return;
    }

    const QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        QStringLiteral("确认删除"),
        QStringLiteral("确定要删除连接 \"%1\" 下的全部 %2 个电表吗？").arg(connectionAlias).arg(metersToDelete.size()),
        QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes)
    {
        return;
    }

    bool currentMeterDeleted = false;
    const QString currentMeterAddress = m_viewModel->currentMeterAddress();
    for (const MeterInfo &meterInfo : metersToDelete)
    {
        deviceTree->deleteMeter(connectionAlias, meterInfo.alias);

        if (!meterInfo.address.isEmpty())
        {
            currentMeterDeleted = currentMeterDeleted || (meterInfo.address == currentMeterAddress);
            removeMeterModel(meterInfo.address);
        }
    }

    if (currentMeterDeleted)
    {
        clearCurrentMeter();
    }
    refreshMeterPresence();

    LOG_INFO(QString("All meters deleted from connection: %1, count=%2")
                 .arg(connectionAlias)
                 .arg(metersToDelete.size()));
}

void MainWindow::onDeleteMeterRequested(const QString &connectionAlias, const QString &meterAlias)
{
    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        QStringLiteral("确认删除"),
        QStringLiteral("确定要删除电表 \"%1\" 吗？").arg(meterAlias),
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes)
    {
        const MeterInfo meterInfo = findMeterInfo(connectionAlias, meterAlias);

        DeviceTreeWidget *deviceTree = deviceTreeWidget();
        if (deviceTree)
        {
            deviceTree->deleteMeter(connectionAlias, meterAlias);
            refreshMeterPresence();
        }

        if (!meterInfo.address.isEmpty())
        {
            removeMeterModel(meterInfo.address);
        }

        LOG_INFO(QString("Meter deleted: %1").arg(meterAlias));
    }
}

void MainWindow::onNewMeterRequested(const QString &connectionAlias)
{
    m_viewModel->selectConnection(connectionAlias);

    // 检查连接是否已经打开
    if (!m_viewModel->isConnectionConnected(connectionAlias))
    {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请先打开连接"));
        return;
    }

    DeviceTreeWidget *deviceTree = deviceTreeWidget();
    if (!deviceTree)
    {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("未找到设备树"));
        return;
    }

    NewMeterDialog dialog(this);

    // 设置当前电表配置
    MeterConfig currentConfig;
    currentConfig.alias = "电表别名";
    currentConfig.address = "000000000000";
    currentConfig.operatorCode = "00000000";
    currentConfig.password = "00000000";
    currentConfig.includePrefixFE = false;
    currentConfig.phaseType = MeterPhaseType::ThreePhase;
    dialog.setMeterConfig(currentConfig);

    if (dialog.exec() == QDialog::Accepted)
    {
        MeterConfig config = dialog.getMeterConfig();

        // 检查通信地址是否已经存在
        if (deviceTree->isMeterAddressExists(config.address))
        {
            QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("通信地址 %1 已存在，请使用不同的地址").arg(config.address));
            return;
        }

        // 创建 MeterInfo 结构体
        MeterInfo meterInfo;
        meterInfo.alias = config.alias;
        meterInfo.address = config.address;
        meterInfo.operatorCode = config.operatorCode;
        meterInfo.password = config.password;
        meterInfo.includeLeadingFE = config.includePrefixFE;
        meterInfo.eventRecordPresent = config.eventRecordPresent;
        meterInfo.phaseType = config.phaseType;

        // 添加电表节点到设备树
        if (deviceTree)
        {
            if (!deviceTree->addMeter(connectionAlias, meterInfo))
            {
                return;
            }
        }

        refreshMeterPresence();
        applyCurrentMeter(meterInfo);

        QStatusBar *statusBar = this->statusBar();
        QList<QLabel *> labels = statusBar->findChildren<QLabel *>();
        if (labels.size() >= 2)
        {
            labels[1]->setText(QStringLiteral("地址: ") + config.address);
        }

        LOG_INFO(QString("Meter added: Alias=%1, Address=%2, OperatorCode=%3, IncludePrefixFE=%4")
                     .arg(config.alias)
                     .arg(config.address)
                     .arg(config.operatorCode)
                     .arg(config.includePrefixFE ? "true" : "false"));

        QMessageBox::information(this, QStringLiteral("成功"), QStringLiteral("电表已添加"));
    }
}

void MainWindow::onEventRecordSettingsRequested(const QString &connectionAlias, const QString &meterAlias)
{
    MeterInfo meterInfo = findMeterInfo(connectionAlias, meterAlias);
    if (meterInfo.address.isEmpty())
    {
        QMessageBox::warning(this, "错误", "未找到电表信息");
        return;
    }

    MeterModel *meterModel = m_meterModelsByAddress.value(meterInfo.address, nullptr);
    if (!meterModel)
    {
        meterModel = createMeterModel(connectionAlias, meterInfo);
    }

    if (!meterModel)
    {
        QMessageBox::warning(this, "错误", "无法创建电表模型");
        return;
    }

    EventRecordDialog dialog(meterModel, this);

    if (m_viewModel->isDarkTheme())
    {
        dialog.applyDarkTheme();
    }
    else
    {
        dialog.applyLightTheme();
    }

    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    DeviceTreeWidget *deviceTree = deviceTreeWidget();
    if (deviceTree)
    {
        deviceTree->updateMeterEventRecordPresent(connectionAlias, meterAlias, meterModel->getEventRecordPresent());
    }

    if (m_currentMeterModel == meterModel && m_dataTableWidget)
    {
        m_dataTableWidget->refreshTable();
    }
}
