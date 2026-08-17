QT += core testlib
CONFIG += console testcase c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = function_flow_runtime_log_tests

INCLUDEPATH += ../..

SOURCES += \
    function_flow_runtime_log_tests.cpp \
    ../../src/domain/function_flow_errors.cpp \
    ../../src/runtime/function_flow_runtime_log.cpp

HEADERS += \
    ../../src/domain/execution_types.h \
    ../../src/domain/function_flow_errors.h \
    ../../src/domain/operation_error.h \
    ../../src/runtime/function_flow_runtime_log.h
