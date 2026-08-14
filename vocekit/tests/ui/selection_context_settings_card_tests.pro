QT += core gui widgets testlib

CONFIG += c++11 console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = selection_context_settings_card_tests

INCLUDEPATH += ../..

SOURCES += \
    selection_context_settings_card_tests.cpp \
    ../../src/domain/selection_context_actions.cpp \
    ../../src/ui/selection_context_settings_card.cpp \
    ../../src/ui/ui_style.cpp

HEADERS += \
    ../../src/config/app_settings_data.h \
    ../../src/domain/selection_context_actions.h \
    ../../src/ui/selection_context_settings_card.h \
    ../../src/ui/ui_style.h
