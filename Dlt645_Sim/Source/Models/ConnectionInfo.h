#ifndef CONNECTIONINFO_H
#define CONNECTIONINFO_H

#include <QString>

struct ConnectionInfo
{
    QString alias;
    QString type;
    QString port;
    bool isConnected = false;

    int baudRate = 0;
    int dataBits = 0;
    int stopBits = 0;
    int parity = 0;
    int flowControl = 0;

    QString tcpAddress;
    int tcpPort = 0;
};

#endif // CONNECTIONINFO_H