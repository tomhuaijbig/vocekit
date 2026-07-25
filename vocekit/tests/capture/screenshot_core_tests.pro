QT += core gui testlib

CONFIG += c++11 console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = screenshot_core_tests

SOURCES += \
    screenshot_core_tests.cpp \
    ../../src/capture/screenshot_types.cpp

HEADERS += \
    ../../src/capture/screenshot_types.h
