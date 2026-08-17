QT += core gui widgets testlib

CONFIG += c++17 console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = prompt_access_factory_tests

SOURCES += \
    prompt_access_factory_tests.cpp \
    ../../src/capture/screenshot_types.cpp \
    ../../src/config/app_paths.cpp \
    ../../src/config/app_settings_defaults.cpp \
    ../../src/config/secret_config.cpp \
    ../../src/config/secret_store.cpp \
    ../../src/config/selection_context_action_customization.cpp \
    ../../src/domain/selection_context_actions.cpp \
    ../../src/file_utils.cpp \
    ../../src/providers/model_catalog.cpp \
    ../../src/domain/function_settings.cpp \
    ../../src/input/hotkey_definitions.cpp \
    ../../src/result_flow_config.cpp \
    ../../src/ui/hub_settings_state.cpp \
    ../../src/ui/prompt_access_factory.cpp

HEADERS += \
    ../../src/capture/screenshot_types.h \
    ../../src/config/app_settings_data.h \
    ../../src/config/secret_config.h \
    ../../src/config/secret_store.h \
    ../../src/config/selection_context_action_customization.h \
    ../../src/domain/selection_context_actions.h \
    ../../src/file_utils.h \
    ../../src/providers/model_catalog.h \
    ../../src/domain/app_legacy_types.h \
    ../../src/domain/function_settings.h \
    ../../src/domain/prompt_runtime_library.h \
    ../../src/ui/hub_settings_state.h \
    ../../src/ui/prompt_access_factory.h \
    ../../src/ui/prompt_settings_adapter.h \
    ../../src/ui/prompts_panel.h
