QT += core gui widgets testlib
CONFIG += c++11 testcase
CONFIG -= debug_and_release debug
CONFIG += release
TEMPLATE = app
TARGET = basic_settings_section_floating_bar_tests

INCLUDEPATH += ../../src

SOURCES += \
    basic_settings_section_floating_bar_tests.cpp \
    ../../src/domain/selection_context_actions.cpp \
    ../../src/ui/basic_settings_section.cpp \
    ../../src/ui/attention_message.cpp \
    ../../src/ui/floating_bar_style_selector.cpp \
    ../../src/ui/selection_context_settings_card.cpp \
    ../../src/ui/ui_style.cpp \
    ../../src/config/app_settings_defaults.cpp \
    ../../src/platform/windows_autostart.cpp

HEADERS += \
    ../../src/ui/basic_settings_section.h \
    ../../src/ui/attention_message.h \
    ../../src/ui/floating_bar_style_selector.h \
    ../../src/ui/selection_context_settings_card.h \
    ../../src/ui/ui_style.h \
    ../../src/config/app_settings_defaults.h \
    ../../src/platform/windows_autostart.h
