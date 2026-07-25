QT += core gui widgets testlib

CONFIG += c++11 console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = command_center_shell_access_factory_tests

SOURCES += \
    command_center_shell_access_factory_tests.cpp \
    ../../src/capture/screenshot_types.cpp \
    ../../src/config/app_paths.cpp \
    ../../src/config/app_settings_defaults.cpp \
    ../../src/domain/function_catalog.cpp \
    ../../src/domain/function_settings.cpp \
    ../../src/input/hotkey_definitions.cpp \
    ../../src/result_flow_config.cpp \
    ../../src/ui/command_center_shell_access_factory.cpp \
    ../../src/ui/hub_settings_state.cpp \
    ../../src/ui/shortcut_display.cpp

HEADERS += \
    ../../src/config/app_settings_data.h \
    ../../src/domain/app_legacy_types.h \
    ../../src/domain/function_settings.h \
    ../../src/input/hotkey_definitions.h \
    ../../src/ui/command_center_shell.h \
    ../../src/ui/command_center_shell_access_factory.h \
    ../../src/ui/hub_settings_state.h \
    ../../src/ui/shortcut_display.h
