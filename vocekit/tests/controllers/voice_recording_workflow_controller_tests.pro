QT += core testlib

CONFIG += c++11 console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = voice_recording_workflow_controller_tests

SOURCES += voice_recording_workflow_controller_tests.cpp

HEADERS += \
    ../../src/controllers/voice_recording_workflow_controller.h
