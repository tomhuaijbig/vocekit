QT += core gui widgets testlib

CONFIG += c++17 console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = settings_panel_header_tests

SOURCES += settings_panel_header_tests.cpp

HEADERS += \
    ../../src/config/app_settings_data.h \
    ../../src/domain/function_settings.h \
    ../../src/ui/basic_settings_section.h \
    ../../src/ui/settings_panel.h
