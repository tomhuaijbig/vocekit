QT += core gui widgets testlib

CONFIG += c++11 console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = command_center_shell_header_tests

SOURCES += command_center_shell_header_tests.cpp

HEADERS += \
    ../../src/ui/command_center_shell.h
