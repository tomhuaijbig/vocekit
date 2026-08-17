QT += core gui widgets concurrent testlib
CONFIG += console testcase c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = clipboard_writer_tests

INCLUDEPATH += ../..

SOURCES += \
    clipboard_writer_tests.cpp \
    ../../src/output/clipboard_writer.cpp

HEADERS += \
    ../../src/output/clipboard_writer.h
