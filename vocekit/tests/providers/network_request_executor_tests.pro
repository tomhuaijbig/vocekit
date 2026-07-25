QT += core network testlib
CONFIG += console testcase c++11
CONFIG -= app_bundle
TEMPLATE = app
TARGET = network_request_executor_tests

INCLUDEPATH += ../..

SOURCES += \
    network_request_executor_tests.cpp \
    ../../src/providers/network_request_executor.cpp \
    ../../src/tasks/cancellation_token.cpp \
    ../../src/result_flow_config.cpp

HEADERS += \
    ../../src/domain/execution_types.h \
    ../../src/domain/operation_error.h \
    ../../src/providers/network_request_executor.h \
    ../../src/providers/provider_types.h \
    ../../src/result_flow_config.h \
    ../../src/tasks/cancellation_token.h
