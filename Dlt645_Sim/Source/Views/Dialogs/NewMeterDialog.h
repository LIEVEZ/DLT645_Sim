#ifndef NEWMETERDIALOG_H
#define NEWMETERDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCheckBox>
#include <QComboBox>
#include <QRegExpValidator>
#include "Models/MeterPhaseType.h"

/**
 * @brief 电表配置结构
 */
struct MeterConfig
{
    QString alias;         // 电表别名
    QString address;       // 通讯地址（12位BCD码）
    QString operatorCode;  // 操作者代码（8位数字）
    QString password;      // 电能表密码（8位数字）
    bool includePrefixFE;  // 是否包含前导FE
    MeterPhaseType phaseType;

    MeterConfig()
        : alias("电表别名"), address("202101190552"), operatorCode("00000000"), password("00000002"), includePrefixFE(true), phaseType(MeterPhaseType::ThreePhase)
    {
    }
};

/**
 * @brief 新建电表对话框
 *
 * 用于配置电表参数
 */
class NewMeterDialog : public QDialog
{
    Q_OBJECT

public:
    explicit NewMeterDialog(QWidget *parent = nullptr);
    ~NewMeterDialog();

    /**
     * @brief 获取电表配置
     * @return 电表配置
     */
    MeterConfig getMeterConfig() const;

    /**
     * @brief 设置电表配置
     * @param config 电表配置
     */
    void setMeterConfig(const MeterConfig &config);

private slots:
    void onOkClicked();
    void onCancelClicked();
    void onAddressTextChanged(const QString &text);
    void onOperatorCodeTextChanged(const QString &text);
    void onPasswordTextChanged(const QString &text);

private:
    void setupUI();
    bool validateInput();

    // 电表别名
    QLineEdit *m_aliasEdit;

    // 通讯地址
    QLineEdit *m_addressEdit;
    QLabel *m_addressHintLabel;

    // 操作者代码
    QLineEdit *m_operatorCodeEdit;
    QLabel *m_operatorCodeHintLabel;

    // 电能表密码
    QLineEdit *m_passwordEdit;
    QLabel *m_passwordHintLabel;

    // 包含前导FE
    QCheckBox *m_includePrefixFECheckBox;
    QComboBox *m_phaseTypeComboBox;

    // 按钮
    QPushButton *m_okButton;
    QPushButton *m_cancelButton;

    MeterConfig m_config;
};

#endif // NEWMETERDIALOG_H
