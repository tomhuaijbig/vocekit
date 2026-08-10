QT += core gui widgets testlib

CONFIG += c++11 console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = basic_settings_section_tests

INCLUDEPATH += ../..

SOURCES += \
    basic_settings_section_tests.cpp \
    ../../src/config/app_settings_defaults.cpp \
    ../../src/platform/windows_autostart.cpp \
    ../../src/ui/attention_message.cpp \
    ../../src/ui/basic_settings_section.cpp \
    ../../src/ui/ui_style.cpp

HEADERS += \
    ../../src/config/app_settings_defaults.h \
    ../../src/platform/windows_autostart.h \
    ../../src/ui/attention_message.h \
    ../../src/ui/basic_settings_section.h \
    ../../src/ui/ui_style.h
