QT       += core gui serialport network sql

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

INCLUDEPATH += $$PWD/Source
DEPENDPATH += $$PWD/Source

win32-msvc*:QMAKE_CXXFLAGS += /utf-8
win32-g++ {
    QMAKE_CXXFLAGS += -finput-charset=UTF-8 -fexec-charset=UTF-8
    QMAKE_CFLAGS += -finput-charset=UTF-8 -fexec-charset=UTF-8
}

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

# 源文件
SOURCES += \
    Source/main.cpp \
    Source/Views/MainWindow/mainwindow.cpp \
    Source/Views/MainWindow/mainwindow_theme.cpp \
    Source/Views/MainWindow/mainwindow_setup.cpp \
    Source/Views/MainWindow/mainwindow_dialogs.cpp \
    Source/Views/MainWindow/mainwindow_configuration.cpp \
    Source/Views/MainWindow/mainwindow_connections.cpp \
    Source/Views/MainWindow/mainwindow_meters.cpp \
    Source/Views/MainWindow/mainwindow_protocol.cpp \
    Source/Views/MainWindow/mainwindow_status.cpp \
    Source/Views/MainWindow/mainwindow_helpers.cpp \
    Source/Utils/UiTheme.cpp \
    Source/Utils/BCDHelper.cpp \
    Source/Utils/DLT645FrameFormatter.cpp \
    Source/Utils/DLT645BlockHandler.cpp \
    Source/Utils/DLT645ValueFormatter.cpp \
    Source/Utils/DLT645DataHelper.cpp \
    Source/Utils/ChecksumHelper.cpp \
    Source/Utils/Logger.cpp \
    Source/Models/DataItem.cpp \
    Source/Models/ConnectionInfoMapper.cpp \
    Source/Models/CategoryCatalog.cpp \
    Source/Models/MeterModelAccess.cpp \
    Source/Models/MeterModel.cpp \
    Source/Models/MeterModelInitialization.cpp \
    Source/Models/MeterModelInitializationEnergy.cpp \
    Source/Models/MeterModelInitializationVariable.cpp \
    Source/Models/MeterModelInitializationEvent.cpp \
    Source/Models/MeterModelInitializationEventPhase.cpp \
    Source/Models/MeterModelInitializationEventStats.cpp \
    Source/Models/MeterModelInitializationEventProgramming.cpp \
    Source/Models/MeterModelInitializationParameter.cpp \
    Source/Models/MeterModelInitializationFreeze.cpp \
    Source/Models/MeterModelQueries.cpp \
    Source/ViewModels/MainWindowViewModel.cpp \
    Source/Services/ProtocolParser.cpp \
    Source/Services/MeterDataStore.cpp \
    Source/Services/ConfigurationSerializer.cpp \
    Source/Services/TermCatalogService.cpp \
    Source/Controllers/ConnectionManager.cpp \
    Source/Controllers/SerialController.cpp \
    Source/Controllers/TcpController.cpp \
    Source/Controllers/MasterQueryController.cpp \
    Source/Controllers/ProtocolController.cpp \
    Source/Views/OperationLog/OperationLogWidget.cpp \
    Source/Views/DeviceTree/DeviceTreeWidget.cpp \
    Source/Views/DataTable/DataTableWidget.cpp \
    Source/Views/DataTable/DataTableExporter.cpp \
    Source/Views/Common/FrameParseResultWidget.cpp \
    Source/Views/Common/TermExplanationWidget.cpp \
    Source/Views/CommunicationBar/CommunicationBarWidget.cpp \
    Source/Views/Dialogs/NewConnectionDialog.cpp \
    Source/Views/Dialogs/NewMeterDialog.cpp \
    Source/Views/Dialogs/CommunicationDialog.cpp \
    Source/Views/Dialogs/BatchNewMeterDialog.cpp \
    Source/Views/Dialogs/EventRecordDialog.cpp \
    Source/Views/Dialogs/MasterQueryDialog.cpp \
    Source/Views/Dialogs/FrameParseDialog.cpp \
    Source/Views/Dialogs/DataItemManualDialog.cpp \
    Source/Views/Dialogs/QueryFrameDialog.cpp

# 头文件
HEADERS += \
    Source/Version.h \
    Source/Views/MainWindow/mainwindow.h \
    Source/Utils/BCDHelper.h \
    Source/Utils/DLT645FrameFormatter.h \
    Source/Utils/DLT645BlockHandler.h \
    Source/Utils/DLT645ValueFormatter.h \
    Source/Utils/DLT645FrameFormatterFields.h \
    Source/Utils/DLT645FrameFormatterValueDisplay.h \
    Source/Utils/DLT645DataHelper.h \
    Source/Utils/ChecksumHelper.h \
    Source/Utils/Logger.h \
    Source/Utils/LoggerTypes.h \
    Source/Utils/UiTheme.h \
    Source/Models/ConnectionConfig.h \
    Source/Models/ConnectionInfo.h \
    Source/Models/ConnectionInfoMapper.h \
    Source/Models/CategoryCatalog.h \
    Source/Models/DataItem.h \
    Source/Models/MeterInfo.h \
    Source/Models/MeterModel.h \
    Source/Models/MeterPhaseType.h \
    Source/Models/TermDefinition.h \
    Source/Models/MeterModelInitializationEventPrivate.h \
    Source/Models/MeterModelPrivate.h \
    Source/Models/MeterModelPrivateCommon.h \
    Source/Models/MeterModelPrivateFreeze.h \
    Source/Models/MeterModelPrivateRate.h \
    Source/Models/MeterModelPrivateSupport.h \
    Source/Models/MeterModelPrivateSync.h \
    Source/Models/MeterModelPrivateRandom.h \
    Source/Models/MeterModelPrivateRuntime.h \
    Source/ViewModels/MainWindowViewModel.h \
    Source/Services/ProtocolParser.h \
    Source/Services/MeterDataStore.h \
    Source/Services/ConfigurationSerializer.h \
    Source/Services/TermCatalogService.h \
    Source/Controllers/ConnectionManager.h \
    Source/Controllers/SerialController.h \
    Source/Controllers/TcpController.h \
    Source/Controllers/MasterQueryController.h \
    Source/Controllers/ProtocolController.h \
    Source/Views/OperationLog/OperationLogWidget.h \
    Source/Views/DeviceTree/DeviceTreeWidget.h \
    Source/Views/DataTable/DataTableWidget.h \
    Source/Views/DataTable/DataTableExporter.h \
    Source/Views/Common/FrameParseResultWidget.h \
    Source/Views/Common/TermExplanationWidget.h \
    Source/Views/CommunicationBar/CommunicationBarWidget.h \
    Source/Views/Dialogs/NewConnectionDialog.h \
    Source/Views/Dialogs/NewMeterDialog.h \
    Source/Views/Dialogs/CommunicationDialog.h \
    Source/Views/Dialogs/BatchNewMeterDialog.h \
    Source/Views/Dialogs/EventRecordDialog.h \
    Source/Views/Dialogs/MasterQueryDialog.h \
    Source/Views/Dialogs/FrameParseDialog.h \
    Source/Views/Dialogs/DataItemManualDialog.h \
    Source/Views/Dialogs/QueryFrameDialog.h

# UI文件
FORMS += \
    Source/Views/MainWindow/mainwindow.ui

# 资源文件
RESOURCES += \
    resources.qrc

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
