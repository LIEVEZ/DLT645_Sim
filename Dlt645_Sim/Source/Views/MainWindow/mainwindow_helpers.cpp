#include "mainwindow.h"

#include "Controllers/ConnectionManager.h"
#include "Controllers/ProtocolController.h"
#include "Views/DeviceTree/DeviceTreeWidget.h"

DeviceTreeWidget *MainWindow::deviceTreeWidget() const
{
    return findChild<DeviceTreeWidget *>();
}

ProtocolController *MainWindow::ensureProtocolController(const QString &connectionAlias)
{
    if (connectionAlias.isEmpty())
    {
        return nullptr;
    }

    ProtocolController *controller = m_protocolControllers.value(connectionAlias, nullptr);
    if (controller)
    {
        return controller;
    }

    controller = new ProtocolController(this);
    controller->setMeterModel(nullptr);
    m_protocolControllers.insert(connectionAlias, controller);

    connect(controller, &ProtocolController::frameReceived,
            this, [this, connectionAlias](const ProtocolFrame &frame, const QByteArray &rawFrame)
            {
                onProtocolFrameReceived(connectionAlias, frame, rawFrame);
            });
    connect(controller, &ProtocolController::frameSent,
            this, [this, connectionAlias](const ProtocolFrame &frame, const QByteArray &rawFrame)
            {
                onProtocolFrameSent(connectionAlias, frame, rawFrame);
            });
    connect(controller, &ProtocolController::rawDataReceived,
            this, [this, connectionAlias](const QByteArray &data)
            {
                onRawDataReceived(connectionAlias, data);
            });
    connect(controller, &ProtocolController::rawDataSent,
            this, [this, connectionAlias](const QByteArray &data)
            {
                onRawDataSent(connectionAlias, data);
            });

    return controller;
}

ProtocolController *MainWindow::protocolController(const QString &connectionAlias) const
{
    return m_protocolControllers.value(connectionAlias, nullptr);
}

void MainWindow::attachProtocolTransport(const QString &connectionAlias)
{
    ProtocolController *controller = ensureProtocolController(connectionAlias);
    if (!controller)
    {
        return;
    }

    controller->setSerialController(m_connectionManager->serialController(connectionAlias));
    controller->setTcpController(m_connectionManager->tcpController(connectionAlias));
}

void MainWindow::removeProtocolController(const QString &connectionAlias)
{
    ProtocolController *controller = m_protocolControllers.take(connectionAlias);
    if (!controller)
    {
        return;
    }

    controller->setSerialController(nullptr);
    controller->setTcpController(nullptr);
    controller->deleteLater();
}

void MainWindow::refreshProtocolMeters()
{
    DeviceTreeWidget *deviceTree = deviceTreeWidget();
    if (!deviceTree)
    {
        return;
    }

    for (auto it = m_protocolControllers.cbegin(); it != m_protocolControllers.cend(); ++it)
    {
        QStringList addresses;
        const QList<MeterInfo> meterInfos = deviceTree->meterInfosForConnection(it.key());
        for (const MeterInfo &meterInfo : meterInfos)
        {
            if (!meterInfo.address.isEmpty())
            {
                addresses.append(meterInfo.address);
            }
        }

        it.value()->setKnownMeterAddresses(addresses);
        it.value()->setHasMeter(!addresses.isEmpty());
    }
}
