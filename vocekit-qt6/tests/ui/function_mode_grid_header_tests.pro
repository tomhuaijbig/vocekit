QT += core gui widgets testlib

CONFIG += c++17 console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = function_mode_grid_header_tests

SOURCES += function_mode_grid_header_tests.cpp

HEADERS += \
    ../../src/domain/app_legacy_types.h \
    ../../src/ui/function_mode_grid.h
