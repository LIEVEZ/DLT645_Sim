QT += core testlib
QT -= gui

CONFIG += console c++17 testcase
TEMPLATE = app

INCLUDEPATH += ../../Source

SOURCES += \
    PowerFactorFormatTest.cpp \
    ../../Source/Utils/BCDHelper.cpp

HEADERS += \
    ../../Source/Utils/BCDHelper.h
