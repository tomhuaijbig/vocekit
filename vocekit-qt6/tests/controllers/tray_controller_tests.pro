QT += core gui widgets testlib
CONFIG += console testcase c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = tray_controller_tests

INCLUDEPATH += ../..

SOURCES += \
    tray_controller_tests.cpp \
    ../../src/config/app_settings_defaults.cpp \
    ../../src/controllers/tray_controller.cpp

HEADERS += \
    ../../src/config/app_settings_defaults.h \
    ../../src/controllers/tray_controller.h
