QT += core gui testlib
CONFIG += console testcase c++11
CONFIG -= app_bundle
TEMPLATE = app
TARGET = voice_run_session_tests

INCLUDEPATH += ../..

SOURCES += \
    voice_run_session_tests.cpp \
    ../../src/domain/voice_run_context.cpp \
    ../../src/domain/voice_run_session.cpp

HEADERS += \
    ../../src/domain/voice_run_context.h \
    ../../src/domain/voice_run_session.h \
    ../../src/ocr/ocr_types.h \
    ../../src/recording/segmented_recording.h \
    ../../src/result_flow_config.h
