#include "MainWindowViewModel.h"

namespace
{
const QString kDefaultMeterAddress = QStringLiteral("000000000000");
}

MainWindowViewModel::MainWindowViewModel(QObject *parent)
    : QObject(parent), m_connectionSelected(false), m_darkTheme(true), m_hasMeter(false), m_currentMeterAddress(kDefaultMeterAddress)
{
}

bool MainWindowViewModel::isConnected() const
{
    return isConnectionConnected(m_currentConnectionAlias);
}

bool MainWindowViewModel::isConnecting() const
{
    return isConnectionConnecting(m_currentConnectionAlias);
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

bool MainWindowViewModel::isConnectionConnected(const QString &alias) const
{
    return !alias.isEmpty() && m_connectedConnections.contains(alias);
}

bool MainWindowViewModel::isConnectionConnecting(const QString &alias) const
{
    return !alias.isEmpty() && m_connectingConnections.contains(alias);
}

bool MainWindowViewModel::hasActiveConnections() const
{
    return !m_connectedConnections.isEmpty() || !m_connectingConnections.isEmpty();
}

QString MainWindowViewModel::currentMeterAddress() const
{
    return m_currentMeterAddress;
}

QString MainWindowViewModel::connectionStatusText() const
{
    if (isConnecting())
    {
        return QStringLiteral("Connecting");
    }

    return isConnected() ? QStringLiteral("Connected") : QStringLiteral("Disconnected");
}

QColor MainWindowViewModel::connectionStatusColor() const
{
    if (isConnecting())
    {
        return QColor("#F5C542");
    }

    return isConnected() ? QColor("#50C878") : QColor("#F14C4C");
}

QString MainWindowViewModel::meterAddressText() const
{
    return QStringLiteral("Address: %1").arg(m_currentMeterAddress);
}

bool MainWindowViewModel::isOpenConnectionVisible() const
{
    return !isCloseConnectionVisible();
}

bool MainWindowViewModel::isCloseConnectionVisible() const
{
    return isConnected() || isConnecting();
}

bool MainWindowViewModel::isNewMeterEnabled() const
{
    return m_connectionSelected;
}

bool MainWindowViewModel::isCommunicationInteractionEnabled() const
{
    return isConnected() && !isConnecting();
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
    }

    m_connectedConnections.remove(alias);
    m_connectingConnections.remove(alias);

    emit stateChanged();
}

void MainWindowViewModel::setConnected(bool connected)
{
    if (m_currentConnectionAlias.isEmpty())
    {
        return;
    }

    if (connected)
    {
        markConnectionOpening(m_currentConnectionAlias, true);
    }
    else
    {
        markConnectionClosed(m_currentConnectionAlias);
    }
}

void MainWindowViewModel::setConnecting(bool connecting)
{
    if (m_currentConnectionAlias.isEmpty())
    {
        return;
    }

    if (connecting)
    {
        markConnectionOpening(m_currentConnectionAlias, false);
    }
    else
    {
        m_connectingConnections.remove(m_currentConnectionAlias);
        emit stateChanged();
    }
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
    selectMeter(address);
}

void MainWindowViewModel::selectMeter(const QString &address)
{
    const QString nextAddress = address.isEmpty() ? kDefaultMeterAddress : address;
    if (m_currentMeterAddress == nextAddress)
    {
        return;
    }

    m_currentMeterAddress = nextAddress;
    emit stateChanged();
}

void MainWindowViewModel::clearMeter()
{
    if (m_currentMeterAddress == kDefaultMeterAddress)
    {
        return;
    }

    m_currentMeterAddress = kDefaultMeterAddress;
    emit stateChanged();
}

bool MainWindowViewModel::setKnownMeterPresence(bool hasMeter)
{
    if (m_hasMeter == hasMeter)
    {
        return false;
    }

    m_hasMeter = hasMeter;
    emit hasMeterChanged(m_hasMeter);
    emit stateChanged();
    return true;
}
void MainWindowViewModel::selectConnection(const QString &alias)
{
    const bool selected = !alias.isEmpty();
    if (m_currentConnectionAlias == alias && m_connectionSelected == selected)
    {
        return;
    }

    m_currentConnectionAlias = alias;
    m_connectionSelected = selected;
    emit stateChanged();
}

void MainWindowViewModel::markConnectionOpening(const QString &alias, bool connected)
{
    if (alias.isEmpty())
    {
        return;
    }

    const bool connecting = !connected;
    if (isConnectionConnected(alias) == connected &&
        isConnectionConnecting(alias) == connecting)
    {
        return;
    }

    if (!m_connectionSelected && !alias.isEmpty())
    {
        m_currentConnectionAlias = alias;
        m_connectionSelected = true;
    }
    if (connected)
    {
        m_connectedConnections.insert(alias);
        m_connectingConnections.remove(alias);
    }
    else if (connecting)
    {
        m_connectingConnections.insert(alias);
        m_connectedConnections.remove(alias);
    }
    emit stateChanged();
}

void MainWindowViewModel::markConnectionClosed(const QString &alias)
{
    if (alias.isEmpty() ||
        (!m_connectedConnections.contains(alias) &&
         !m_connectingConnections.contains(alias)))
    {
        return;
    }

    m_connectedConnections.remove(alias);
    m_connectingConnections.remove(alias);
    emit stateChanged();
}
