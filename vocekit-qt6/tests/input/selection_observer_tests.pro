QT += core gui widgets testlib
CONFIG += console testcase c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = selection_observer_tests

INCLUDEPATH += ../..

SOURCES += \
    selection_observer_tests.cpp \
    ../../src/input/selection_observer.cpp

HEADERS += \
    ../../src/input/selection_observer.h \
    ../../src/input/selection_snapshot.h

win32:LIBS += -luser32 -lwtsapi32
