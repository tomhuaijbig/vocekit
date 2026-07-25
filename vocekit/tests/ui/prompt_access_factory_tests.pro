QT += core gui widgets testlib

CONFIG += c++11 console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = prompt_access_factory_tests

SOURCES += \
    prompt_access_factory_tests.cpp \
    ../../src/capture/screenshot_types.cpp \
    ../../src/config/app_paths.cpp \
    ../../src/config/app_settings_defaults.cpp \
    ../../src/domain/function_settings.cpp \
    ../../src/input/hotkey_definitions.cpp \
    ../../src/result_flow_config.cpp \
    ../../src/ui/hub_settings_state.cpp \
    ../../src/ui/prompt_access_factory.cpp

HEADERS += \
    ../../src/capture/screenshot_types.h \
    ../../src/config/app_settings_data.h \
    ../../src/domain/app_legacy_types.h \
    ../../src/domain/function_settings.h \
    ../../src/domain/prompt_runtime_library.h \
    ../../src/ui/hub_settings_state.h \
    ../../src/ui/prompt_access_factory.h \
    ../../src/ui/prompt_settings_adapter.h \
    ../../src/ui/prompts_panel.h
