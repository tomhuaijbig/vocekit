QT += core gui testlib
CONFIG += console testcase c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = function_flow_execution_controller_tests

INCLUDEPATH += ../..

win32:LIBS += -luser32

SOURCES += \
    function_flow_execution_controller_tests.cpp \
    ../../src/controllers/function_flow_execution_controller.cpp \
    ../../src/domain/function_flow_compiler.cpp \
    ../../src/domain/function_flow_graph.cpp \
    ../../src/domain/function_flow_ports.cpp \
    ../../src/domain/function_flow_runtime_types.cpp \
    ../../src/domain/function_flow_scheduler.cpp \
    ../../src/tasks/cancellation_token.cpp

HEADERS += \
    ../../src/controllers/function_flow_execution_controller.h \
    ../../src/domain/execution_types.h \
    ../../src/domain/function_flow_compiler.h \
    ../../src/domain/function_flow_graph.h \
    ../../src/domain/function_flow_ports.h \
    ../../src/domain/function_flow_runtime_types.h \
    ../../src/domain/function_flow_scheduler.h \
    ../../src/domain/operation_error.h \
    ../../src/ocr/ocr_types.h \
    ../../src/recording/segmented_recording.h \
    ../../src/tasks/cancellation_token.h
