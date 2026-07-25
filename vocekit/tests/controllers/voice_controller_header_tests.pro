QT += core testlib

CONFIG += c++11 console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = voice_controller_header_tests

SOURCES += voice_controller_header_tests.cpp

HEADERS += \
    ../../src/controllers/voice_controller.h
