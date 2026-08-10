QT += core multimedia testlib

CONFIG += c++11 console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = voice_audio_recorder_stream_tests

INCLUDEPATH += ../..

SOURCES += voice_audio_recorder_stream_tests.cpp
