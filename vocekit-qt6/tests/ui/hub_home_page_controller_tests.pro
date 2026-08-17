QT += core gui widgets testlib

CONFIG += c++17 console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = hub_home_page_controller_tests

SOURCES += hub_home_page_controller_tests.cpp

HEADERS += \
    ../../src/ui/home_page.h \
    ../../src/ui/hub_home_page_controller.h
