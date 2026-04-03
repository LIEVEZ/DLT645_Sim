#ifndef CONNECTIONCONFIG_H
#define CONNECTIONCONFIG_H

#include <QString>

enum class ConnectionType
{
    Serial,
    TCP
};

struct ConnectionConfig
{
    ConnectionType type;
    QString alias;

    QString serialPort;
    int baudRate;
    int dataBits;
    int stopBits;
    int parity;
    int flowControl;

    QString tcpAddress;
    int tcpPort;

    ConnectionConfig()
        : type(ConnectionType::Serial), alias("连接1"), baudRate(2400), dataBits(8), stopBits(1), parity(2), flowControl(0), tcpPort(502)
    {
    }
};

#endif // CONNECTIONCONFIG_H
