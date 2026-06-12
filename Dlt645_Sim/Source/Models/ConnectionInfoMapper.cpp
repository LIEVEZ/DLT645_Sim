#include "ConnectionInfoMapper.h"

#include "Controllers/ConnectionManager.h"

ConnectionInfo ConnectionInfoMapper::fromConfig(const ConnectionConfig &config, bool connected)
{
    ConnectionInfo connectionInfo;
    connectionInfo.alias = config.alias;
    connectionInfo.type = (config.type == ConnectionType::Serial) ? QStringLiteral("Serial") : QStringLiteral("TCP");
    connectionInfo.port = ConnectionManager::endpoint(config);
    connectionInfo.isConnected = connected;
    connectionInfo.baudRate = config.baudRate;
    connectionInfo.dataBits = config.dataBits;
    connectionInfo.stopBits = config.stopBits;
    connectionInfo.parity = config.parity;
    connectionInfo.flowControl = config.flowControl;
    connectionInfo.tcpAddress = config.tcpAddress;
    connectionInfo.tcpPort = config.tcpPort;
    return connectionInfo;
}