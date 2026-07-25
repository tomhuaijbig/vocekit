QT += core gui testlib

CONFIG += c++11 console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = function_editor_coordinator_tests

SOURCES += \
    function_editor_coordinator_tests.cpp \
    ../../src/capture/screenshot_types.cpp \
    ../../src/config/app_paths.cpp \
    ../../src/config/app_settings_defaults.cpp \
    ../../src/config/secret_config.cpp \
    ../../src/config/secret_store.cpp \
    ../../src/domain/function_settings.cpp \
    ../../src/domain/prompt_runtime_library.cpp \
    ../../src/file_utils.cpp \
    ../../src/input/hotkey_definitions.cpp \
    ../../src/providers/model_catalog.cpp \
    ../../src/result_flow_config.cpp \
    ../../src/ui/function_editor_coordinator.cpp \
    ../../src/ui/function_editor_dialog_access_factory.cpp \
    ../../src/ui/function_summary_formatter.cpp \
    ../../src/ui/hub_settings_state.cpp \
    ../../src/ui/shortcut_display.cpp

HEADERS += \
    ../../src/config/app_settings_data.h \
    ../../src/domain/app_legacy_types.h \
    ../../src/domain/function_settings.h \
    ../../src/domain/prompt_runtime_library.h \
    ../../src/ui/function_editor_coordinator.h \
    ../../src/ui/function_editor_dialog.h \
    ../../src/ui/function_editor_dialog_access_factory.h \
    ../../src/ui/function_summary_formatter.h \
    ../../src/ui/hub_settings_state.h \
    ../../src/ui/prompt_settings_adapter.h
