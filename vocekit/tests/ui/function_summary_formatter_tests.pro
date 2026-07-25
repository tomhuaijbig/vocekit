QT += core testlib

CONFIG += c++11 console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = function_summary_formatter_tests

SOURCES += \
    function_summary_formatter_tests.cpp \
    ../../src/ui/function_summary_formatter.cpp

HEADERS += \
    ../../src/ui/function_summary_formatter.h
