QT += core gui widgets testlib

CONFIG += c++11 console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = history_page_controller_header_tests

SOURCES += history_page_controller_header_tests.cpp

HEADERS += \
    ../../src/domain/app_legacy_types.h \
    ../../src/domain/history_modes.h \
    ../../src/domain/history_types.h \
    ../../src/ui/history_page_controller.h
