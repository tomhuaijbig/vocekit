QT += core testlib
CONFIG += console testcase c++11
CONFIG -= app_bundle
TEMPLATE = app
TARGET = hotkey_definitions_tests

INCLUDEPATH += ../..

SOURCES += \
    hotkey_definitions_tests.cpp \
    ../../src/input/hotkey_definitions.cpp

HEADERS += \
    ../../src/domain/app_legacy_types.h \
    ../../src/input/hotkey_definitions.h \
    ../../src/result_flow_config.h
