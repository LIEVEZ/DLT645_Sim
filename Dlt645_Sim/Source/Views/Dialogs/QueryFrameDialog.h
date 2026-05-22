#ifndef QUERYFRAMEDIALOG_H
#define QUERYFRAMEDIALOG_H

#include <QDialog>
#include <QList>

class QCheckBox;
class QCloseEvent;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class MeterModel;
class QPushButton;
class QGroupBox;
class FrameParseDialog;
struct DataItem;

class QueryFrameDialog : public QDialog
{
    Q_OBJECT

public:
    explicit QueryFrameDialog(QWidget *parent = nullptr);
    ~QueryFrameDialog();

    void setMeterModel(MeterModel *meterModel);
    void setDefaultMeterAddress(const QString &address);
    void setIncludePrefixFE(bool include);
    void applyDarkTheme();
    void applyLightTheme();

signals:
    void dialogClosed();

private slots:
    void onModeChanged(int index);
    void onCategoryChanged(int index);
    void onSearchTextChanged(const QString &text);
    void onCurrentItemChanged(QListWidgetItem *current, QListWidgetItem *previous);
    void onInputChanged();
    void onCopySpacedClicked();
    void onCopyCompactClicked();
    void onParseFrameClicked();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    enum class FrameOperationMode
    {
        ReadData,
        WriteData
    };

    void setupUI();
    void populateCategories();
    void populateDataItems();
    void refreshGeneratedFrame();
    void refreshWriteValueState();
    void updateActionButtons();
    QList<QString> allCategories() const;
    QList<QString> dataCodesForCategory(const QString &category) const;
    const MeterModel *dataSourceModel() const;
    FrameOperationMode currentOperationMode() const;
    bool isWriteMode() const;
    bool supportsCurrentMode(const DataItem &item) const;
    QString selectedDiCode() const;
    QString selectedDiDescription() const;
    QString selectedDiDefaultValue() const;
    void showFrameParseDialog(const QString &frameHex);

    MeterModel *m_meterModel;
    MeterModel *m_catalogMeterModel;
    FrameParseDialog *m_frameParseDialog;
    bool m_isDarkTheme;

    QLineEdit *m_addressEdit;
    QCheckBox *m_includePrefixCheckBox;
    QComboBox *m_modeComboBox;
    QComboBox *m_categoryComboBox;
    QLineEdit *m_searchEdit;
    QListWidget *m_dataItemList;
    QLabel *m_selectedDiLabel;
    QGroupBox *m_writeGroup;
    QLineEdit *m_writePermissionEdit;
    QLineEdit *m_writePasswordEdit;
    QLineEdit *m_writeOperatorCodeEdit;
    QLabel *m_writeValueLabel;
    QLineEdit *m_writeValueEdit;
    QLabel *m_writeHintLabel;
    QLineEdit *m_spacedHexEdit;
    QLineEdit *m_compactHexEdit;
    QPushButton *m_copySpacedButton;
    QPushButton *m_copyCompactButton;
    QPushButton *m_parseFrameButton;
    QLabel *m_statusLabel;
};

#endif // QUERYFRAMEDIALOG_H
