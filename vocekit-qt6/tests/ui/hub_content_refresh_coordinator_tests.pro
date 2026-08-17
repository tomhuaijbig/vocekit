QT += core testlib

CONFIG += c++17 console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = hub_content_refresh_coordinator_tests

SOURCES += \
    hub_content_refresh_coordinator_tests.cpp \
    ../../src/ui/hub_content_refresh_coordinator.cpp

HEADERS += \
    ../../src/ui/hub_content_refresh_coordinator.h
