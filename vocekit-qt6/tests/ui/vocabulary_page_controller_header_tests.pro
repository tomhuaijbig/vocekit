QT += core gui widgets testlib

CONFIG += c++17 console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = vocabulary_page_controller_header_tests

SOURCES += vocabulary_page_controller_header_tests.cpp

HEADERS += \
    ../../src/domain/app_legacy_types.h \
    ../../src/domain/history_types.h \
    ../../src/domain/vocabulary_io.h \
    ../../src/ui/vocabulary_page_controller.h
