QT += core testlib

CONFIG += c++11 console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = hub_settings_refresh_coordinator_tests

SOURCES += \
    hub_settings_refresh_coordinator_tests.cpp \
    ../../src/ui/hub_settings_refresh_coordinator.cpp

HEADERS += \
    ../../src/ui/hub_settings_refresh_coordinator.h
