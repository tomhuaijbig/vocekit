QT += core testlib

CONFIG += c++11 console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = hub_application_event_coordinator_tests

SOURCES += \
    hub_application_event_coordinator_tests.cpp \
    ../../src/app/application_events.cpp \
    ../../src/ui/hub_application_event_coordinator.cpp

HEADERS += \
    ../../src/app/application_events.h \
    ../../src/ui/hub_application_event_coordinator.h
