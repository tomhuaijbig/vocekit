QT += core testlib
CONFIG += console testcase c++11
CONFIG -= app_bundle
TEMPLATE = app
TARGET = app_settings_defaults_tests

INCLUDEPATH += ../..

SOURCES += \
    app_settings_defaults_tests.cpp \
    ../../src/config/app_settings_defaults.cpp

HEADERS += \
    ../../src/config/app_settings_defaults.h
