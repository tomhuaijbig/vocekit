QT += core network websockets testlib

CONFIG += c++11 console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = provider_streaming_websocket_transport_tests

INCLUDEPATH += ../..

SOURCES += \
    provider_streaming_websocket_transport_tests.cpp \
    ../../src/providers/provider_streaming_websocket_transport.cpp \
    ../../src/result_flow_config.cpp

HEADERS += \
    ../../src/domain/execution_types.h \
    ../../src/domain/operation_error.h \
    ../../src/providers/provider_streaming_websocket_transport.h \
    ../../src/providers/provider_types.h \
    ../../src/result_flow_config.h
