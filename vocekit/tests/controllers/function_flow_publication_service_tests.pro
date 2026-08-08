QT += core gui testlib
CONFIG += console testcase c++11
CONFIG -= app_bundle
TEMPLATE = app
TARGET = function_flow_publication_service_tests

INCLUDEPATH += ../..

SOURCES += \
    function_flow_publication_service_tests.cpp \
    ../../src/app/application_events.cpp \
    ../../src/controllers/function_flow_publication_service.cpp \
    ../../src/domain/function_flow_compiler.cpp \
    ../../src/domain/function_flow_graph.cpp \
    ../../src/domain/function_flow_ports.cpp \
    ../../src/domain/function_flow_validation.cpp \
    ../../src/domain/function_settings.cpp \
    ../../src/input/hotkey_parser.cpp \
    ../../src/result_flow_config.cpp

HEADERS += \
    ../../src/app/application_events.h \
    ../../src/config/app_settings_data.h \
    ../../src/controllers/function_flow_publication_service.h \
    ../../src/domain/function_flow_compiler.h \
    ../../src/domain/function_flow_graph.h \
    ../../src/domain/function_flow_ports.h \
    ../../src/domain/function_flow_publication_types.h \
    ../../src/domain/function_flow_runtime_types.h \
    ../../src/domain/function_flow_validation.h \
    ../../src/domain/function_settings.h \
    ../../src/domain/operation_error.h \
    ../../src/input/hotkey_parser.h \
    ../../src/result_flow_config.h
