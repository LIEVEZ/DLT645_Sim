#include "mainwindow.h"
#include "ViewModels/MainWindowViewModel.h"

#include "Views/Dialogs/CommunicationDialog.h"
#include "Views/Dialogs/DataItemManualDialog.h"
#include "Views/Dialogs/FrameParseDialog.h"
#include "Views/Dialogs/MasterQueryDialog.h"
#include "Views/Dialogs/QueryFrameDialog.h"

#include <QAction>
#include <QWidget>

void MainWindow::applyDarkTheme()
{
    QString styleSheet = R"(
        QMainWindow {
            background-color: #2B2B2B;
        }
        
        QMenuBar {
            background-color: #3C3F41;
            color: #FFFFFF;
            border-bottom: 1px solid #555555;
        }
        
        QMenuBar::item {
            padding: 5px 15px;
        }
        
        QMenuBar::item:selected {
            background-color: #094771;
        }
        
        QToolBar#mainToolBar {
            background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                                              stop:0 #565A5D, stop:1 #4A4E51);
            border: none;
            border-bottom: 1px solid #383B3D;
            spacing: 8px;
            padding: 6px 10px;
        }

        QToolBar#mainToolBar::separator {
            width: 1px;
            margin: 8px 6px;
            background-color: rgba(255, 255, 255, 0.18);
        }
        
        QToolBar#mainToolBar QToolButton {
            background-color: transparent;
            border: 1px solid transparent;
            border-radius: 8px;
            padding: 7px 14px;
            color: #FFFFFF;
            font-size: 10pt;
            font-weight: 500;
            min-width: 98px;
            min-height: 22px;
        }
        
        QToolBar#mainToolBar QToolButton:hover {
            background-color: rgba(255, 255, 255, 0.08);
            border: 1px solid rgba(255, 255, 255, 0.18);
        }
        
        QToolBar#mainToolBar QToolButton:pressed,
        QToolBar#mainToolBar QToolButton:checked {
            background-color: rgba(9, 71, 113, 0.72);
            border: 1px solid rgba(110, 178, 226, 0.55);
        }

        QToolBar#mainToolBar QToolButton:disabled {
            color: rgba(255, 255, 255, 0.42);
        }
        
        QToolBar#mainToolBar QToolButton[popupMode="1"] {
            padding-right: 20px;
        }
        
        QSplitter::handle {
            background-color: #555555;
        }
        
        QStatusBar {
            background-color: #4E5254;
            color: #FFFFFF;
        }
        
        QLabel {
            color: #FFFFFF;
            font-family: "Microsoft YaHei";
        }
        
        QTreeWidget {
            background-color: #2B2B2B;
            color: #FFFFFF;
            border: none;
            outline: none;
        }
        
        QTreeWidget::item {
            padding: 3px;
            border-bottom: 1px solid #3C3F41;
        }
        
        QTreeWidget::item:selected {
            background-color: #094771;
            color: #FFFFFF;
        }
        
        QTreeWidget::item:hover {
            background-color: #3C3F41;
        }
        
        QTableWidget {
            background-color: #2B2B2B;
            color: #FFFFFF;
            border: none;
            gridline-color: #3C3F41;
        }
        
        QTableWidget::item {
            padding: 5px;
            border-bottom: 1px solid #3C3F41;
        }
        
        QTableWidget::item:selected {
            background-color: #094771;
            color: #FFFFFF;
        }
        
        QHeaderView::section {
            background-color: #3C3F41;
            color: #FFFFFF;
            padding: 5px;
            border: none;
            border-bottom: 1px solid #555555;
        }
        
        QLineEdit {
            background-color: #3C3F41;
            color: #FFFFFF;
            border: 1px solid #555555;
            border-radius: 3px;
            padding: 5px;
        }

        QLineEdit#searchBox {
            padding-top: 4px;
            padding-right: 8px;
            padding-bottom: 4px;
        }
        
        QLineEdit:focus {
            border: 1px solid #094771;
        }
        
        QPushButton {
            background-color: #3C3F41;
            color: #FFFFFF;
            border: 1px solid #555555;
            border-radius: 3px;
            padding: 5px 15px;
        }
        
        QPushButton:hover {
            background-color: #4E5254;
        }
        
        QPushButton:pressed {
            background-color: #2B2B2B;
        }
        
        QComboBox {
            background-color: #3C3F41;
            color: #FFFFFF;
            border: 1px solid #555555;
            border-radius: 3px;
            padding: 5px;
        }
        
        QComboBox::drop-down {
            border: none;
        }
        
        QComboBox QAbstractItemView {
            background-color: #3C3F41;
            color: #FFFFFF;
            selection-background-color: #094771;
        }
        
        QListWidget {
            background-color: #2B2B2B;
            color: #FFFFFF;
            border: none;
            outline: none;
        }
        
        QListWidget::item {
            padding: 3px;
            border-bottom: 1px solid #3C3F41;
        }
        
        QListWidget::item:selected {
            background-color: #094771;
            color: #FFFFFF;
        }
        
        QListWidget::item:hover {
            background-color: #3C3F41;
        }
        
        QCheckBox {
            color: #FFFFFF;
            spacing: 4px;
        }
        
        QCheckBox::indicator {
            width: 16px;
            height: 16px;
            border: 1px solid #555555;
            border-radius: 3px;
            background-color: #3C3F41;
        }
        
        QCheckBox::indicator:checked {
            background-color: #094771;
            border: 1px solid #094771;
        }
        
        QSpinBox {
            background-color: #3C3F41;
            color: #FFFFFF;
            border: 1px solid #555555;
            border-radius: 3px;
            padding: 3px;
        }
        
        QSpinBox:focus {
            border: 1px solid #094771;
        }
        
        QSpinBox::up-button, QSpinBox::down-button {
            background-color: #4E5254;
            border: none;
            width: 16px;
        }
        
        QSpinBox::up-button:hover, QSpinBox::down-button:hover {
            background-color: #5A5E60;
        }
        
        QGroupBox {
            border: 1px solid #555555;
            border-radius: 5px;
            margin-top: 10px;
            padding-top: 10px;
            font-weight: bold;
            color: #FFFFFF;
        }
        
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 5px;
            color: #4EC9B0;
        }
        
        QTabWidget::pane {
            border: 1px solid #555555;
            background-color: #2B2B2B;
        }
        
        QTabBar::tab {
            background-color: #3C3F41;
            color: #D4D4D4;
            padding: 8px 20px;
            border-top-left-radius: 3px;
            border-top-right-radius: 3px;
        }
        
        QTabBar::tab:selected {
            background-color: #094771;
            color: #FFFFFF;
        }
        
        QDialog {
            background-color: #2B2B2B;
        }
    )";

    setStyleSheet(styleSheet);

    // 鏇存柊瀹瑰櫒鑳屾櫙鑹?
    QWidget *commBar = findChild<QWidget *>("communicationBarWidget");
    if (commBar)
        commBar->setStyleSheet("background-color: #4E5254;");

    QWidget *leftCont = findChild<QWidget *>("leftContainer");
    if (leftCont)
        leftCont->setStyleSheet("background-color: #2B2B2B;");

    QWidget *centerCont = findChild<QWidget *>("centerContainer");
    if (centerCont)
        centerCont->setStyleSheet("background-color: #2B2B2B;");

    QWidget *rightCont = findChild<QWidget *>("rightContainer");
    if (rightCont)
        rightCont->setStyleSheet("background-color: #2B2B2B;");
}

