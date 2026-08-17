QT += core testlib
CONFIG += console testcase c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = function_flow_errors_tests

INCLUDEPATH += ../..

SOURCES += \
    function_flow_errors_tests.cpp \
    ../../src/domain/function_flow_errors.cpp

HEADERS += \
    ../../src/domain/function_flow_errors.h \
    ../../src/domain/operation_error.h
