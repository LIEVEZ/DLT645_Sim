#include "ConnectionManager.h"

#include "Controllers/SerialController.h"
#include "Controllers/TcpController.h"

ConnectionManager::ConnectionManager(QObject *parent)
    : QObject(parent)
{
}

ConnectionOpenResult ConnectionManager::openConnection(const ConnectionConfig &config)
{
    ConnectionOpenResult result;
    result.endpoint = endpoint(config);

    if (config.alias.isEmpty())
    {
        result.errorMessage = QStringLiteral("Connection alias is empty");
        return result;
    }

    if (isEndpointInUse(result.endpoint, config.alias))
    {
        result.errorMessage = QString("Connection %1 is already in use").arg(result.endpoint);
        return result;
    }

    if (isOpen(config.alias) || isConnecting(config.alias))
    {
        result.started = true;
        result.connected = isOpen(config.alias);
        return result;
    }

    m_connectionConfigs.insert(config.alias, config);
    m_lastErrors.remove(config.alias);
    m_openInProgress.insert(config.alias);

    if (config.type == ConnectionType::Serial)
    {
        if (m_tcpControllers.contains(config.alias))
        {
            removeConnection(config.alias);
            m_connectionConfigs.insert(config.alias, config);
            m_openInProgress.insert(config.alias);
        }

        SerialController *controller = ensureSerialController(config.alias);
        SerialConfig serialConfig;
        serialConfig.portName = config.serialPort;
        serialConfig.baudRate = config.baudRate;
        serialConfig.dataBits = static_cast<QSerialPort::DataBits>(config.dataBits);
        serialConfig.stopBits = static_cast<QSerialPort::StopBits>(config.stopBits);
        serialConfig.parity = static_cast<QSerialPort::Parity>(config.parity);
        serialConfig.flowControl = static_cast<QSerialPort::FlowControl>(config.flowControl);

        result.started = controller && controller->openPort(serialConfig);
        result.connected = result.started;
    }
    else
    {
        if (m_serialControllers.contains(config.alias))
        {
            removeConnection(config.alias);
            m_connectionConfigs.insert(config.alias, config);
            m_openInProgress.insert(config.alias);
        }

        TcpController *controller = ensureTcpController(config.alias);
        TcpConfig tcpConfig;
        tcpConfig.hostAddress = config.tcpAddress;
        tcpConfig.port = static_cast<quint16>(config.tcpPort);

        result.started = controller && controller->openConnection(tcpConfig);
        result.connected = controller && controller->isOpen();
    }

    m_openInProgress.remove(config.alias);
    if (!result.started)
    {
        result.errorMessage = m_lastErrors.value(config.alias);
        if (result.errorMessage.isEmpty())
        {
            result.errorMessage = QString("Failed to open connection: %1").arg(result.endpoint);
        }
    }

    return result;
}

void ConnectionManager::closeConnection(const QString &connectionAlias)
{
    SerialController *serial = m_serialControllers.value(connectionAlias, nullptr);
    if (serial && serial->isOpen())
    {
        serial->closePort();
    }

    TcpController *tcp = m_tcpControllers.value(connectionAlias, nullptr);
    if (tcp && (tcp->isOpen() || tcp->isConnecting()))
    {
        tcp->closeConnection();
    }
}

void ConnectionManager::closeAllConnections()
{
    const QStringList aliases = m_connectionConfigs.keys();
    for (const QString &alias : aliases)
    {
        closeConnection(alias);
    }
}

void ConnectionManager::removeConnection(const QString &connectionAlias)
{
    closeConnection(connectionAlias);

    SerialController *serial = m_serialControllers.take(connectionAlias);
    if (serial)
    {
        serial->deleteLater();
    }

    TcpController *tcp = m_tcpControllers.take(connectionAlias);
    if (tcp)
    {
        tcp->deleteLater();
    }

    m_connectionConfigs.remove(connectionAlias);
    m_lastErrors.remove(connectionAlias);
    m_openInProgress.remove(connectionAlias);
}

