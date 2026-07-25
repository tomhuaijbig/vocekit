QT += core testlib

CONFIG += c++11 console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = screenshot_workflow_controller_tests

SOURCES += screenshot_workflow_controller_tests.cpp

HEADERS += \
    ../../src/controllers/screenshot_workflow_controller.h
