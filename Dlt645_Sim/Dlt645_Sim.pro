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

# 婧愭枃浠?
SOURCES += \
    Source/main.cpp \
    Source/Views/MainWindow/mainwindow.cpp \
    Source/Utils/BCDHelper.cpp \
    Source/Utils/DLT645FrameFormatter.cpp \
    Source/Utils/DLT645DataHelper.cpp \
    Source/Utils/ChecksumHelper.cpp \
    Source/Utils/Logger.cpp \
    Source/Models/DataItem.cpp \
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
    Source/Services/DataGenerator.cpp \
    Source/Services/MeterDataStore.cpp \
    Source/Services/ConfigurationSerializer.cpp \
    Source/Controllers/ConnectionManager.cpp \
    Source/Controllers/SerialController.cpp \
    Source/Controllers/TcpController.cpp \
    Source/Controllers/MasterQueryController.cpp \
    Source/Controllers/ProtocolController.cpp \
    Source/Views/OperationLog/OperationLogWidget.cpp \
    Source/Views/DeviceTree/DeviceTreeWidget.cpp \
    Source/Views/DataTable/DataTableWidget.cpp \
    Source/Views/Common/FrameParseResultWidget.cpp \
    Source/Views/CommunicationBar/CommunicationBarWidget.cpp \
    Source/Views/Dialogs/NewConnectionDialog.cpp \
    Source/Views/Dialogs/NewMeterDialog.cpp \
    Source/Views/Dialogs/CommunicationDialog.cpp \
    Source/Views/Dialogs/BatchNewMeterDialog.cpp \
    Source/Views/Dialogs/MasterQueryDialog.cpp \
    Source/Views/Dialogs/FrameParseDialog.cpp \
    Source/Views/Dialogs/QueryFrameDialog.cpp

# 澶存枃浠?
HEADERS += \
    Source/Views/MainWindow/mainwindow.h \
    Source/Utils/BCDHelper.h \
    Source/Utils/DLT645FrameFormatter.h \
    Source/Utils/DLT645FrameFormatterFields.h \
    Source/Utils/DLT645FrameFormatterValueDisplay.h \
    Source/Utils/DLT645DataHelper.h \
    Source/Utils/ChecksumHelper.h \
    Source/Utils/Logger.h \
    Source/Models/ConnectionConfig.h \
    Source/Models/DataItem.h \
    Source/Models/MeterModel.h \
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
    Source/Services/DataGenerator.h \
    Source/Services/MeterDataStore.h \
    Source/Services/ConfigurationSerializer.h \
    Source/Controllers/ConnectionManager.h \
    Source/Controllers/SerialController.h \
    Source/Controllers/TcpController.h \
    Source/Controllers/MasterQueryController.h \
    Source/Controllers/ProtocolController.h \
    Source/Views/OperationLog/OperationLogWidget.h \
    Source/Views/DeviceTree/DeviceTreeWidget.h \
    Source/Views/DataTable/DataTableWidget.h \
    Source/Views/Common/FrameParseResultWidget.h \
    Source/Views/CommunicationBar/CommunicationBarWidget.h \
    Source/Views/Dialogs/NewConnectionDialog.h \
    Source/Views/Dialogs/NewMeterDialog.h \
    Source/Views/Dialogs/CommunicationDialog.h \
    Source/Views/Dialogs/BatchNewMeterDialog.h \
    Source/Views/Dialogs/MasterQueryDialog.h \
    Source/Views/Dialogs/FrameParseDialog.h \
    Source/Views/Dialogs/QueryFrameDialog.h

# UI鏂囦欢
FORMS += \
    Source/Views/MainWindow/mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
