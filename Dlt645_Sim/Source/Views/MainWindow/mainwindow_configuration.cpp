#include "mainwindow.h"
#include "Models/ConnectionInfoMapper.h"
#include "ViewModels/MainWindowViewModel.h"

#include "Services/ConfigurationSerializer.h"
#include "Views/DeviceTree/DeviceTreeWidget.h"

#include <QFileDialog>
#include <QMessageBox>
void MainWindow::onExportConfiguration()
{
    DeviceTreeWidget *deviceTree = deviceTreeWidget();
    if (!deviceTree)
    {
        QMessageBox::warning(this, QStringLiteral("Notice"), QStringLiteral("Device tree is not available"));
        return;
    }

    const QString fileName = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("Export configuration"),
        QStringLiteral("dlt645_config.json"),
        QStringLiteral("JSON config file (*.json)"));
    if (fileName.isEmpty())
    {
        return;
    }

    ConfigurationSnapshot snapshot;
    const QStringList connectionAliases = deviceTree->connectionAliases();
    for (const QString &alias : connectionAliases)
    {
        if (!m_viewModel->hasConnectionConfig(alias))
        {
            continue;
        }

        ConnectionConfigSnapshot connectionSnapshot;
        connectionSnapshot.config = m_viewModel->connectionConfig(alias);

        const QList<MeterInfo> meters = deviceTree->meterInfosForConnection(alias);
        for (const MeterInfo &meterInfo : meters)
        {
            MeterConfigSnapshot meterSnapshot;
            meterSnapshot.alias = meterInfo.alias;
            meterSnapshot.address = meterInfo.address;
            meterSnapshot.operatorCode = meterInfo.operatorCode;
            meterSnapshot.password = meterInfo.password;
            meterSnapshot.includeLeadingFE = meterInfo.includeLeadingFE;
            meterSnapshot.eventRecordPresent = meterInfo.eventRecordPresent;
            meterSnapshot.phaseType = meterInfo.phaseType;
            connectionSnapshot.meters.append(meterSnapshot);
        }

        snapshot.connections.append(connectionSnapshot);
    }

    QString errorMessage;
    if (!ConfigurationSerializer::writeToFile(fileName, snapshot, &errorMessage))
    {
        QMessageBox::warning(this, QStringLiteral("Error"), errorMessage);
        return;
    }

    QMessageBox::information(this, QStringLiteral("Success"), QStringLiteral("Configuration exported"));
}

void MainWindow::onImportConfiguration()
{
    const QString fileName = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("Import configuration"),
        QString(),
        QStringLiteral("JSON config file (*.json)"));
    if (fileName.isEmpty())
    {
        return;
    }

    ConfigurationSnapshot snapshot;
    QStringList validationErrors;
    QString errorMessage;
    if (!ConfigurationSerializer::readFromFile(fileName, &snapshot, &validationErrors, &errorMessage))
    {
        if (!validationErrors.isEmpty())
        {
            QMessageBox::warning(this,
                                 QStringLiteral("Import failed"),
                                 QStringLiteral("Configuration validation failed. Current configuration was not changed:\n%1")
                                     .arg(validationErrors.join(QLatin1Char('\n'))));
            return;
        }

        QMessageBox::warning(this, QStringLiteral("Error"), errorMessage);
        return;
    }

    if (snapshot.connections.isEmpty())
    {
        QMessageBox::warning(this, QStringLiteral("Import failed"), QStringLiteral("No importable connection configs"));
        return;
    }

    const QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        QStringLiteral("Confirm import"),
        QStringLiteral("Importing will replace all current connections and meters. Continue?"),
        QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes)
    {
        return;
    }

    if (m_viewModel->isCloseConnectionVisible())
    {
        closeConnectionInternal(m_viewModel->currentConnectionAlias());
    }

    DeviceTreeWidget *deviceTree = deviceTreeWidget();
    if (!deviceTree)
    {
        QMessageBox::warning(this, QStringLiteral("Notice"), QStringLiteral("Device tree is not available"));
        return;
    }

    const QStringList existingConnectionAliases = deviceTree->connectionAliases();

    for (const QString &alias : existingConnectionAliases)
    {
        m_viewModel->removeConnectionConfig(alias);
    }

    clearMeterModels(collectMeterAddresses());

    deviceTree->clearTree();
    clearCurrentMeter();

    MeterInfo lastMeterInfo;
    for (const ConnectionConfigSnapshot &importedConnection : snapshot.connections)
    {
        m_viewModel->upsertConnectionConfig(importedConnection.config);
        deviceTree->addConnection(ConnectionInfoMapper::fromConfig(importedConnection.config, false));

        for (const MeterConfigSnapshot &meterSnapshot : importedConnection.meters)
        {
            MeterInfo meterInfo;
            meterInfo.alias = meterSnapshot.alias;
            meterInfo.address = meterSnapshot.address;
            meterInfo.operatorCode = meterSnapshot.operatorCode;
            meterInfo.password = meterSnapshot.password;
            meterInfo.includeLeadingFE = meterSnapshot.includeLeadingFE;
            meterInfo.eventRecordPresent = meterSnapshot.eventRecordPresent;
            meterInfo.phaseType = meterSnapshot.phaseType;

            if (deviceTree->addMeter(importedConnection.config.alias, meterInfo))
            {
                createMeterModel(meterInfo);
                lastMeterInfo = meterInfo;
            }
        }
    }

    refreshMeterPresence();
    if (!lastMeterInfo.address.isEmpty())
    {
        applyCurrentMeter(lastMeterInfo);
    }
    updateToolBarButtons();
    QMessageBox::information(this, QStringLiteral("Success"), QStringLiteral("Configuration imported"));
}
