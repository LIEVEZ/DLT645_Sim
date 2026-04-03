#ifndef NEWCONNECTIONDIALOG_H
#define NEWCONNECTIONDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QPushButton>
#include <QLabel>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStackedWidget>
#include <QGroupBox>
#include <QRegExpValidator>
#include "Models/ConnectionConfig.h"

class NewConnectionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit NewConnectionDialog(QWidget *parent = nullptr);
    ~NewConnectionDialog();

    ConnectionConfig getConnectionConfig() const;
    void setConnectionConfig(const ConnectionConfig &config);
    void setConnectedPorts(const QStringList &connectedPorts);
    void setEditMode(bool editing);

private slots:
    void onConnectionTypeChanged(int index);
    void onOkClicked();
    void onCancelClicked();
    void onRefreshPorts();

private:
    void setupUI();
    void setupSerialConfig();
    void setupTCPConfig();
    void updateUI();
    bool validateInput();

    QComboBox *m_connectionTypeCombo;
    QLineEdit *m_aliasEdit;
    QStackedWidget *m_configStack;

    QComboBox *m_serialPortCombo;
    QComboBox *m_baudRateCombo;
    QComboBox *m_dataBitsCombo;
    QComboBox *m_stopBitsCombo;
    QComboBox *m_parityCombo;
    QComboBox *m_flowControlCombo;
    QPushButton *m_refreshPortsButton;

    QLineEdit *m_tcpAddressEdit;
    QSpinBox *m_tcpPortSpinBox;

    QPushButton *m_okButton;
    QPushButton *m_cancelButton;

    ConnectionConfig m_config;
    QStringList m_connectedPorts;
};

#endif // NEWCONNECTIONDIALOG_H
