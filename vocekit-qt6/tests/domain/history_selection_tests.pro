QT += core testlib

CONFIG += console testcase c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = history_selection_tests

INCLUDEPATH += ../..

SOURCES += \
    history_selection_tests.cpp \
    ../../src/domain/history_selection.cpp \
    ../../src/domain/history_types.cpp

HEADERS += \
    ../../src/domain/history_selection.h \
    ../../src/domain/history_types.h \
    ../../src/recording/segmented_recording.h
