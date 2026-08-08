QT += core gui widgets concurrent testlib

CONFIG += c++11 console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = screenshot_workflow_controller_tests

INCLUDEPATH += ../..

SOURCES += \
    screenshot_workflow_controller_tests.cpp \
    ../../src/controllers/screenshot_workflow_controller.cpp \
    ../../src/domain/function_flow_graph.cpp \
    ../../src/domain/function_flow_ports.cpp \
    ../../src/domain/function_flow_runtime_types.cpp \
    ../../src/domain/voice_screenshot_session.cpp \
    ../../src/tasks/cancellation_token.cpp

HEADERS += \
    ../../src/capture/screen_capture_overlay.h \
    ../../src/controllers/screenshot_workflow_controller.h \
    ../../src/domain/execution_types.h \
    ../../src/domain/function_flow_compiler.h \
    ../../src/domain/function_flow_graph.h \
    ../../src/domain/function_flow_ports.h \
    ../../src/domain/function_flow_runtime_types.h \
    ../../src/domain/operation_error.h \
    ../../src/domain/voice_screenshot_session.h \
    ../../src/ocr/ocr_manager.h \
    ../../src/ocr/ocr_types.h \
    ../../src/tasks/cancellation_token.h
