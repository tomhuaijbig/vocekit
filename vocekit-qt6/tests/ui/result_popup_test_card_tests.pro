QT += core gui widgets testlib

CONFIG += c++17 console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = result_popup_test_card_tests

SOURCES += \
    result_popup_test_card_tests.cpp \
    ../../src/ui/result_popup_test_card.cpp \
    ../../src/ui/ui_style.cpp

HEADERS += \
    ../../src/ui/result_popup_test_card.h \
    ../../src/ui/ui_style.h
