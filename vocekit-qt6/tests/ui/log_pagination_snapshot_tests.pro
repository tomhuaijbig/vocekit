QT += core testlib

CONFIG += c++17 console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = log_pagination_snapshot_tests

SOURCES += \
    log_pagination_snapshot_tests.cpp \
    ../../src/ui/log_pagination_snapshot.cpp

HEADERS += \
    ../../src/config/app_settings_data.h \
    ../../src/domain/function_settings.h \
    ../../src/ui/log_pagination_snapshot.h
