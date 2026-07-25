QT += core gui widgets testlib

CONFIG += c++11 console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = hub_function_workspace_controller_tests

SOURCES += hub_function_workspace_controller_tests.cpp

HEADERS += ../../src/ui/hub_function_workspace_controller.h
