QT += core gui testlib

CONFIG += c++17 console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = function_command_controller_tests

INCLUDEPATH += ../..

SOURCES += \
    function_command_controller_tests.cpp \
    ../../src/config/app_settings_defaults.cpp \
    ../../src/controllers/function_command_controller.cpp \
    ../../src/capture/screenshot_types.cpp \
    ../../src/domain/function_settings.cpp \
    ../../src/result_flow_config.cpp

HEADERS += \
    ../../src/controllers/function_command_controller.h \
    ../../src/controllers/selected_text_workflow_controller.h \
    ../../src/config/app_settings_data.h \
    ../../src/domain/function_settings.h \
    ../../src/input/voice_input_collector.h \
    ../../src/input/selected_text_reader.h \
    ../../src/capture/screenshot_types.h \
    ../../src/result_flow_config.h
