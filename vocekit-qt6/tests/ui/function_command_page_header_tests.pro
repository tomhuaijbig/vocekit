QT += core gui widgets testlib

CONFIG += c++17 console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = function_command_page_header_tests

SOURCES += function_command_page_header_tests.cpp

HEADERS += \
    ../../src/ui/function_command_page.h
