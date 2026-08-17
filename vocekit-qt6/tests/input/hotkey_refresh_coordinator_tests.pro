QT += core gui testlib

CONFIG += c++17 console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = hotkey_refresh_coordinator_tests

INCLUDEPATH += ../..

SOURCES += \
    hotkey_refresh_coordinator_tests.cpp \
    ../../src/capture/screenshot_types.cpp \
    ../../src/input/hotkey_refresh_coordinator.cpp \
    ../../src/input/hold_to_talk.cpp

HEADERS += \
    ../../src/capture/screenshot_types.h \
    ../../src/domain/function_settings.h \
    ../../src/input/global_hotkeys.h \
    ../../src/input/hold_to_talk.h \
    ../../src/input/hotkey_refresh_coordinator.h
