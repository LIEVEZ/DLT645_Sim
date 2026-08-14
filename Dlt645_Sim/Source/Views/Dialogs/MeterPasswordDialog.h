#ifndef METERPASSWORDDIALOG_H
#define METERPASSWORDDIALOG_H

#include <QDialog>

namespace ui
{
struct ThemePalette;
}

class QCloseEvent;
class QDateTimeEdit;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QRadioButton;

class MeterPasswordDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MeterPasswordDialog(QWidget *parent = nullptr);
    ~MeterPasswordDialog();

    void setMeterAddress(const QString &meterAddress);
    void applyDarkTheme();
    void applyLightTheme();

signals:
    void dialogClosed();

private slots:
    void onGenerateClicked();
    void onCopyPasswordClicked();
    void onAuthenticationGenerateClicked();
    void onMeterAuthenticationGenerateClicked();
    void clearGeneratedPassword();
    void clearAuthenticationFrame();
    void clearMeterAuthenticationFrame();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void setupUi();
    void applyTheme(const ui::ThemePalette &palette);

    QLineEdit *m_meterAddressEdit;
    QLineEdit *m_keyEdit;
    QLineEdit *m_passwordEdit;
    QRadioButton *m_enableAuthenticationButton;
    QRadioButton *m_disableAuthenticationButton;
    QLineEdit *m_authenticationFrameEdit;
    QLineEdit *m_authenticationCompactFrameEdit;
    QLineEdit *m_meterAuthenticationPasswordEdit;
    QLineEdit *m_operatorCodeEdit;
    QDateTimeEdit *m_deadlineEdit;
    QDateTimeEdit *m_baselineTimeEdit;
    QPlainTextEdit *m_meterAuthenticationFrameEdit;
    QPlainTextEdit *m_meterAuthenticationCompactFrameEdit;
    QLabel *m_statusLabel;
    QPushButton *m_generateButton;
    QPushButton *m_copyPasswordButton;
    QPushButton *m_generateAuthenticationFrameButton;
    QPushButton *m_generateMeterAuthenticationButton;
};

#endif // METERPASSWORDDIALOG_H
