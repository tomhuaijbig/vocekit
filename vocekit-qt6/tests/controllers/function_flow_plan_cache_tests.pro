QT += core gui testlib
CONFIG += console testcase c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = function_flow_plan_cache_tests

INCLUDEPATH += ../..

SOURCES += \
    function_flow_plan_cache_tests.cpp \
    ../../src/controllers/function_flow_plan_cache.cpp \
    ../../src/domain/function_flow_compiler.cpp \
    ../../src/domain/function_flow_graph.cpp \
    ../../src/domain/function_flow_ports.cpp \
    ../../src/domain/function_settings.cpp \
    ../../src/result_flow_config.cpp

HEADERS += \
    ../../src/config/app_settings_data.h \
    ../../src/controllers/function_flow_plan_cache.h \
    ../../src/domain/function_flow_compiler.h \
    ../../src/domain/function_flow_graph.h \
    ../../src/domain/function_flow_ports.h \
    ../../src/domain/function_flow_runtime_types.h \
    ../../src/domain/function_settings.h \
    ../../src/domain/operation_error.h \
    ../../src/result_flow_config.h
