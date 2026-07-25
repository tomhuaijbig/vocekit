QT += core gui testlib

CONFIG += c++11 console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = custom_function_creation_coordinator_tests

SOURCES += \
    custom_function_creation_coordinator_tests.cpp \
    ../../src/capture/screenshot_types.cpp \
    ../../src/config/app_paths.cpp \
    ../../src/config/app_settings_defaults.cpp \
    ../../src/domain/function_settings.cpp \
    ../../src/input/hotkey_definitions.cpp \
    ../../src/result_flow_config.cpp \
    ../../src/ui/custom_function_creation_coordinator.cpp \
    ../../src/ui/hub_settings_state.cpp

HEADERS += \
    ../../src/config/app_settings_data.h \
    ../../src/domain/app_legacy_types.h \
    ../../src/domain/function_settings.h \
    ../../src/ui/custom_function_creation_coordinator.h \
    ../../src/ui/hub_settings_state.h