void MainWindow::applyLightTheme()
{
    QString styleSheet = R"(
        QMainWindow {
            background-color: #FFFFFF;
        }
        
        QMenuBar {
            background-color: #F0F0F0;
            color: #000000;
            border-bottom: 1px solid #CCCCCC;
        }
        
        QMenuBar::item {
            padding: 5px 15px;
        }
        
        QMenuBar::item:selected {
            background-color: #E0E0E0;
        }
        
        QToolBar#mainToolBar {
            background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                                              stop:0 #FAFAFA, stop:1 #F1F3F5);
            border: none;
            border-bottom: 1px solid #DADDE1;
            spacing: 8px;
            padding: 6px 10px;
        }

        QToolBar#mainToolBar::separator {
            width: 1px;
            margin: 8px 6px;
            background-color: rgba(0, 0, 0, 0.14);
        }
        
        QToolBar#mainToolBar QToolButton {
            background-color: transparent;
            border: 1px solid transparent;
            border-radius: 8px;
            padding: 7px 14px;
            color: #000000;
            font-size: 10pt;
            font-weight: 500;
            min-width: 98px;
            min-height: 22px;
        }
        
        QToolBar#mainToolBar QToolButton:hover {
            background-color: rgba(0, 0, 0, 0.05);
            border: 1px solid rgba(0, 0, 0, 0.12);
        }
        
        QToolBar#mainToolBar QToolButton:pressed,
        QToolBar#mainToolBar QToolButton:checked {
            background-color: rgba(0, 120, 215, 0.12);
            border: 1px solid rgba(0, 120, 215, 0.32);
        }

        QToolBar#mainToolBar QToolButton:disabled {
            color: rgba(0, 0, 0, 0.34);
        }
        
        QToolBar#mainToolBar QToolButton[popupMode="1"] {
            padding-right: 20px;
        }
        
        QSplitter::handle {
            background-color: #CCCCCC;
        }
        
        QStatusBar {
            background-color: #F5F5F5;
            color: #000000;
        }
        
        QLabel {
            color: #000000;
            font-family: "Microsoft YaHei";
        }
        
        QTreeWidget {
            background-color: #FFFFFF;
            color: #000000;
            border: none;
            outline: none;
        }
        
        QTreeWidget::item {
            padding: 3px;
            border-bottom: 1px solid #E0E0E0;
        }
        
        QTreeWidget::item:selected {
            background-color: #0078D7;
            color: #FFFFFF;
        }
        
        QTreeWidget::item:hover {
            background-color: #F0F0F0;
        }
        
        QTableWidget {
            background-color: #FFFFFF;
            color: #000000;
            border: none;
            gridline-color: #E0E0E0;
        }
        
        QTableWidget::item {
            padding: 5px;
            border-bottom: 1px solid #E0E0E0;
        }
        
        QTableWidget::item:selected {
            background-color: #0078D7;
            color: #FFFFFF;
        }
        
        QHeaderView::section {
            background-color: #F0F0F0;
            color: #000000;
            padding: 5px;
            border: none;
            border-bottom: 1px solid #CCCCCC;
        }
        
        QLineEdit {
            background-color: #FFFFFF;
            color: #000000;
            border: 1px solid #CCCCCC;
            border-radius: 3px;
            padding: 5px;
        }

        QLineEdit#searchBox {
            padding-top: 4px;
            padding-right: 8px;
            padding-bottom: 4px;
        }
        
        QLineEdit:focus {
            border: 1px solid #0078D7;
        }
        
        QPushButton {
            background-color: #F0F0F0;
            color: #000000;
            border: 1px solid #CCCCCC;
            border-radius: 3px;
            padding: 5px 15px;
        }
        
        QPushButton:hover {
            background-color: #E0E0E0;
        }
        
        QPushButton:pressed {
            background-color: #D0D0D0;
        }
        
        QComboBox {
            background-color: #FFFFFF;
            color: #000000;
            border: 1px solid #CCCCCC;
            border-radius: 3px;
            padding: 5px;
        }
        
        QComboBox::drop-down {
            border: none;
        }
        
        QComboBox QAbstractItemView {
            background-color: #FFFFFF;
            color: #000000;
            selection-background-color: #0078D7;
        }
        
        QListWidget {
            background-color: #FFFFFF;
            color: #000000;
            border: none;
            outline: none;
        }
        
        QListWidget::item {
            padding: 3px;
            border-bottom: 1px solid #E0E0E0;
        }
        
        QListWidget::item:selected {
            background-color: #0078D7;
            color: #FFFFFF;
        }
        
        QListWidget::item:hover {
            background-color: #F0F0F0;
        }
        
        QCheckBox {
            color: #000000;
            spacing: 4px;
        }
        
        QCheckBox::indicator {
            width: 16px;
            height: 16px;
            border: 1px solid #CCCCCC;
            border-radius: 3px;
            background-color: #FFFFFF;
        }
        
        QCheckBox::indicator:checked {
            background-color: #0078D7;
            border: 1px solid #0078D7;
        }
        
        QSpinBox {
            background-color: #FFFFFF;
            color: #000000;
            border: 1px solid #CCCCCC;
            border-radius: 3px;
            padding: 3px;
        }
        
        QSpinBox:focus {
            border: 1px solid #0078D7;
        }
        
        QSpinBox::up-button, QSpinBox::down-button {
            background-color: #F0F0F0;
            border: none;
            width: 16px;
        }
        
        QSpinBox::up-button:hover, QSpinBox::down-button:hover {
            background-color: #E0E0E0;
        }
        
        QGroupBox {
            border: 1px solid #CCCCCC;
            border-radius: 5px;
            margin-top: 10px;
            padding-top: 10px;
            font-weight: bold;
            color: #000000;
        }
        
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 5px;
            color: #0078D7;
        }
        
        QTabWidget::pane {
            border: 1px solid #CCCCCC;
            background-color: #FFFFFF;
        }
        
        QTabBar::tab {
            background-color: #F0F0F0;
            color: #000000;
            padding: 8px 20px;
            border-top-left-radius: 3px;
            border-top-right-radius: 3px;
        }
        
        QTabBar::tab:selected {
            background-color: #0078D7;
            color: #FFFFFF;
        }
        
        QDialog {
            background-color: #FFFFFF;
        }
    )";

    setStyleSheet(styleSheet);

    // 鏇存柊瀹瑰櫒鑳屾櫙鑹?
    QWidget *commBar = findChild<QWidget *>("communicationBarWidget");
    if (commBar)
        commBar->setStyleSheet("background-color: #F5F5F5;");

    QWidget *leftCont = findChild<QWidget *>("leftContainer");
    if (leftCont)
        leftCont->setStyleSheet("background-color: #FFFFFF;");

    QWidget *centerCont = findChild<QWidget *>("centerContainer");
    if (centerCont)
        centerCont->setStyleSheet("background-color: #FFFFFF;");

    QWidget *rightCont = findChild<QWidget *>("rightContainer");
    if (rightCont)
        rightCont->setStyleSheet("background-color: #FFFFFF;");
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
    }
}