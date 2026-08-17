QT += core gui testlib
CONFIG += console testcase c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = function_flow_validation_tests

INCLUDEPATH += ../..

SOURCES += \
    function_flow_validation_tests.cpp \
    ../../src/domain/function_flow_graph.cpp \
    ../../src/domain/function_flow_ports.cpp \
    ../../src/domain/function_flow_validation.cpp \
    ../../src/input/hotkey_parser.cpp

HEADERS += \
    ../../src/domain/function_flow_graph.h \
    ../../src/domain/function_flow_ports.h \
    ../../src/domain/function_flow_validation.h \
    ../../src/input/hotkey_parser.h
