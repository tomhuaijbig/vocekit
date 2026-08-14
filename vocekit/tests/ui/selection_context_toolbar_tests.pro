QT += core gui widgets testlib
CONFIG += console testcase c++11
CONFIG -= app_bundle
TEMPLATE = app
TARGET = selection_context_toolbar_tests

INCLUDEPATH += ../..

SOURCES += \
    selection_context_toolbar_tests.cpp \
    ../../src/ui/selection_context_placement.cpp \
    ../../src/ui/selection_context_toolbar.cpp \
    ../../src/domain/selection_context_actions.cpp

HEADERS += \
    ../../src/ui/selection_context_placement.h \
    ../../src/ui/selection_context_toolbar.h \
    ../../src/domain/selection_context_actions.h \
    ../../src/input/selection_snapshot.h
