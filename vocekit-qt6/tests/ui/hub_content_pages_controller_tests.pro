QT += core gui widgets testlib

CONFIG += c++17 console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = hub_content_pages_controller_tests

SOURCES += hub_content_pages_controller_tests.cpp

HEADERS += ../../src/ui/hub_content_pages_controller.h
