QT += core gui widgets testlib

CONFIG += c++17 console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = basic_settings_section_header_tests

SOURCES += basic_settings_section_header_tests.cpp

HEADERS += ../../src/ui/basic_settings_section.h
