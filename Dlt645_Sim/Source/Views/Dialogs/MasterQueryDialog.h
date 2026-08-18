#ifndef MASTERQUERYDIALOG_H
#define MASTERQUERYDIALOG_H

#include <QDialog>
#include <QFutureWatcher>
#include <QSerialPortInfo>
#include <QVector>

#include "Controllers/MasterQueryController.h"

namespace ui
{
struct ThemePalette;
}

class QCheckBox;
class QCloseEvent;
class QComboBox;
class QGroupBox;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QSpinBox;
class QTableWidget;
class QTextEdit;
class QLabel;
class MeterModel;
class FrameParseResultWidget;

class MasterQueryDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MasterQueryDialog(QWidget *parent = nullptr);
    ~MasterQueryDialog();

    void setUnavailablePorts(const QStringList &ports);
    void setDefaultMeterAddress(const QString &address);
    void setDefaultMeterCredentials(const QString &password, const QString &operatorCode);
    void setDefaultIncludePrefixFE(bool includePrefixFE);
    void applyDarkTheme();
    void applyLightTheme();

signals:
    void dialogClosed();

private slots:
    void onRefreshPortsClicked();
    void onConnectClicked();
    void onDisconnectClicked();
    void onQueryTypeChanged(int index);
    void onCategoryChanged(int index);
    void onSearchTextChanged(const QString &text);
    void onCurrentItemChanged(QListWidgetItem *current, QListWidgetItem *previous);
    void onSendClicked();
    void onClearHistoryClicked();
    void onHistorySelectionChanged();
    void onRawSendClicked();
    void onRawClearClicked();
    void onQueryAddressClicked();

    void onSessionStateChanged(bool connected);
    void onRequestStarted(const MasterQueryResult &result);
    void onResponseReceived(const MasterQueryResult &result);
    void onRequestFailed(const MasterQueryResult &result);
    void onNoticeGenerated(const QString &message);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void setupUi();
    QGroupBox *setupConnectionGroup();
    QGroupBox *setupMeterConfigGroup();
    QGroupBox *setupQueryGroup();
    QGroupBox *setupRawSendGroup();
    QGroupBox *setupResultGroup();
    QGroupBox *setupHistoryGroup();

    void requestPortRefresh();
    void refreshPorts();
    void applyPortList(const QList<QSerialPortInfo> &ports);
    void populateCategories();
    void populateDataItems();
    QList<QString> allCategories() const;
    QList<QString> dataCodesForCategory(const QString &category) const;
    QString selectedDiCode() const;
    QString selectedDiDescription() const;
    QString currentPortName() const;
    MasterSerialConfig currentSerialConfig() const;
    MasterRequestSpec currentRequestSpec() const;

    void applyConnectionUiState(bool connected);
    void updateQueryUiState();
    void displayResult(const MasterQueryResult &result);
    int appendHistoryRow(const MasterQueryResult &result);
    void updateHistoryRow(int row, const MasterQueryResult &result);
    QString requestTypeText(const MasterQueryResult &result) const;
    QString requestTargetText(const MasterQueryResult &result) const;
    QString requestSummaryText(const MasterQueryResult &result) const;
    QString statusText(MasterQueryStatus status) const;
    void applyTheme(const ui::ThemePalette &palette);

    MasterQueryController *m_controller;
    MeterModel *m_catalogMeterModel;
    QFutureWatcher<QList<QSerialPortInfo>> *m_portRefreshWatcher;
    bool m_darkTheme;
    bool m_portRefreshPending;
    int m_pendingHistoryRow;
    QStringList m_unavailablePorts;
    QList<QSerialPortInfo> m_cachedPorts;
    QVector<MasterQueryResult> m_historyResults;

    QComboBox *m_portCombo;
    QComboBox *m_baudRateCombo;
    QComboBox *m_dataBitsCombo;
    QComboBox *m_parityCombo;
    QPushButton *m_refreshPortsButton;
    QPushButton *m_connectButton;
    QPushButton *m_disconnectButton;
    QLabel *m_connectionStatusLabel;
    QLabel *m_portHintLabel;

    QLineEdit *m_meterAddressEdit;
    QLineEdit *m_meterPasswordEdit;
    QLineEdit *m_operatorCodeEdit;
    QPushButton *m_queryAddressButton;

    QComboBox *m_queryTypeCombo;
    QCheckBox *m_includePrefixCheckBox;
    QSpinBox *m_timeoutSpinBox;
    QLabel *m_writeValueLabel;
    QLineEdit *m_writeValueEdit;
    QLabel *m_remoteControlTimeLabel;
    QLineEdit *m_remoteControlTimeEdit;
    QLineEdit *m_searchEdit;
    QComboBox *m_categoryComboBox;
    QListWidget *m_dataItemList;
    QLabel *m_selectedDiLabel;
    QPushButton *m_sendButton;
    QTextEdit *m_rawHexEdit;
    QPushButton *m_rawSendButton;
    QPushButton *m_rawClearButton;

    QLabel *m_resultMetaLabel;
    QLabel *m_noticeLabel;
    QLineEdit *m_requestHexEdit;
    QLineEdit *m_responseHexEdit;
    FrameParseResultWidget *m_parseResultWidget;

    QTableWidget *m_historyTable;
    QPushButton *m_clearHistoryButton;
};

#endif // MASTERQUERYDIALOG_H
