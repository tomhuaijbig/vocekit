QT += core gui widgets testlib

CONFIG += c++11 console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = floating_bar_streaming_tests

INCLUDEPATH += ../..

SOURCES += \
    floating_bar_streaming_tests.cpp \
    ../../src/ui/ui_style.cpp

HEADERS += \
    ../../src/ui/floating_bar.h \
    ../../src/ui/ui_style.h
