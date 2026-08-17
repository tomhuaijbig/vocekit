QT += core gui widgets testlib concurrent
CONFIG += console testcase c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = function_flow_input_adapters_tests

INCLUDEPATH += ../..

SOURCES += \
    function_flow_input_adapters_tests.cpp \
    ../../src/config/app_paths.cpp \
    ../../src/config/app_settings_defaults.cpp \
    ../../src/controllers/function_flow_runtime_adapters.cpp \
    ../../src/domain/function_flow_graph.cpp \
    ../../src/domain/function_flow_model_message.cpp \
    ../../src/domain/function_flow_ports.cpp \
    ../../src/domain/function_flow_runtime_types.cpp \
    ../../src/domain/prompt_runtime_library.cpp \
    ../../src/file_utils.cpp \
    ../../src/result_flow_config.cpp \
    ../../src/tasks/function_flow_model_task_runner.cpp \
    ../../src/tasks/cancellation_token.cpp

HEADERS += \
    ../../src/config/app_settings_data.h \
    ../../src/controllers/function_flow_runtime_adapters.h \
    ../../src/controllers/selected_text_workflow_controller.h \
    ../../src/domain/execution_types.h \
    ../../src/domain/function_flow_graph.h \
    ../../src/domain/function_flow_model_message.h \
    ../../src/domain/function_flow_ports.h \
    ../../src/domain/function_flow_runtime_types.h \
    ../../src/domain/function_settings.h \
    ../../src/domain/operation_error.h \
    ../../src/domain/prompt_runtime_library.h \
    ../../src/tasks/cancellation_token.h \
    ../../src/tasks/function_flow_model_task_runner.h \
    ../../src/tasks/model_request_task.h
