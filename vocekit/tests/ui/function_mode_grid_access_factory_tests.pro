QT += core gui widgets testlib

CONFIG += c++11 console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = function_mode_grid_access_factory_tests

SOURCES += \
    function_mode_grid_access_factory_tests.cpp \
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
    ../../src/ui/function_mode_grid_access_factory.cpp \
    ../../src/ui/hub_settings_state.cpp

HEADERS += \
    ../../src/config/app_settings_data.h \
    ../../src/config/secret_config.h \
    ../../src/config/secret_store.h \
    ../../src/config/selection_context_action_customization.h \
    ../../src/domain/selection_context_actions.h \
    ../../src/file_utils.h \
    ../../src/providers/model_catalog.h \
    ../../src/domain/app_legacy_types.h \
    ../../src/domain/function_settings.h \
    ../../src/ui/function_mode_grid.h \
    ../../src/ui/function_mode_grid_access_factory.h \
    ../../src/ui/home_page_access_factory.h \
    ../../src/ui/hub_settings_state.h
