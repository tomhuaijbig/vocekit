QT += core gui concurrent testlib
CONFIG += console testcase c++11
CONFIG -= app_bundle
TEMPLATE = app
TARGET = selection_context_coordinator_tests

INCLUDEPATH += ../..

SOURCES += \
    selection_context_coordinator_tests.cpp \
    ../../src/controllers/selection_context_coordinator.cpp \
    ../../src/controllers/selection_context_policy.cpp

HEADERS += \
    ../../src/controllers/selection_context_coordinator.h \
    ../../src/controllers/selection_context_policy.h \
    ../../src/input/selection_observer.h \
    ../../src/input/selection_snapshot.h \
    ../../src/config/app_settings_data.h
