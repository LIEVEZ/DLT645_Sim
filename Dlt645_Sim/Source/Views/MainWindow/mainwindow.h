#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QAction>
#include <QMap>
#include <QLabel>
#include <QMainWindow>
#include <QStringList>
#include <QSplitter>
#include <QStackedWidget>
#include <QTimer>
#include <QWidget>
#include "Controllers/ProtocolController.h"
#include "Controllers/SerialController.h"
#include "Controllers/TcpController.h"
#include "Services/DataGenerator.h"
#include "Utils/Logger.h"
#include "ViewModels/MainWindowViewModel.h"
#include "Views/DataTable/DataTableWidget.h"
#include "Views/DeviceTree/DeviceTreeWidget.h"
#include "Views/Dialogs/CommunicationDialog.h"
#include "Views/Dialogs/MasterQueryDialog.h"
#include "Views/Dialogs/NewConnectionDialog.h"
#include "Views/Dialogs/QueryFrameDialog.h"
#include "Views/OperationLog/OperationLogWidget.h"

QT_BEGIN_NAMESPACE
namespace Ui
{
    class MainWindow;
}
QT_END_NAMESPACE

struct ProtocolFrame;
struct ConnectionInfo;
struct MeterInfo;
class FrameParseDialog;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onNewConnection();
    void onNewMeterDialog();
    void onOpenConnection();
    void onCloseConnection();
    void onDataUpdated();
    void onRealTimeDataRotationTimeout();
    void onConnectionStateChanged(bool connected);
    void onProtocolFrameReceived(const ProtocolFrame &frame, const QByteArray &rawFrame);
    void onProtocolFrameSent(const ProtocolFrame &frame, const QByteArray &rawFrame);
    void onRawDataReceived(const QByteArray &data);
    void onRawDataSent(const QByteArray &data);
    void onCommunicationInteraction();
    void onMasterQueryWindow();
    void onCommunicationDialogClosed();
    void onMasterQueryDialogClosed();
    void onQueryFrameGenerator();
    void onQueryFrameDialogClosed();
    void onFrameParseDialog();
    void onLoggerLogEntryAdded(LogLevel level, const QDateTime &timestamp, const QString &message);

    void onSearchTextChanged(const QString &text);
    void onConnectionSelected(const QString &connectionAlias);
    void onMeterSelected(const QString &meterAlias);
    void onMeterInfoSelected(const MeterInfo &meterInfo);
    void onOpenConnectionRequested(const QString &connectionAlias);
    void onCloseConnectionRequested(const QString &connectionAlias);
    void onDeleteConnectionRequested(const QString &connectionAlias);
    void onDeleteAllMetersRequested(const QString &connectionAlias);
    void onEditConnectionRequested(const QString &connectionAlias);
    void onNewMeterRequested(const QString &connectionAlias);
    void onBatchNewMeterDialog();
    void onBatchNewMeterRequested(const QString &connectionAlias);
    void onDeleteMeterRequested(const QString &connectionAlias, const QString &meterAlias);
    void onToggleDarkTheme(bool enabled);
    void onToggleLightTheme(bool enabled);

private:
    void attachMeterPersistence(MeterModel *meterModel);
    void setupUI();
    void setupConnections();
    void setupMenuBar();
    void setupToolBar();
    void setupStatusBar();
    void applyDarkTheme();
    void applyLightTheme();
    void updateConnectionStatus(bool connected);
    void updateToolBarButtons();
    void updateCommunicationDialogState();
    void updateMasterQueryDialogState();
    void updateQueryFrameDialogState();
    void updateFrameParseDialogState();
    DeviceTreeWidget *deviceTreeWidget() const;
    QStringList collectMeterAddresses() const;
    QStringList collectMeterAddressesForConnection(const QString &connectionAlias) const;
    MeterInfo findMeterInfo(const QString &connectionAlias, const QString &meterAlias) const;
    MeterModel *createMeterModel(const MeterInfo &meterInfo);
    bool openConnectionInternal(const QString &connectionAlias, const ConnectionConfig &config, bool addConnectionNode);
    void closeConnectionInternal(const QString &connectionAlias);
    ConnectionInfo buildConnectionInfo(const ConnectionConfig &config, bool connected) const;
    QString connectionEndpoint(const ConnectionConfig &config) const;
    void applyCurrentMeter(const MeterInfo &meterInfo);
    void clearCurrentMeter();
    void refreshMeterPresence();
    void updateRealTimeDataDisplay(bool advancePhase = false);

    Ui::MainWindow *ui;

    SerialController *m_serialController;
    TcpController *m_tcpController;
    ProtocolController *m_protocolController;
    DataGenerator *m_dataGenerator;
    MainWindowViewModel *m_viewModel;
    OperationLogWidget *m_operationLogWidget;
    DataTableWidget *m_dataTableWidget;

    QSplitter *m_mainSplitter;
    QSplitter *m_bottomSplitter;
    QStackedWidget *m_connectionButtonStack;
    QLabel *m_connectionStatusLabel;
    QLabel *m_meterAddressLabel;
    QWidget *m_realTimeBarWidget;
    QLabel *m_realTimeDataLabel;
    QTimer *m_realTimeDataTimer;
    int m_realTimePhaseIndex;

    QAction *m_newConnectionAction;
    QAction *m_newMeterAction;
    QAction *m_batchNewMeterAction;
    QAction *m_openConnectionAction;
    QAction *m_openConnectionSeparatorAction;
    QAction *m_closeConnectionAction;
    QAction *m_closeConnectionSeparatorAction;
    QAction *m_commInteractionAction;
    QAction *m_masterQueryAction;
    QAction *m_queryFrameAction;
    QAction *m_frameParseAction;
    QAction *m_darkThemeAction;
    QAction *m_lightThemeAction;

    CommunicationDialog *m_communicationDialog;
    MasterQueryDialog *m_masterQueryDialog;
    QueryFrameDialog *m_queryFrameDialog;
    FrameParseDialog *m_frameParseDialog;
    class MeterDataStore *m_meterDataStore;
    QMap<QString, MeterModel *> m_meterModelsByAddress;
    MeterModel *m_currentMeterModel;
};

#endif // MAINWINDOW_H
