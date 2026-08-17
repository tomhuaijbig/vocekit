QT += core network websockets testlib

CONFIG += c++17 console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = xfyun_streaming_speech_session_tests

INCLUDEPATH += ../..

SOURCES += \
    xfyun_streaming_speech_session_tests.cpp \
    ../../src/api/api_client_utils.cpp \
    ../../src/providers/streaming_transcript_accumulator.cpp \
    ../../src/providers/xfyun_speech_protocol.cpp \
    ../../src/providers/xfyun_streaming_speech_session.cpp

HEADERS += \
    ../../src/api/api_client_utils.h \
    ../../src/config/secret_config.h \
    ../../src/domain/operation_error.h \
    ../../src/providers/provider_streaming_websocket_transport.h \
    ../../src/providers/provider_types.h \
    ../../src/providers/streaming_speech_session.h \
    ../../src/providers/streaming_transcript_accumulator.h \
    ../../src/providers/xfyun_speech_protocol.h \
    ../../src/providers/xfyun_streaming_speech_session.h
