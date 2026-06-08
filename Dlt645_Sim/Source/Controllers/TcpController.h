#ifndef TCPCONTROLLER_H
#define TCPCONTROLLER_H

#include <QAbstractSocket>
#include <QByteArray>
#include <QObject>
#include <QTimer>
#include <QTcpSocket>

struct TcpConfig
{
    QString hostAddress;
    quint16 port;
    int connectTimeoutMs;

    TcpConfig()
        : hostAddress("127.0.0.1"), port(502), connectTimeoutMs(3000)
    {
    }
};

class TcpController : public QObject
{
    Q_OBJECT

public:
    explicit TcpController(QObject *parent = nullptr);
    ~TcpController();

    bool openConnection(const TcpConfig &config);
    void closeConnection();
    bool sendData(const QByteArray &data);
    bool isOpen() const;
    bool isConnecting() const;
    TcpConfig getConfig() const;

signals:
    void dataReceived(const QByteArray &data);
    void dataSent(const QByteArray &data);
    void connectionStateChanged(bool connected);
    void errorOccurred(const QString &errorString);

private slots:
    void onReadyRead();
    void onConnected();
    void onDisconnected();
    void onErrorOccurred(QAbstractSocket::SocketError error);
    void onConnectTimeout();

private:
    QTcpSocket *m_tcpSocket;
    QTimer *m_connectTimer;
    TcpConfig m_config;
    QByteArray m_receiveBuffer;
};

#endif // TCPCONTROLLER_H
