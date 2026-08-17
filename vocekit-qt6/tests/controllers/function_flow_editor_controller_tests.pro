QT += core gui widgets testlib
CONFIG += console testcase c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = function_flow_editor_controller_tests

INCLUDEPATH += ../..

SOURCES += \
    function_flow_editor_controller_tests.cpp \
    ../../src/controllers/function_flow_editor_controller.cpp \
    ../../src/domain/function_flow_graph.cpp \
    ../../src/domain/function_flow_ports.cpp \
    ../../src/result_flow_config.cpp

HEADERS += \
    ../../src/controllers/function_flow_editor_controller.h \
    ../../src/domain/function_flow_graph.h \
    ../../src/domain/function_flow_ports.h \
    ../../src/domain/function_flow_publication_types.h \
    ../../src/domain/function_flow_runtime_types.h \
    ../../src/domain/function_flow_validation.h \
    ../../src/domain/function_settings.h \
    ../../src/ui/function_flow_settings_access.h
