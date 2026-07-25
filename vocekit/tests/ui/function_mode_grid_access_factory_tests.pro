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
    ../../src/domain/function_settings.cpp \
    ../../src/input/hotkey_definitions.cpp \
    ../../src/result_flow_config.cpp \
    ../../src/ui/function_mode_grid_access_factory.cpp \
    ../../src/ui/hub_settings_state.cpp

HEADERS += \
    ../../src/config/app_settings_data.h \
    ../../src/domain/app_legacy_types.h \
    ../../src/domain/function_settings.h \
    ../../src/ui/function_mode_grid.h \
    ../../src/ui/function_mode_grid_access_factory.h \
    ../../src/ui/home_page_access_factory.h \
    ../../src/ui/hub_settings_state.h
