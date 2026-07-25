QT += core testlib

CONFIG += c++11 console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = selected_text_workflow_controller_tests

SOURCES += \
    selected_text_workflow_controller_tests.cpp \
    ../../src/controllers/selected_text_workflow_controller.cpp

HEADERS += \
    ../../src/controllers/selected_text_workflow_controller.h \
    ../../src/input/voice_input_collector.h
