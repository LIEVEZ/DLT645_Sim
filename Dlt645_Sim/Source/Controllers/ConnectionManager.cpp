#include "ConnectionManager.h"

ConnectionManager::ConnectionManager(SerialController *serialController,
                                     TcpController *tcpController,
                                     QObject *parent)
    : QObject(parent),
      m_serialController(serialController),
      m_tcpController(tcpController),
      m_openInProgress(false)
{
    if (m_serialController)
    {
        connect(m_serialController, &SerialController::connectionStateChanged,
                this, &ConnectionManager::connectionStateChanged);
        connect(m_serialController, &SerialController::errorOccurred,
                this, &ConnectionManager::handleTransportError);
    }

    if (m_tcpController)
    {
        connect(m_tcpController, &TcpController::connectionStateChanged,
                this, &ConnectionManager::connectionStateChanged);
        connect(m_tcpController, &TcpController::errorOccurred,
                this, &ConnectionManager::handleTransportError);
    }
}
ConnectionOpenResult ConnectionManager::openConnection(const ConnectionConfig &config)
{
    ConnectionOpenResult result;
    result.endpoint = endpoint(config);

    if (config.type == ConnectionType::Serial)
    {
        if (!m_serialController)
        {
            result.errorMessage = QStringLiteral("Serial controller is not initialized");
            return result;
        }

        SerialConfig serialConfig;
        serialConfig.portName = config.serialPort;
        serialConfig.baudRate = config.baudRate;
        serialConfig.dataBits = static_cast<QSerialPort::DataBits>(config.dataBits);
        serialConfig.stopBits = static_cast<QSerialPort::StopBits>(config.stopBits);
        serialConfig.parity = static_cast<QSerialPort::Parity>(config.parity);
        serialConfig.flowControl = static_cast<QSerialPort::FlowControl>(config.flowControl);

        m_lastError.clear();
        m_openInProgress = true;
        result.started = m_serialController->openPort(serialConfig);
        m_openInProgress = false;
        result.connected = result.started;
        if (!result.started)
        {
            result.errorMessage = m_lastError.isEmpty()
                                      ? QString("Failed to open serial port: %1").arg(result.endpoint)
                                      : m_lastError;
        }
        return result;
    }

    if (!m_tcpController)
    {
        result.errorMessage = QStringLiteral("TCP controller is not initialized");
        return result;
    }

    TcpConfig tcpConfig;
    tcpConfig.hostAddress = config.tcpAddress;
    tcpConfig.port = static_cast<quint16>(config.tcpPort);

    result.started = m_tcpController->openConnection(tcpConfig);
    result.connected = false;
    if (!result.started)
    {
        result.errorMessage = QString("Failed to open TCP connection: %1").arg(result.endpoint);
    }
    return result;
}

void ConnectionManager::closeConnection()
{
    if (m_serialController && m_serialController->isOpen())
    {
        m_serialController->closePort();
    }

    if (m_tcpController && (m_tcpController->isOpen() || m_tcpController->isConnecting()))
    {
        m_tcpController->closeConnection();
    }
}

bool ConnectionManager::isOpen() const
{
    return (m_serialController && m_serialController->isOpen()) ||
           (m_tcpController && m_tcpController->isOpen());
}

bool ConnectionManager::isConnecting() const
{
    return m_tcpController && m_tcpController->isConnecting();
}

QString ConnectionManager::endpoint(const ConnectionConfig &config)
{
    if (config.type == ConnectionType::Serial)
    {
        return config.serialPort;
    }

    return QString("%1:%2").arg(config.tcpAddress).arg(config.tcpPort);
}

void ConnectionManager::handleTransportError(const QString &errorString)
{
    m_lastError = errorString;
    if (m_openInProgress)
    {
        return;
    }

    emit errorOccurred(errorString);
}
