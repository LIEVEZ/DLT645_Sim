#include "mainwindow.h"
#include "ViewModels/MainWindowViewModel.h"

#include "Utils/UiTheme.h"
#include "Views/Dialogs/CommunicationDialog.h"
#include "Views/Dialogs/DataItemManualDialog.h"
#include "Views/Dialogs/FrameParseDialog.h"
#include "Views/Dialogs/MasterQueryDialog.h"
#include "Views/Dialogs/MeterPasswordDialog.h"
#include "Views/Dialogs/QueryFrameDialog.h"
#include "Views/OperationLog/OperationLogWidget.h"

#include <QAction>
#include <QWidget>

namespace
{
// 主窗口专属的 QSS 模板（含菜单栏 / 工具栏 / 分隔条等仅主窗口出现的控件）。
// 占位符形如 {{token}}，由 ui::applyThemeTokens 用全局调色板替换。
// 调色板与设计原则统一定义在 Utils/UiTheme.{h,cpp}。
QString buildStyleSheet(const ui::ThemePalette &p)
{
    QString tpl = R"(
        QMainWindow {
            background-color: {{windowBg}};
        }

        QMainWindow, QMainWindow QWidget, QMenu, QToolTip {
            font-family: "Microsoft YaHei UI";
        }

        QMenuBar {
            background-color: {{panelBg}};
            color: {{text}};
            border-bottom: 1px solid {{border}};
        }

        QMenuBar::item {
            padding: 5px 15px;
        }

        QMenuBar::item:selected {
            background-color: {{accent}};
            color: {{onAccent}};
        }

        QToolBar#mainToolBar {
            background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                                              stop:0 {{toolbarTop}}, stop:1 {{toolbarBottom}});
            border: none;
            border-bottom: 1px solid {{toolbarEdge}};
            spacing: 8px;
            padding: 6px 10px;
        }

        QToolBar#mainToolBar::separator {
            width: 1px;
            margin: 8px 6px;
            background-color: {{tbSeparator}};
        }

        QToolBar#mainToolBar QToolButton {
            background-color: transparent;
            border: 1px solid transparent;
            border-radius: 8px;
            padding: 7px 14px;
            color: {{text}};
            font-size: 10pt;
            font-weight: 500;
            min-width: 98px;
            min-height: 22px;
        }

        QToolBar#mainToolBar QToolButton:hover {
            background-color: {{tbHover}};
            border: 1px solid {{tbHoverEdge}};
        }

        QToolBar#mainToolBar QToolButton:pressed,
        QToolBar#mainToolBar QToolButton:checked {
            background-color: {{tbPressed}};
            border: 1px solid {{tbPressedEdge}};
        }

        QToolBar#mainToolBar QToolButton:disabled {
            color: {{textMuted}};
        }

        QToolBar#mainToolBar QToolButton[popupMode="1"] {
            padding-right: 20px;
        }

        QSplitter::handle {
            background-color: {{borderStrong}};
        }

        QStatusBar {
            background-color: {{barBg}};
            color: {{text}};
        }

        QLabel {
            color: {{text}};
        }

        QFrame#meterInfoCard {
            background-color: {{panelBg}};
            border: 1px solid {{border}};
            border-radius: 7px;
        }

        QLabel#meterInfoHeading {
            color: {{accentTitle}};
            font-size: 13px;
            font-weight: 600;
        }

        QLabel#meterInfoCaption {
            color: {{textMuted}};
            font-size: 11px;
            font-weight: 500;
        }

        QLabel#meterInfoValue,
        QLabel#meterInfoAddress {
            color: {{text}};
            font-size: 12px;
            font-weight: 600;
        }

        QLabel#meterInfoAddress {
            padding: 3px 7px;
            background-color: {{windowBg}};
            border: 1px solid {{border}};
            border-radius: 4px;
        }

        QLabel#meterInfoAddress::selection {
            background-color: {{accent}};
            color: {{onAccent}};
        }

        QFrame#meterInfoSeparator {
            color: {{border}};
            background-color: {{border}};
            min-width: 1px;
            max-width: 1px;
        }

        /* 数据表格上方「格式类型提示」卡片：柔和提示底色 + 圆角边框 */
        QFrame#formatHintCard {
            background-color: {{noticeBg}};
            border: 1px solid {{noticeBorder}};
            border-radius: 8px;
        }

        QLabel#formatHintTitle {
            color: {{accentTitle}};
            font-size: 12px;
            font-weight: 600;
            background: transparent;
        }

        QLabel#formatHintBody {
            color: {{noticeText}};
            font-size: 12px;
            background: transparent;
        }

        QTreeWidget {
            background-color: {{windowBg}};
            color: {{text}};
            border: none;
            outline: none;
        }

        QTreeWidget::item {
            padding: 3px;
            border-bottom: 1px solid {{gridLine}};
        }

        QTreeWidget::item:selected {
            background-color: {{accent}};
            color: {{onAccent}};
        }

        QTreeWidget::item:hover {
            background-color: {{panelBg}};
        }

        /* 选中项悬停时保持强调色（特异性高于 :hover，避免被灰色覆盖） */
        QTreeWidget::item:selected:hover {
            background-color: {{accentHover}};
            color: {{onAccent}};
        }

        QTableWidget {
            background-color: {{windowBg}};
            color: {{text}};
            border: none;
            gridline-color: {{gridLine}};
        }

        QTableWidget::item {
            padding: 5px;
            border-bottom: 1px solid {{gridLine}};
        }

        QTableWidget::item:selected {
            background-color: {{accent}};
            color: {{onAccent}};
        }

        QHeaderView::section {
            background-color: {{panelBg}};
            color: {{text}};
            padding: 5px;
            border: none;
            border-bottom: 1px solid {{border}};
        }

        QLineEdit {
            background-color: {{panelBg}};
            color: {{text}};
            border: 1px solid {{border}};
            border-radius: 4px;
            padding: 5px;
        }

        QLineEdit#searchBox {
            padding-top: 4px;
            padding-right: 8px;
            padding-bottom: 4px;
        }

        QLineEdit:focus {
            border: 1px solid {{accent}};
        }

        {{buttonBlock}}

        QComboBox {
            background-color: {{panelBg}};
            color: {{text}};
            border: 1px solid {{border}};
            border-radius: 4px;
            padding: 5px;
        }

        QComboBox::drop-down {
            border: none;
        }

        QComboBox QAbstractItemView {
            background-color: {{panelBg}};
            color: {{text}};
            selection-background-color: {{accent}};
        }

        QListWidget {
            background-color: {{windowBg}};
            color: {{text}};
            border: none;
            outline: none;
        }

        QListWidget::item {
            padding: 3px;
            border-bottom: 1px solid {{gridLine}};
        }

        QListWidget#operationLogListWidget::item {
            padding: 2px 4px;
        }

        QListWidget::item:selected {
            background-color: {{accent}};
            color: {{onAccent}};
        }

        QListWidget::item:hover {
            background-color: {{panelBg}};
        }

        /* 选中项悬停时保持强调色（特异性高于 :hover，避免被灰色覆盖） */
        QListWidget::item:selected:hover {
            background-color: {{accentHover}};
            color: {{onAccent}};
        }

        QCheckBox {
            color: {{text}};
            spacing: 4px;
        }

        QCheckBox::indicator {
            width: 16px;
            height: 16px;
            border: 1px solid {{border}};
            border-radius: 4px;
            background-color: {{panelBg}};
        }

        QCheckBox::indicator:checked {
            background-color: {{accent}};
            border: 1px solid {{accent}};
        }

        QSpinBox {
            background-color: {{panelBg}};
            color: {{text}};
            border: 1px solid {{border}};
            border-radius: 4px;
            padding: 3px;
        }

        QSpinBox:focus {
            border: 1px solid {{accent}};
        }

        QSpinBox::up-button, QSpinBox::down-button {
            background-color: {{panelHover}};
            border: none;
            width: 16px;
        }

        QSpinBox::up-button:hover, QSpinBox::down-button:hover {
            background-color: {{panelPressed}};
        }

        QGroupBox {
            border: 1px solid {{border}};
            border-radius: 8px;
            margin-top: 10px;
            padding-top: 10px;
            font-weight: bold;
            color: {{text}};
        }

        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 5px;
            color: {{accentTitle}};
        }

        QTabWidget::pane {
            border: 1px solid {{border}};
            background-color: {{windowBg}};
        }

        QTabBar::tab {
            background-color: {{panelBg}};
            color: {{text}};
            padding: 8px 20px;
            border-top-left-radius: 4px;
            border-top-right-radius: 4px;
        }

        QTabBar::tab:selected {
            background-color: {{accent}};
            color: {{onAccent}};
        }

        QDialog {
            background-color: {{windowBg}};
        }
    )";

    return ui::applyThemeTokens(tpl, p);
}

