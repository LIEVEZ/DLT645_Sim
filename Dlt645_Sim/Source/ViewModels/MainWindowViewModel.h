#ifndef MAINWINDOWVIEWMODEL_H
#define MAINWINDOWVIEWMODEL_H

#include <QObject>
#include <QColor>
#include <QMap>
#include "Models/ConnectionConfig.h"

class MainWindowViewModel : public QObject
{
    Q_OBJECT

public:
    explicit MainWindowViewModel(QObject *parent = nullptr);

    bool isConnected() const;
    bool isConnecting() const;
    bool isConnectionSelected() const;
    bool isDarkTheme() const;
    bool hasMeter() const;

    QString currentConnectionAlias() const;
    QString currentMeterAddress() const;

    QString connectionStatusText() const;
    QColor connectionStatusColor() const;
    QString meterAddressText() const;

    bool isOpenConnectionVisible() const;
    bool isCloseConnectionVisible() const;
    bool isNewMeterEnabled() const;
    bool isCommunicationInteractionEnabled() const;

    bool hasConnectionConfig(const QString &alias) const;
    ConnectionConfig connectionConfig(const QString &alias) const;
    void upsertConnectionConfig(const ConnectionConfig &config);
    void removeConnectionConfig(const QString &alias);

    void setConnected(bool connected);
    void setConnecting(bool connecting);
    void setConnectionSelected(bool selected);
    void setDarkTheme(bool darkTheme);
    void setHasMeter(bool hasMeter);
    void setCurrentConnectionAlias(const QString &alias);
    void setCurrentMeterAddress(const QString &address);
    void selectMeter(const QString &address);
    void clearMeter();
    bool setKnownMeterPresence(bool hasMeter);
    void selectConnection(const QString &alias);
    void markConnectionOpening(const QString &alias, bool connected);
    void markConnectionClosed(const QString &alias);

signals:
    void stateChanged();
    void hasMeterChanged(bool hasMeter);

private:
    bool m_connected;
    bool m_connecting;
    bool m_connectionSelected;
    bool m_darkTheme;
    bool m_hasMeter;
    QString m_currentConnectionAlias;
    QString m_currentMeterAddress;
    QMap<QString, ConnectionConfig> m_connectionConfigs;
};

#endif // MAINWINDOWVIEWMODEL_H
