QT += core gui testlib
CONFIG += console testcase c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = function_flow_compiler_tests

INCLUDEPATH += ../..

SOURCES += \
    function_flow_compiler_tests.cpp \
    ../../src/domain/function_flow_compiler.cpp \
    ../../src/domain/function_flow_graph.cpp \
    ../../src/domain/function_flow_ports.cpp \
    ../../src/domain/function_flow_runtime_types.cpp \
    ../../src/tasks/cancellation_token.cpp

HEADERS += \
    ../../src/domain/execution_types.h \
    ../../src/domain/function_flow_compiler.h \
    ../../src/domain/function_flow_graph.h \
    ../../src/domain/function_flow_ports.h \
    ../../src/domain/function_flow_runtime_types.h \
    ../../src/domain/operation_error.h \
    ../../src/ocr/ocr_types.h \
    ../../src/recording/segmented_recording.h \
    ../../src/tasks/cancellation_token.h
