QT += core gui widgets testlib

CONFIG += c++11 console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = shortcut_settings_section_header_tests

SOURCES += shortcut_settings_section_header_tests.cpp

HEADERS += ../../src/ui/shortcut_settings_section.h
