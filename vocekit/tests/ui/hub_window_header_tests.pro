QT += core gui widgets testlib

CONFIG += c++11 console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = hub_window_header_tests

SOURCES += hub_window_header_tests.cpp

HEADERS += \
    ../../src/controllers/voice_controller_host.h \
    ../../src/ui/hub_window.h
