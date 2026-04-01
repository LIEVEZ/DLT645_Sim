#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QAction>
#include <QLabel>
#include <QMainWindow>
#include <QSplitter>
#include <QStackedWidget>
#include <QTimer>
#include "Controllers/ProtocolController.h"
#include "Controllers/SerialController.h"
#include "Services/DataGenerator.h"
#include "Utils/Logger.h"
#include "ViewModels/MainWindowViewModel.h"
#include "Views/DataTable/DataTableWidget.h"
#include "Views/DeviceTree/DeviceTreeWidget.h"
#include "Views/Dialogs/CommunicationDialog.h"
#include "Views/Dialogs/NewConnectionDialog.h"
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
    void onConnectionStateChanged(bool connected);
    void onProtocolFrameReceived(const ProtocolFrame &frame);
    void onProtocolFrameSent(const ProtocolFrame &frame);
    void onRawDataReceived(const QByteArray &data);
    void onRawDataSent(const QByteArray &data);
    void onCommunicationInteraction();
    void onCommunicationDialogClosed();
    void onLoggerLogEntryAdded(LogLevel level, const QDateTime &timestamp, const QString &message);

    void onSearchTextChanged(const QString &text);
    void onConnectionSelected(const QString &connectionAlias);
    void onMeterSelected(const QString &meterAlias);
    void onMeterInfoSelected(const MeterInfo &meterInfo);
    void onOpenConnectionRequested(const QString &connectionAlias);
    void onCloseConnectionRequested(const QString &connectionAlias);
    void onDeleteConnectionRequested(const QString &connectionAlias);
    void onNewMeterRequested(const QString &connectionAlias);
    void onDeleteMeterRequested(const QString &connectionAlias, const QString &meterAlias);
    void onToggleDarkTheme(bool enabled);
    void onToggleLightTheme(bool enabled);

private:
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
    DeviceTreeWidget *deviceTreeWidget() const;
    void refreshMeterPresence();

    Ui::MainWindow *ui;

    SerialController *m_serialController;
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

    QAction *m_newConnectionAction;
    QAction *m_newMeterAction;
    QAction *m_openConnectionAction;
    QAction *m_closeConnectionAction;
    QAction *m_commInteractionAction;
    QAction *m_darkThemeAction;
    QAction *m_lightThemeAction;

    CommunicationDialog *m_communicationDialog;
};

#endif // MAINWINDOW_H