bool ConnectionManager::isOpen(const QString &connectionAlias) const
{
    SerialController *serial = m_serialControllers.value(connectionAlias, nullptr);
    TcpController *tcp = m_tcpControllers.value(connectionAlias, nullptr);
    return (serial && serial->isOpen()) || (tcp && tcp->isOpen());
}

bool ConnectionManager::isConnecting(const QString &connectionAlias) const
{
    TcpController *tcp = m_tcpControllers.value(connectionAlias, nullptr);
    return tcp && tcp->isConnecting();
}

bool ConnectionManager::isOpen() const
{
    for (auto it = m_connectionConfigs.cbegin(); it != m_connectionConfigs.cend(); ++it)
    {
        if (isOpen(it.key()))
        {
            return true;
        }
    }
    return false;
}

bool ConnectionManager::isConnecting() const
{
    for (auto it = m_connectionConfigs.cbegin(); it != m_connectionConfigs.cend(); ++it)
    {
        if (isConnecting(it.key()))
        {
            return true;
        }
    }
    return false;
}

SerialController *ConnectionManager::serialController(const QString &connectionAlias) const
{
    return m_serialControllers.value(connectionAlias, nullptr);
}

TcpController *ConnectionManager::tcpController(const QString &connectionAlias) const
{
    return m_tcpControllers.value(connectionAlias, nullptr);
}

QStringList ConnectionManager::connectedSerialPorts() const
{
    QStringList ports;
    for (auto it = m_serialControllers.cbegin(); it != m_serialControllers.cend(); ++it)
    {
        SerialController *controller = it.value();
        if (controller && controller->isOpen())
        {
            const QString portName = controller->getConfig().portName;
            if (!portName.isEmpty())
            {
                ports.append(portName);
            }
        }
    }
    ports.removeDuplicates();
    return ports;
}

QString ConnectionManager::endpoint(const ConnectionConfig &config)
{
    if (config.type == ConnectionType::Serial)
    {
        return config.serialPort;
    }

    return QString("%1:%2").arg(config.tcpAddress).arg(config.tcpPort);
}

SerialController *ConnectionManager::ensureSerialController(const QString &connectionAlias)
{
    SerialController *controller = m_serialControllers.value(connectionAlias, nullptr);
    if (controller)
    {
        return controller;
    }

    controller = new SerialController(this);
    m_serialControllers.insert(connectionAlias, controller);
    connect(controller, &SerialController::connectionStateChanged,
            this, [this, connectionAlias](bool connected)
            {
                emit connectionStateChanged(connectionAlias, connected);
            });
    connect(controller, &SerialController::errorOccurred,
            this, [this, connectionAlias](const QString &errorString)
            {
                m_lastErrors.insert(connectionAlias, errorString);
                if (!m_openInProgress.contains(connectionAlias))
                {
                    emit errorOccurred(connectionAlias, errorString);
                }
            });
    return controller;
}

TcpController *ConnectionManager::ensureTcpController(const QString &connectionAlias)
{
    TcpController *controller = m_tcpControllers.value(connectionAlias, nullptr);
    if (controller)
    {
        return controller;
    }

    controller = new TcpController(this);
    m_tcpControllers.insert(connectionAlias, controller);
    connect(controller, &TcpController::connectionStateChanged,
            this, [this, connectionAlias](bool connected)
            {
                emit connectionStateChanged(connectionAlias, connected);
            });
    connect(controller, &TcpController::errorOccurred,
            this, [this, connectionAlias](const QString &errorString)
            {
                m_lastErrors.insert(connectionAlias, errorString);
                if (!m_openInProgress.contains(connectionAlias))
                {
                    emit errorOccurred(connectionAlias, errorString);
                }
            });
    return controller;
}

bool ConnectionManager::isEndpointInUse(const QString &connectionEndpoint,
                                        const QString &exceptConnectionAlias) const
{
    for (auto it = m_connectionConfigs.cbegin(); it != m_connectionConfigs.cend(); ++it)
    {
        if (it.key() == exceptConnectionAlias)
        {
            continue;
        }
        if ((isOpen(it.key()) || isConnecting(it.key())) &&
            endpoint(it.value()) == connectionEndpoint)
        {
            return true;
        }
    }
    return false;
}
