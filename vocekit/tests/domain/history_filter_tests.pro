QT += core testlib

CONFIG += console testcase c++11
CONFIG -= app_bundle
TEMPLATE = app
TARGET = history_filter_tests

INCLUDEPATH += ../..

SOURCES += \
    history_filter_tests.cpp \
    ../../src/domain/history_filter.cpp \
    ../../src/domain/history_modes.cpp \
    ../../src/domain/history_text.cpp \
    ../../src/result_flow_config.cpp

HEADERS += \
    ../../src/domain/app_legacy_types.h \
    ../../src/domain/history_filter.h \
    ../../src/domain/history_modes.h \
    ../../src/domain/history_text.h \
    ../../src/domain/history_types.h \
    ../../src/result_flow_config.h
