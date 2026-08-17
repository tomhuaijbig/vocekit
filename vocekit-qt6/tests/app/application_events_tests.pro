QT += core testlib
CONFIG += console testcase c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = application_events_tests

INCLUDEPATH += ../..

SOURCES += \
    application_events_tests.cpp \
    ../../src/app/application_events.cpp

HEADERS += \
    ../../src/app/application_events.h
