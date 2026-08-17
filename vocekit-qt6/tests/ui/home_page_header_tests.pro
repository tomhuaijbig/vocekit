QT += core gui widgets testlib

CONFIG += c++17 console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = home_page_header_tests

SOURCES += home_page_header_tests.cpp

HEADERS += \
    ../../src/ui/home_page.h
