QT += core gui testlib
CONFIG += console testcase c++11
CONFIG -= app_bundle
TEMPLATE = app
TARGET = selection_context_toolbar_tests

INCLUDEPATH += ../..

SOURCES += \
    selection_context_toolbar_tests.cpp \
    ../../src/ui/selection_context_placement.cpp

HEADERS += \
    ../../src/ui/selection_context_placement.h
