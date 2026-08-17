QT += core testlib

CONFIG += c++17 console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = current_status_snapshot_tests

SOURCES += \
    current_status_snapshot_tests.cpp \
    ../../src/result_flow_config.cpp \
    ../../src/ui/current_status_snapshot.cpp

HEADERS += \
    ../../src/config/app_settings_data.h \
    ../../src/domain/function_settings.h \
    ../../src/result_flow_config.h \
    ../../src/ui/current_status_snapshot.h
