QT += core testlib

CONFIG += console testcase c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = history_modes_tests

INCLUDEPATH += ../..

SOURCES += \
    history_modes_tests.cpp \
    ../../src/domain/history_modes.cpp \
    ../../src/result_flow_config.cpp

HEADERS += \
    ../../src/domain/app_legacy_types.h \
    ../../src/domain/history_modes.h \
    ../../src/domain/history_types.h \
    ../../src/result_flow_config.h
