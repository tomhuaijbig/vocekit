QT += core gui widgets testlib
CONFIG += c++11 testcase
CONFIG -= debug_and_release debug
CONFIG += release
TEMPLATE = app
TARGET = basic_settings_section_floating_bar_tests

INCLUDEPATH += ../../src

SOURCES += \
    basic_settings_section_floating_bar_tests.cpp \
    ../../src/config/secret_config.cpp \
    ../../src/config/secret_store.cpp \
    ../../src/config/selection_context_action_customization.cpp \
    ../../src/domain/selection_context_actions.cpp \
    ../../src/file_utils.cpp \
    ../../src/providers/model_catalog.cpp \
    ../../src/ui/basic_settings_section.cpp \
    ../../src/ui/attention_message.cpp \
    ../../src/ui/floating_bar_style_selector.cpp \
    ../../src/ui/selection_context_action_editor.cpp \
    ../../src/ui/selection_context_settings_card.cpp \
    ../../src/ui/ui_style.cpp \
    ../../src/config/app_settings_defaults.cpp \
    ../../src/platform/windows_autostart.cpp

HEADERS += \
    ../../src/config/secret_config.h \
    ../../src/config/secret_store.h \
    ../../src/config/selection_context_action_customization.h \
    ../../src/file_utils.h \
    ../../src/providers/model_catalog.h \
    ../../src/ui/basic_settings_section.h \
    ../../src/ui/attention_message.h \
    ../../src/ui/floating_bar_style_selector.h \
    ../../src/ui/selection_context_action_editor.h \
    ../../src/ui/selection_context_settings_card.h \
    ../../src/ui/ui_style.h \
    ../../src/config/app_settings_defaults.h \
    ../../src/platform/windows_autostart.h
