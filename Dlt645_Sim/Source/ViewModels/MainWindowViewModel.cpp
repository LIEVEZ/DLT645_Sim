#include "MainWindowViewModel.h"

MainWindowViewModel::MainWindowViewModel(QObject *parent)
    : QObject(parent), m_connected(false), m_connecting(false), m_connectionSelected(false), m_darkTheme(true), m_hasMeter(false), m_currentMeterAddress("000000000000")
{
}

bool MainWindowViewModel::isConnected() const
{
    return m_connected;
}

bool MainWindowViewModel::isConnecting() const
{
    return m_connecting;
}

bool MainWindowViewModel::isConnectionSelected() const
{
    return m_connectionSelected;
}

bool MainWindowViewModel::isDarkTheme() const
{
    return m_darkTheme;
}

bool MainWindowViewModel::hasMeter() const
{
    return m_hasMeter;
}

QString MainWindowViewModel::currentConnectionAlias() const
{
    return m_currentConnectionAlias;
}

QString MainWindowViewModel::currentMeterAddress() const
{
    return m_currentMeterAddress;
}

QString MainWindowViewModel::connectionStatusText() const
{
    if (m_connecting)
    {
        return QStringLiteral("Connecting");
    }

    return m_connected ? QStringLiteral("Connected") : QStringLiteral("Disconnected");
}

QColor MainWindowViewModel::connectionStatusColor() const
{
    if (m_connecting)
    {
        return QColor("#F5C542");
    }

    return m_connected ? QColor("#50C878") : QColor("#F14C4C");
}

QString MainWindowViewModel::meterAddressText() const
{
    return QStringLiteral("Address: %1").arg(m_currentMeterAddress);
}

bool MainWindowViewModel::isOpenConnectionVisible() const
{
    return !m_connected && !m_connecting;
}

bool MainWindowViewModel::isCloseConnectionVisible() const
{
    return m_connected || m_connecting;
}

bool MainWindowViewModel::isNewMeterEnabled() const
{
    return m_connectionSelected;
}

bool MainWindowViewModel::isCommunicationInteractionEnabled() const
{
    return m_connected && !m_connecting;
}

bool MainWindowViewModel::hasConnectionConfig(const QString &alias) const
{
    return m_connectionConfigs.contains(alias);
}

ConnectionConfig MainWindowViewModel::connectionConfig(const QString &alias) const
{
    return m_connectionConfigs.value(alias);
}

void MainWindowViewModel::upsertConnectionConfig(const ConnectionConfig &config)
{
    m_connectionConfigs.insert(config.alias, config);
    emit stateChanged();
}

void MainWindowViewModel::removeConnectionConfig(const QString &alias)
{
    if (!m_connectionConfigs.contains(alias))
    {
        return;
    }

    m_connectionConfigs.remove(alias);

    if (m_currentConnectionAlias == alias)
    {
        m_currentConnectionAlias.clear();
        m_connectionSelected = false;
        m_connected = false;
        m_connecting = false;
    }

    emit stateChanged();
}

void MainWindowViewModel::setConnected(bool connected)
{
    const bool nextConnecting = connected ? false : m_connecting;
    if (m_connected == connected && m_connecting == nextConnecting)
    {
        return;
    }

    m_connected = connected;
    m_connecting = nextConnecting;
    emit stateChanged();
}

void MainWindowViewModel::setConnecting(bool connecting)
{
    const bool nextConnected = connecting ? false : m_connected;
    if (m_connecting == connecting && m_connected == nextConnected)
    {
        return;
    }

    m_connecting = connecting;
    m_connected = nextConnected;
    emit stateChanged();
}

void MainWindowViewModel::setConnectionSelected(bool selected)
{
    if (m_connectionSelected == selected)
    {
        return;
    }

    m_connectionSelected = selected;
    emit stateChanged();
}

void MainWindowViewModel::setDarkTheme(bool darkTheme)
{
    if (m_darkTheme == darkTheme)
    {
        return;
    }

    m_darkTheme = darkTheme;
    emit stateChanged();
}

void MainWindowViewModel::setHasMeter(bool hasMeter)
{
    if (m_hasMeter == hasMeter)
    {
        return;
    }

    m_hasMeter = hasMeter;
    emit hasMeterChanged(m_hasMeter);
    emit stateChanged();
}

void MainWindowViewModel::setCurrentConnectionAlias(const QString &alias)
{
    if (m_currentConnectionAlias == alias)
    {
        return;
    }

    m_currentConnectionAlias = alias;
    emit stateChanged();
}

void MainWindowViewModel::setCurrentMeterAddress(const QString &address)
{
    const QString nextAddress = address.isEmpty() ? QStringLiteral("000000000000") : address;
    if (m_currentMeterAddress == nextAddress)
    {
        return;
    }

    m_currentMeterAddress = nextAddress;
    emit stateChanged();
}
