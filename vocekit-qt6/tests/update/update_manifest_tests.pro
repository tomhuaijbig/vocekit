QT += core testlib

CONFIG += c++17 console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = update_manifest_tests

INCLUDEPATH += ../..

SOURCES += \
    update_manifest_tests.cpp \
    ../../src/update/semantic_version.cpp \
    ../../src/update/update_manifest.cpp

HEADERS += \
    ../../src/update/semantic_version.h \
    ../../src/update/update_manifest.h
