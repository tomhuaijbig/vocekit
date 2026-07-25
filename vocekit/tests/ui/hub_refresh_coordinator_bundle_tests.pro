QT += core testlib

CONFIG += c++11 console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = hub_refresh_coordinator_bundle_tests

SOURCES += \
    hub_refresh_coordinator_bundle_tests.cpp \
    ../../src/app/application_events.cpp \
    ../../src/ui/hub_application_event_coordinator.cpp \
    ../../src/ui/hub_content_refresh_coordinator.cpp \
    ../../src/ui/hub_refresh_coordinator_bundle.cpp \
    ../../src/ui/hub_settings_refresh_coordinator.cpp

HEADERS += \
    ../../src/app/application_events.h \
    ../../src/ui/hub_application_event_coordinator.h \
    ../../src/ui/hub_content_refresh_coordinator.h \
    ../../src/ui/hub_refresh_coordinator_bundle.h \
    ../../src/ui/hub_settings_refresh_coordinator.h
