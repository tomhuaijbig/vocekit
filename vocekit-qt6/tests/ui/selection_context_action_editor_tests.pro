QT += core gui widgets testlib

CONFIG += c++17 console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = selection_context_action_editor_tests

INCLUDEPATH += ../..

SOURCES += \
    selection_context_action_editor_tests.cpp \
    ../../src/domain/selection_context_actions.cpp \
    ../../src/ui/selection_context_action_editor.cpp

HEADERS += \
    ../../src/config/selection_context_action_customization.h \
    ../../src/domain/selection_context_actions.h \
    ../../src/ui/selection_context_action_editor.h
