QT += core gui widgets testlib
CONFIG += console testcase c++11
CONFIG -= app_bundle
TEMPLATE = app
TARGET = screenshot_launcher_target_tests

INCLUDEPATH += ../..

SOURCES += \
    screenshot_launcher_target_tests.cpp \
    ../../src/capture/screenshot_launcher.cpp

HEADERS += \
    ../../src/capture/screenshot_launcher.h
