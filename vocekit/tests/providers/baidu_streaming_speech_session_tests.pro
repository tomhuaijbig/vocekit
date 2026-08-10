QT += core network websockets testlib

CONFIG += c++11 console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = baidu_streaming_speech_session_tests

INCLUDEPATH += ../..

SOURCES += \
    baidu_streaming_speech_session_tests.cpp \
    ../../src/providers/baidu_realtime_speech_protocol.cpp \
    ../../src/providers/baidu_streaming_speech_session.cpp \
    ../../src/providers/streaming_transcript_accumulator.cpp

HEADERS += \
    ../../src/config/secret_config.h \
    ../../src/domain/operation_error.h \
    ../../src/providers/baidu_realtime_speech_protocol.h \
    ../../src/providers/baidu_streaming_speech_session.h \
    ../../src/providers/provider_streaming_websocket_transport.h \
    ../../src/providers/provider_types.h \
    ../../src/providers/streaming_speech_session.h \
    ../../src/providers/streaming_transcript_accumulator.h
