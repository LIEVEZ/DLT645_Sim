QT += core testlib network serialport sql
QT -= gui

CONFIG += console c++17 testcase
TEMPLATE = app

INCLUDEPATH += ../../Source

SOURCES += \
    ProtocolControllerTest.cpp \
    ../../Source/Controllers/ProtocolController.cpp \
    ../../Source/Controllers/SerialController.cpp \
    ../../Source/Controllers/TcpController.cpp \
    ../../Source/Services/ProtocolParser.cpp \
    ../../Source/Models/CategoryCatalog.cpp \
    ../../Source/Models/DataItem.cpp \
    ../../Source/Models/StatusWordBitCatalog.cpp \
    ../../Source/Utils/BCDHelper.cpp \
    ../../Source/Utils/ChecksumHelper.cpp \
    ../../Source/Utils/DLT645BlockHandler.cpp \
    ../../Source/Utils/DLT645DataHelper.cpp \
    ../../Source/Utils/Logger.cpp

SOURCES += $$files(../../Source/Models/MeterModel*.cpp)

HEADERS += \
    ../../Source/Controllers/ProtocolController.h \
    ../../Source/Controllers/SerialController.h \
    ../../Source/Controllers/TcpController.h \
    ../../Source/Services/ProtocolParser.h \
    ../../Source/Models/MeterModel.h \
    ../../Source/Models/CategoryCatalog.h \
    ../../Source/Models/DataItem.h \
    ../../Source/Models/StatusWordBitCatalog.h \
    ../../Source/Utils/BCDHelper.h \
    ../../Source/Utils/ChecksumHelper.h \
    ../../Source/Utils/DLT645BlockHandler.h \
    ../../Source/Utils/DLT645DataHelper.h \
    ../../Source/Utils/Logger.h
