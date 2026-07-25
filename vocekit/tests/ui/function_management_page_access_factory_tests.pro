QT += core gui widgets testlib

CONFIG += c++11 console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = function_management_page_access_factory_tests

SOURCES += \
    function_management_page_access_factory_tests.cpp \
    ../../src/capture/screenshot_types.cpp \
    ../../src/config/app_paths.cpp \
    ../../src/config/app_settings_defaults.cpp \
    ../../src/domain/function_settings.cpp \
    ../../src/input/hotkey_definitions.cpp \
    ../../src/result_flow_config.cpp \
    ../../src/ui/function_command_page_access_factory.cpp \
    ../../src/ui/function_management_page_access_factory.cpp \
    ../../src/ui/function_pages_access_factory.cpp \
    ../../src/ui/hub_settings_state.cpp

HEADERS += \
    ../../src/config/app_settings_data.h \
    ../../src/domain/app_legacy_types.h \
    ../../src/domain/function_settings.h \
    ../../src/domain/prompt_runtime_library.h \
    ../../src/ui/function_command_page.h \
    ../../src/ui/function_command_page_access_factory.h \
    ../../src/ui/function_management_page.h \
    ../../src/ui/function_management_page_access_factory.h \
    ../../src/ui/function_pages_access_factory.h \
    ../../src/ui/hub_settings_state.h
