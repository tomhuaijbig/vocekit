QT += core testlib
CONFIG += console testcase c++11
CONFIG -= app_bundle
TEMPLATE = app
TARGET = hotkey_parser_tests

INCLUDEPATH += ../..

SOURCES += \
    hotkey_parser_tests.cpp \
    ../../src/input/hotkey_parser.cpp

HEADERS += \
    ../../src/input/hotkey_parser.h
