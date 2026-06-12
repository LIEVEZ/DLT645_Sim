#ifndef EVENTRECORDDIALOG_H
#define EVENTRECORDDIALOG_H

#include <QDialog>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QMap>
#include <QScrollArea>

class MeterModel;

/**
 * @brief 事件记录设置对话框
 *
 * 用于管理当前电表的事件记录存在状态
 */
class EventRecordDialog : public QDialog
{
    Q_OBJECT

public:
    explicit EventRecordDialog(MeterModel *meterModel, QWidget *parent = nullptr);
    ~EventRecordDialog();

    void applyDarkTheme();
    void applyLightTheme();

private slots:
    void onOkClicked();
    void onCancelClicked();
    void onGlobalCheckBoxToggled(bool checked);

private:
    void setupUI();
    void loadMeterSettings();

    MeterModel *m_meterModel;
    QLabel *m_infoLabel;
    QCheckBox *m_globalCheckBox;
    QMap<QString, QCheckBox*> m_eventCheckBoxes;
    QScrollArea *m_scrollArea;
    QPushButton *m_okButton;
    QPushButton *m_cancelButton;
};

#endif // EVENTRECORDDIALOG_H
