QT += core gui widgets testlib

CONFIG += c++11 console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = function_editor_dialog_header_tests

SOURCES += function_editor_dialog_header_tests.cpp

HEADERS += \
    ../../src/ui/function_editor_dialog.h
