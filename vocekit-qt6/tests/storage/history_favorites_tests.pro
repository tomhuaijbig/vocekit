QT += core testlib
CONFIG += console testcase c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = history_favorites_tests

INCLUDEPATH += ../..

SOURCES += \
    history_favorites_tests.cpp \
    ../../src/file_utils.cpp \
    ../../src/storage/history_favorites.cpp

HEADERS += \
    ../../src/file_utils.h \
    ../../src/storage/history_favorites.h
