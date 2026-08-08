QT += core gui widgets testlib

CONFIG += c++11 console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = tray_controller_exit_tests

SOURCES += \
    tray_controller_exit_tests.cpp \
    ../../src/config/app_settings_defaults.cpp \
    ../../src/controllers/tray_controller.cpp

HEADERS += \
    ../../src/config/app_settings_defaults.h \
    ../../src/controllers/tray_controller.h
