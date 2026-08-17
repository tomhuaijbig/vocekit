QT += core gui widgets testlib

CONFIG += c++17 console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = faq_panel_performance_tests

SOURCES += \
    faq_panel_performance_tests.cpp \
    ../../src/ui/faq_panel.cpp \
    ../../src/ui/attention_message.cpp \
    ../../src/ui/ui_style.cpp

HEADERS += \
    ../../src/ui/faq_panel.h \
    ../../src/ui/attention_message.h \
    ../../src/ui/ui_style.h
