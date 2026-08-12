QT += core testlib

CONFIG += c++11 console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = windows_speech_helper_client_tests

INCLUDEPATH += ../..

SOURCES += \
    windows_speech_helper_client_tests.cpp \
    ../../src/providers/windows_speech_helper_client.cpp \
    ../../src/providers/windows_speech_helper_protocol.cpp \
    ../../src/tasks/cancellation_token.cpp

HEADERS += \
    ../../src/providers/windows_speech_helper_client.h \
    ../../src/providers/windows_speech_helper_protocol.h \
    ../../src/tasks/cancellation_token.h \
    ../../src/domain/execution_types.h
