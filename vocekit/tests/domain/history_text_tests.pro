QT += core testlib

CONFIG += console testcase c++11
CONFIG -= app_bundle
TEMPLATE = app
TARGET = history_text_tests

INCLUDEPATH += ../..

SOURCES += \
    history_text_tests.cpp \
    ../../src/domain/history_text.cpp

HEADERS += \
    ../../src/domain/history_text.h \
    ../../src/domain/history_types.h
