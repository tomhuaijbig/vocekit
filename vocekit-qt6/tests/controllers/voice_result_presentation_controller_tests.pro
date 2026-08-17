QT += core testlib

CONFIG += c++17 console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = voice_result_presentation_controller_tests

SOURCES += voice_result_presentation_controller_tests.cpp

HEADERS += \
    ../../src/controllers/voice_result_presentation_controller.h
