QT += core gui widgets testlib
CONFIG += c++17 console testcase
CONFIG -= app_bundle
TEMPLATE = app
TARGET = history_list_view_tests

INCLUDEPATH += ../..

SOURCES += \
    history_list_view_tests.cpp \
    ../../src/domain/history_filter.cpp \
    ../../src/domain/history_modes.cpp \
    ../../src/domain/history_text.cpp \
    ../../src/ui/history_list_view.cpp \
    ../../src/ui/ui_style.cpp

HEADERS += \
    ../../src/domain/app_legacy_types.h \
    ../../src/domain/history_filter.h \
    ../../src/domain/history_modes.h \
    ../../src/domain/history_text.h \
    ../../src/domain/history_types.h \
    ../../src/ui/history_list_view.h \
    ../../src/ui/ui_style.h
