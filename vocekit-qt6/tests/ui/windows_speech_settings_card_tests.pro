QT += core gui widgets concurrent testlib
CONFIG += c++17 console testcase
CONFIG -= app_bundle
TEMPLATE = app
TARGET = windows_speech_settings_card_tests

SOURCES += \
    windows_speech_settings_card_tests.cpp \
    ../../src/config/app_settings_defaults.cpp \
    ../../src/tasks/cancellation_token.cpp \
    ../../src/tasks/diagnostic_task_runner.cpp \
    ../../src/ui/ui_style.cpp \
    ../../src/ui/windows_speech_settings_card.cpp

HEADERS += \
    ../../src/config/app_settings_defaults.h \
    ../../src/tasks/cancellation_token.h \
    ../../src/tasks/diagnostic_task_runner.h \
    ../../src/ui/windows_speech_settings_card.h
