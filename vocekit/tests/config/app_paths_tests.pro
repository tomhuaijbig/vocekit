QT += core testlib
CONFIG += console testcase c++11
CONFIG -= app_bundle
TEMPLATE = app
TARGET = app_paths_tests

INCLUDEPATH += ../..

SOURCES += \
    app_paths_tests.cpp \
    ../../src/config/app_paths.cpp

HEADERS += \
    ../../src/config/app_paths.h
