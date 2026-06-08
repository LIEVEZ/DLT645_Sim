#include "TcpController.h"
#include "Utils/Logger.h"

TcpController::TcpController(QObject *parent)
    : QObject(parent), m_tcpSocket(new QTcpSocket(this)), m_connectTimer(new QTimer(this))
{
    m_connectTimer->setSingleShot(true);

    connect(m_tcpSocket, &QTcpSocket::readyRead, this, &TcpController::onReadyRead);
    connect(m_tcpSocket, &QTcpSocket::connected, this, &TcpController::onConnected);
    connect(m_tcpSocket, &QTcpSocket::disconnected, this, &TcpController::onDisconnected);
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    connect(m_tcpSocket, &QTcpSocket::errorOccurred, this, &TcpController::onErrorOccurred);
#else
    connect(m_tcpSocket,
            QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::error),
            this,
            &TcpController::onErrorOccurred);
#endif
    connect(m_connectTimer, &QTimer::timeout, this, &TcpController::onConnectTimeout);
}

TcpController::~TcpController()
{
    closeConnection();
}

bool TcpController::openConnection(const TcpConfig &config)
{
    if (m_tcpSocket->state() != QAbstractSocket::UnconnectedState)
    {
        closeConnection();
    }

    m_receiveBuffer.clear();
    m_config = config;
    m_tcpSocket->connectToHost(config.hostAddress, config.port);
    m_connectTimer->start(config.connectTimeoutMs > 0 ? config.connectTimeoutMs : 3000);

    LOG_INFO(QString("TCP connecting: %1:%2")
                 .arg(config.hostAddress)
                 .arg(config.port));
    return true;
}

void TcpController::closeConnection()
{
    m_connectTimer->stop();

    if (m_tcpSocket->state() == QAbstractSocket::UnconnectedState)
    {
        return;
    }

    if (m_tcpSocket->state() == QAbstractSocket::ConnectingState ||
        m_tcpSocket->state() == QAbstractSocket::HostLookupState)
    {
        m_tcpSocket->abort();
        m_receiveBuffer.clear();
        emit connectionStateChanged(false);
        return;
    }

    m_tcpSocket->disconnectFromHost();
}

bool TcpController::sendData(const QByteArray &data)
{
    if (!isOpen())
    {
        LOG_ERROR("Cannot send data: TCP socket is not connected");
        return false;
    }

    const qint64 bytesWritten = m_tcpSocket->write(data);
    if (bytesWritten != data.size())
    {
        LOG_ERROR(QString("Failed to send TCP data: %1 bytes written, expected %2")
                      .arg(bytesWritten)
                      .arg(data.size()));
        return false;
    }

    m_tcpSocket->flush();
    LOG_DEBUG(QString("Sent %1 TCP bytes: %2")
                  .arg(bytesWritten)
                  .arg(QString(data.toHex())));
    emit dataSent(data);
    return true;
}

bool TcpController::isOpen() const
{
    return m_tcpSocket->state() == QAbstractSocket::ConnectedState;
}

bool TcpController::isConnecting() const
{
    return m_tcpSocket->state() == QAbstractSocket::ConnectingState ||
           m_tcpSocket->state() == QAbstractSocket::HostLookupState;
}

TcpConfig TcpController::getConfig() const
{
    return m_config;
}

void TcpController::onReadyRead()
{
    const QByteArray data = m_tcpSocket->readAll();
    m_receiveBuffer.append(data);

    LOG_DEBUG(QString("Received %1 TCP bytes: %2")
                  .arg(data.size())
                  .arg(QString(data.toHex())));

    emit dataReceived(m_receiveBuffer);
    m_receiveBuffer.clear();
}

void TcpController::onConnected()
{
    m_connectTimer->stop();
    LOG_INFO(QString("TCP connected: %1:%2")
                 .arg(m_config.hostAddress)
                 .arg(m_config.port));
    emit connectionStateChanged(true);
}

void TcpController::onDisconnected()
{
    m_connectTimer->stop();
    m_receiveBuffer.clear();
    LOG_INFO(QString("TCP disconnected: %1:%2")
                 .arg(m_config.hostAddress)
                 .arg(m_config.port));
    emit connectionStateChanged(false);
}

void TcpController::onErrorOccurred(QAbstractSocket::SocketError error)
{
    if (error == QAbstractSocket::UnknownSocketError)
    {
        return;
    }

    const QString errorString = m_tcpSocket->errorString();
    LOG_ERROR(QString("TCP socket error: %1").arg(errorString));
    m_connectTimer->stop();
    emit errorOccurred(errorString);
}

void TcpController::onConnectTimeout()
{
    if (!isConnecting())
    {
        return;
    }

    const QString errorString = QString("TCP connect timeout: %1:%2")
                                    .arg(m_config.hostAddress)
                                    .arg(m_config.port);
    LOG_ERROR(errorString);
    m_tcpSocket->abort();
    m_receiveBuffer.clear();
    emit errorOccurred(errorString);
    emit connectionStateChanged(false);
}
