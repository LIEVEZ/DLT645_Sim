#ifndef CONNECTIONMANAGER_H
#define CONNECTIONMANAGER_H

#include <QObject>
#include <QString>
#include "Controllers/SerialController.h"
#include "Controllers/TcpController.h"
#include "Models/ConnectionConfig.h"

struct ConnectionOpenResult
{
    bool started = false;
    bool connected = false;
    QString endpoint;
    QString errorMessage;
};

class ConnectionManager : public QObject
{
    Q_OBJECT

public:
    explicit ConnectionManager(SerialController *serialController,
                               TcpController *tcpController,
                               QObject *parent = nullptr);

    ConnectionOpenResult openConnection(const ConnectionConfig &config);
    void closeConnection();
    bool isOpen() const;
    bool isConnecting() const;

    static QString endpoint(const ConnectionConfig &config);

signals:
    void connectionStateChanged(bool connected);
    void errorOccurred(const QString &errorString);

private:
    void handleTransportError(const QString &errorString);

    SerialController *m_serialController;
    TcpController *m_tcpController;
    bool m_openInProgress;
    QString m_lastError;
};

#endif // CONNECTIONMANAGER_H
