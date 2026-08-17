QT += core gui widgets testlib

CONFIG += c++17 console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = prompt_settings_adapter_header_tests

SOURCES += prompt_settings_adapter_header_tests.cpp

HEADERS += \
    ../../src/domain/prompt_runtime_library.h \
    ../../src/ui/prompt_settings_adapter.h
