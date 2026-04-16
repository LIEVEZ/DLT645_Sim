#ifndef FRAMEPARSEDIALOG_H
#define FRAMEPARSEDIALOG_H

#include <QDialog>

class QLabel;
class MeterModel;
class QPushButton;
class QTextEdit;
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
    void onInputTextChanged();
    void onClearClicked();

private:
    void setupUI();
    bool parseCurrentInput();
    QString normalizedHexInput(QString *errorMessage) const;
    const MeterModel *activeMeterModel() const;

    MeterModel *m_meterModel;
    MeterModel *m_catalogMeterModel;
    QTextEdit *m_inputEdit;
    FrameParseResultWidget *m_resultWidget;
    QLabel *m_statusLabel;
    QPushButton *m_clearButton;
};

#endif // FRAMEPARSEDIALOG_H
