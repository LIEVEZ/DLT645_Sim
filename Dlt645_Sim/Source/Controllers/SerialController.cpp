#include "SerialController.h"
#include "Utils/Logger.h"
#include <QDebug>

SerialController::SerialController(QObject *parent)
    : QObject(parent), m_serialPort(new QSerialPort(this))
{
    connect(m_serialPort, &QSerialPort::readyRead, this, &SerialController::onReadyRead);
    connect(m_serialPort, QOverload<QSerialPort::SerialPortError>::of(&QSerialPort::errorOccurred),
            this, &SerialController::onErrorOccurred);
}

SerialController::~SerialController()
{
    closePort();
}

QList<QSerialPortInfo> SerialController::getAvailablePorts()
{
    return QSerialPortInfo::availablePorts();
}

bool SerialController::openPort(const SerialConfig &config)
{
    if (m_serialPort->isOpen())
    {
        closePort();
    }

    m_config = config;
    m_serialPort->setPortName(config.portName);
    m_serialPort->setBaudRate(config.baudRate);
    m_serialPort->setDataBits(config.dataBits);
    m_serialPort->setParity(config.parity);
    m_serialPort->setStopBits(config.stopBits);
    m_serialPort->setFlowControl(config.flowControl);

    if (m_serialPort->open(QIODevice::ReadWrite))
    {
        LOG_INFO(QString("Serial port %1 opened successfully, baud rate: %2")
                     .arg(config.portName)
                     .arg(config.baudRate));
        emit connectionStateChanged(true);
        return true;
    }
    else
    {
        QString error = m_serialPort->errorString();
        LOG_ERROR(QString("Failed to open serial port %1: %2")
                      .arg(config.portName)
                      .arg(error));
        emit errorOccurred(error);
        return false;
    }
}

void SerialController::closePort()
{
    if (m_serialPort->isOpen())
    {
        m_serialPort->close();
        m_receiveBuffer.clear();
        LOG_INFO(QString("Serial port %1 closed").arg(m_config.portName));
        emit connectionStateChanged(false);
    }
}

bool SerialController::sendData(const QByteArray &data)
{
    if (!m_serialPort->isOpen())
    {
        LOG_ERROR("Cannot send data: serial port is not open");
        return false;
    }

    qint64 bytesWritten = m_serialPort->write(data);
    if (bytesWritten == data.size())
    {
        LOG_DEBUG(QString("Sent %1 bytes: %2")
                      .arg(bytesWritten)
                      .arg(QString(data.toHex())));
        emit dataSent(data);
        return true;
    }
    else
    {
        LOG_ERROR(QString("Failed to send data: %1 bytes written, expected %2")
                      .arg(bytesWritten)
                      .arg(data.size()));
        return false;
    }
}

bool SerialController::isOpen() const
{
    return m_serialPort->isOpen();
}

SerialConfig SerialController::getConfig() const
{
    return m_config;
}

void SerialController::onReadyRead()
{
    QByteArray data = m_serialPort->readAll();
    m_receiveBuffer.append(data);

    LOG_DEBUG(QString("Received %1 bytes: %2")
                  .arg(data.size())
                  .arg(QString(data.toHex())));

    emit dataReceived(m_receiveBuffer);
    m_receiveBuffer.clear();
}

void SerialController::onErrorOccurred(QSerialPort::SerialPortError error)
{
    if (error != QSerialPort::NoError)
    {
        QString errorString = m_serialPort->errorString();
        LOG_ERROR(QString("Serial port error: %1").arg(errorString));
        emit errorOccurred(errorString);

        if (error == QSerialPort::ResourceError ||
            error == QSerialPort::PermissionError ||
            error == QSerialPort::OpenError)
        {
            closePort();
        }
    }
}
