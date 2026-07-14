#ifndef CONNECTIONMANAGER_H
#define CONNECTIONMANAGER_H

#include <QMap>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>

#include "Models/ConnectionConfig.h"

class SerialController;
class TcpController;

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
    explicit ConnectionManager(QObject *parent = nullptr);

    ConnectionOpenResult openConnection(const ConnectionConfig &config);
    void closeConnection(const QString &connectionAlias);
    void closeAllConnections();
    void removeConnection(const QString &connectionAlias);

    bool isOpen(const QString &connectionAlias) const;
    bool isConnecting(const QString &connectionAlias) const;
    bool isOpen() const;
    bool isConnecting() const;

    SerialController *serialController(const QString &connectionAlias) const;
    TcpController *tcpController(const QString &connectionAlias) const;
    QStringList connectedSerialPorts() const;

    static QString endpoint(const ConnectionConfig &config);

signals:
    void connectionStateChanged(const QString &connectionAlias, bool connected);
    void errorOccurred(const QString &connectionAlias, const QString &errorString);

private:
    SerialController *ensureSerialController(const QString &connectionAlias);
    TcpController *ensureTcpController(const QString &connectionAlias);
    bool isEndpointInUse(const QString &connectionEndpoint,
                         const QString &exceptConnectionAlias) const;

    QMap<QString, SerialController *> m_serialControllers;
    QMap<QString, TcpController *> m_tcpControllers;
    QMap<QString, ConnectionConfig> m_connectionConfigs;
    QMap<QString, QString> m_lastErrors;
    QSet<QString> m_openInProgress;
};

#endif // CONNECTIONMANAGER_H
