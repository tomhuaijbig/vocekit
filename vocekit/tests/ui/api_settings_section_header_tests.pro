QT += core gui widgets testlib

CONFIG += c++11 console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = api_settings_section_header_tests

SOURCES += api_settings_section_header_tests.cpp

HEADERS += \
    ../../src/config/secret_config.h \
    ../../src/ui/api_settings_section.h
