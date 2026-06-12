#ifndef CONNECTIONINFOMAPPER_H
#define CONNECTIONINFOMAPPER_H

#include "Models/ConnectionConfig.h"
#include "Models/ConnectionInfo.h"

class ConnectionInfoMapper
{
public:
    static ConnectionInfo fromConfig(const ConnectionConfig &config, bool connected);
};

#endif // CONNECTIONINFOMAPPER_H