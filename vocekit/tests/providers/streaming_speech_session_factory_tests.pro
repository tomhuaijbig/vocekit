QT += core network websockets testlib

CONFIG += c++11 console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = streaming_speech_session_factory_tests

INCLUDEPATH += ../..

SOURCES += \
    streaming_speech_session_factory_tests.cpp \
    ../../src/providers/streaming_speech_session_factory.cpp

HEADERS += \
    ../../src/config/secret_config.h \
    ../../src/providers/streaming_speech_session.h \
    ../../src/providers/streaming_speech_session_factory.h
