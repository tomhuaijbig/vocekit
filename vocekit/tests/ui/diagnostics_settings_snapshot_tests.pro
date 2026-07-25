QT += core testlib

CONFIG += c++11 console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = diagnostics_settings_snapshot_tests

SOURCES += \
    diagnostics_settings_snapshot_tests.cpp \
    ../../src/result_flow_config.cpp \
    ../../src/ui/diagnostics_settings_snapshot.cpp

HEADERS += \
    ../../src/config/app_settings_data.h \
    ../../src/domain/function_settings.h \
    ../../src/result_flow_config.h \
    ../../src/ui/diagnostics_settings_snapshot.h
