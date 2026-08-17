QT += core gui widgets testlib
CONFIG += console testcase c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = selection_result_card_tests

INCLUDEPATH += ../..

SOURCES += \
    selection_result_card_tests.cpp \
    ../../src/ui/selection_result_card.cpp

HEADERS += \
    ../../src/ui/selection_result_card.h \
    ../../src/input/selection_snapshot.h
