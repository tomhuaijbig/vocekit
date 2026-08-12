QT += core testlib

CONFIG += c++11 console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = windows_speech_helper_protocol_tests

INCLUDEPATH += ../..

SOURCES += \
    windows_speech_helper_protocol_tests.cpp \
    ../../src/providers/windows_speech_helper_protocol.cpp

HEADERS += \
    ../../src/providers/windows_speech_helper_protocol.h
