QT += core testlib

CONFIG += c++17 console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = hub_refresh_coordinator_action_factory_tests

SOURCES += \
    hub_refresh_coordinator_action_factory_tests.cpp \
    ../../src/ui/hub_refresh_coordinator_action_factory.cpp \
    ../../src/ui/log_pagination_snapshot.cpp

HEADERS += \
    ../../src/ui/hub_refresh_coordinator_action_factory.h \
    ../../src/ui/hub_refresh_coordinator_bundle.h \
    ../../src/ui/log_pagination_snapshot.h
