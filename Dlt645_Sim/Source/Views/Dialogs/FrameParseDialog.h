#ifndef FRAMEPARSEDIALOG_H
#define FRAMEPARSEDIALOG_H

#include <QDialog>

class QLabel;
class MeterModel;
class QPushButton;
class QTextEdit;
class QGroupBox;
class FrameParseResultWidget;

class FrameParseDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FrameParseDialog(QWidget *parent = nullptr);
    ~FrameParseDialog();

    void setMeterModel(MeterModel *meterModel);
    void setFrameHex(const QString &frameHex);
    void applyDarkTheme();
    void applyLightTheme();

private slots:
    void onSendInputTextChanged();
    void onReceiveInputTextChanged();
    void onSendClearClicked();
    void onReceiveClearClicked();

private:
    void setupUI();
    QGroupBox *createParsePanel(const QString &title,
                                QTextEdit **inputEdit,
                                FrameParseResultWidget **resultWidget,
                                QLabel **statusLabel,
                                QPushButton **clearButton);
    bool parseInput(QTextEdit *inputEdit,
                    QLabel *statusLabel,
                    FrameParseResultWidget *resultWidget);
    void clearPanel(QTextEdit *inputEdit,
                    QLabel *statusLabel,
                    FrameParseResultWidget *resultWidget);
    QString normalizedHexInput(const QTextEdit *inputEdit, QString *errorMessage) const;
    const MeterModel *activeMeterModel() const;

    MeterModel *m_meterModel;
    MeterModel *m_catalogMeterModel;
    QTextEdit *m_sendInputEdit;
    QTextEdit *m_receiveInputEdit;
    FrameParseResultWidget *m_sendResultWidget;
    FrameParseResultWidget *m_receiveResultWidget;
    QLabel *m_sendStatusLabel;
    QLabel *m_receiveStatusLabel;
    QPushButton *m_sendClearButton;
    QPushButton *m_receiveClearButton;
};

#endif // FRAMEPARSEDIALOG_H