// 同步三栏容器与通信条的底色（这些控件不受 setStyleSheet 的选择器覆盖）。
void applyContainerColors(MainWindow *win, const ui::ThemePalette &p)
{
    if (QWidget *commBar = win->findChild<QWidget *>("communicationBarWidget"))
        commBar->setStyleSheet(QString("background-color: %1;").arg(p.barBg));

    const char *containers[] = {"leftContainer", "centerContainer", "rightContainer"};
    for (const char *name : containers)
    {
        if (QWidget *cont = win->findChild<QWidget *>(name))
            cont->setStyleSheet(QString("background-color: %1;").arg(p.windowBg));
    }
}
} // namespace

void MainWindow::applyDarkTheme()
{
    const ui::ThemePalette p = ui::darkPalette();
    setStyleSheet(buildStyleSheet(p));
    applyContainerColors(this, p);
    if (m_operationLogWidget)
        m_operationLogWidget->applyDarkTheme();
}

void MainWindow::applyLightTheme()
{
    const ui::ThemePalette p = ui::lightPalette();
    setStyleSheet(buildStyleSheet(p));
    applyContainerColors(this, p);
    if (m_operationLogWidget)
        m_operationLogWidget->applyLightTheme();
}

void MainWindow::onToggleDarkTheme(bool enabled)
{
    if (enabled)
    {
        m_viewModel->setDarkTheme(true);
        m_darkThemeAction->setChecked(true);
        m_lightThemeAction->setChecked(false);
        applyDarkTheme();

        // 更新通信相关窗口主题
        if (m_communicationDialog)
        {
            m_communicationDialog->applyDarkTheme();
        }
        if (m_queryFrameDialog)
        {
            m_queryFrameDialog->applyDarkTheme();
        }
        if (m_masterQueryDialog)
        {
            m_masterQueryDialog->applyDarkTheme();
        }
        if (m_frameParseDialog)
        {
            m_frameParseDialog->applyDarkTheme();
        }
        if (m_dataItemManualDialog)
        {
            m_dataItemManualDialog->applyDarkTheme();
        }
        if (m_meterPasswordDialog)
        {
            m_meterPasswordDialog->applyDarkTheme();
        }
    }
}

void MainWindow::onToggleLightTheme(bool enabled)
{
    if (enabled)
    {
        m_viewModel->setDarkTheme(false);
        m_darkThemeAction->setChecked(false);
        m_lightThemeAction->setChecked(true);
        applyLightTheme();

        // 更新通信相关窗口主题
        if (m_communicationDialog)
        {
            m_communicationDialog->applyLightTheme();
        }
        if (m_queryFrameDialog)
        {
            m_queryFrameDialog->applyLightTheme();
        }
        if (m_masterQueryDialog)
        {
            m_masterQueryDialog->applyLightTheme();
        }
        if (m_frameParseDialog)
        {
            m_frameParseDialog->applyLightTheme();
        }
        if (m_dataItemManualDialog)
        {
            m_dataItemManualDialog->applyLightTheme();
        }
        if (m_meterPasswordDialog)
        {
            m_meterPasswordDialog->applyLightTheme();
        }
    }
}
