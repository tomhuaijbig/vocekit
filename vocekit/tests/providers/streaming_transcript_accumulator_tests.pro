QT += core testlib

CONFIG += c++11 console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = streaming_transcript_accumulator_tests

INCLUDEPATH += ../..

SOURCES += \
    streaming_transcript_accumulator_tests.cpp \
    ../../src/providers/streaming_transcript_accumulator.cpp

HEADERS += \
    ../../src/providers/streaming_speech_session.h \
    ../../src/providers/streaming_transcript_accumulator.h
