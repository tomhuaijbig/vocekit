QT += core gui testlib

CONFIG += console c++17 testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = recording_core_tests

SOURCES += \
    recording_core_tests.cpp \
    ../../src/input/hold_to_talk.cpp \
    ../../src/recording/segmented_recording.cpp

INCLUDEPATH += ../../src

win32:LIBS += -luser32
