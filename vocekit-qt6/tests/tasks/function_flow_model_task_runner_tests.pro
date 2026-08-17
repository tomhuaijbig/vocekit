QT += core gui testlib concurrent
CONFIG += console testcase c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = function_flow_model_task_runner_tests

INCLUDEPATH += ../..

SOURCES += \
    function_flow_model_task_runner_tests.cpp \
    ../../src/tasks/function_flow_model_task_runner.cpp \
    ../../src/tasks/cancellation_token.cpp

HEADERS += \
    ../../src/domain/execution_types.h \
    ../../src/domain/operation_error.h \
    ../../src/providers/model_provider.h \
    ../../src/providers/provider_types.h \
    ../../src/tasks/cancellation_token.h \
    ../../src/tasks/function_flow_model_task_runner.h \
    ../../src/tasks/model_request_task.h
