QT += core testlib

CONFIG += c++11 console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = windows_streaming_speech_session_tests

INCLUDEPATH += ../..

SOURCES += \
    windows_streaming_speech_session_tests.cpp \
    ../../src/providers/windows_streaming_speech_session.cpp \
    ../../src/providers/windows_speech_helper_protocol.cpp

HEADERS += \
    ../../src/providers/streaming_speech_session.h \
    ../../src/providers/windows_streaming_speech_session.h \
    ../../src/providers/windows_speech_helper_protocol.h
