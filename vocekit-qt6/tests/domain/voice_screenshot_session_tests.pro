QT += core gui testlib
CONFIG += console testcase c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = voice_screenshot_session_tests

INCLUDEPATH += ../..

SOURCES += \
    voice_screenshot_session_tests.cpp \
    ../../src/domain/voice_run_context.cpp \
    ../../src/domain/voice_screenshot_session.cpp

HEADERS += \
    ../../src/domain/voice_run_context.h \
    ../../src/domain/voice_screenshot_session.h \
    ../../src/ocr/ocr_types.h \
    ../../src/result_flow_config.h
