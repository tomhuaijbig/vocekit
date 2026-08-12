QT += core gui widgets testlib
CONFIG += console testcase c++11
CONFIG -= app_bundle
TEMPLATE = app
TARGET = floating_bar_style_selector_tests

INCLUDEPATH += ../..

SOURCES += \
    floating_bar_style_selector_tests.cpp \
    ../../src/config/app_settings_defaults.cpp \
    ../../src/ui/floating_bar_style_selector.cpp \
    ../../src/ui/ui_style.cpp

HEADERS += \
    ../../src/config/app_settings_defaults.h \
    ../../src/ui/floating_bar_style_selector.h \
    ../../src/ui/ui_style.h
