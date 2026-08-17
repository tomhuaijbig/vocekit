QT += core gui widgets testlib
CONFIG += console testcase c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = function_flow_result_controller_tests

INCLUDEPATH += ../..

SOURCES += \
    function_flow_result_controller_tests.cpp \
    ../../src/capture/screenshot_result_window.cpp \
    ../../src/capture/screenshot_types.cpp \
    ../../src/config/app_settings_defaults.cpp \
    ../../src/controllers/function_flow_result_controller.cpp \
    ../../src/domain/function_flow_graph.cpp \
    ../../src/domain/function_flow_ports.cpp \
    ../../src/domain/function_flow_runtime_types.cpp \
    ../../src/output/clipboard_writer.cpp \
    ../../src/result_flow_config.cpp \
    ../../src/tasks/cancellation_token.cpp \
    ../../src/ui/app_dialogs.cpp \
    ../../src/ui/result_choice_popup.cpp \
    ../../src/ui/ui_style.cpp

HEADERS += \
    ../../src/capture/screenshot_result_window.h \
    ../../src/capture/screenshot_types.h \
    ../../src/controllers/function_flow_result_controller.h \
    ../../src/domain/execution_types.h \
    ../../src/domain/function_flow_compiler.h \
    ../../src/domain/function_flow_graph.h \
    ../../src/domain/function_flow_ports.h \
    ../../src/domain/function_flow_runtime_types.h \
    ../../src/domain/operation_error.h \
    ../../src/output/clipboard_writer.h \
    ../../src/tasks/cancellation_token.h \
    ../../src/ui/app_dialogs.h \
    ../../src/ui/result_choice_popup.h \
    ../../src/ui/ui_style.h
