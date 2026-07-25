QT += core gui widgets testlib

CONFIG += c++11 console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = history_settings_section_header_tests

SOURCES += history_settings_section_header_tests.cpp

HEADERS += ../../src/ui/history_settings_section.h
