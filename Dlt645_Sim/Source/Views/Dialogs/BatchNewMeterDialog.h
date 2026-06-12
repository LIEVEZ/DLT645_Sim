#ifndef BATCHNEWMETERDIALOG_H
#define BATCHNEWMETERDIALOG_H

#include <QDialog>
#include <QString>

#include "Models/MeterPhaseType.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;

struct BatchMeterConfig
{
    QString aliasPrefix;
    QString startAddress;
    int count;
    int aliasStartNumber;
    QString operatorCode;
    QString password;
    bool includePrefixFE;
    bool eventRecordPresent;
    MeterPhaseType phaseType;

    BatchMeterConfig()
        : aliasPrefix(QStringLiteral("电表别名")),
          startAddress(QStringLiteral("202101190552")),
          count(10),
          aliasStartNumber(1),
          operatorCode(QStringLiteral("00000000")),
          password(QStringLiteral("00000002")),
          includePrefixFE(true),
          eventRecordPresent(true),
          phaseType(MeterPhaseType::ThreePhase)
    {
    }
};

class BatchNewMeterDialog : public QDialog
{
    Q_OBJECT

public:
    explicit BatchNewMeterDialog(QWidget *parent = nullptr);

    BatchMeterConfig getBatchMeterConfig() const;
    void setBatchMeterConfig(const BatchMeterConfig &config);

private slots:
    void onOkClicked();
    void updateHintsAndPreview();

private:
    void setupUI();
    bool validateInput();
    QString addressAt(int offset) const;
    QString aliasAt(int offset) const;

    QLineEdit *m_aliasPrefixEdit;
    QSpinBox *m_aliasStartNumberSpinBox;
    QLineEdit *m_startAddressEdit;
    QLabel *m_startAddressHintLabel;
    QSpinBox *m_countSpinBox;
    QLineEdit *m_operatorCodeEdit;
    QLabel *m_operatorCodeHintLabel;
    QLineEdit *m_passwordEdit;
    QLabel *m_passwordHintLabel;
    QComboBox *m_phaseTypeComboBox;
    QCheckBox *m_includePrefixFECheckBox;
    QLabel *m_previewLabel;
    QPushButton *m_okButton;
    QPushButton *m_cancelButton;

    BatchMeterConfig m_config;
};

#endif // BATCHNEWMETERDIALOG_H
