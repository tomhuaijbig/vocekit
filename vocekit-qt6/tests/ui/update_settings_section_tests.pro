QT += core gui widgets network testlib

CONFIG += c++17 console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = update_settings_section_tests

INCLUDEPATH += ../..

SOURCES += \
    update_settings_section_tests.cpp \
    ../../src/ui/update_settings_section.cpp \
    ../../src/ui/ui_style.cpp \
    ../../src/update/semantic_version.cpp \
    ../../src/update/update_manifest.cpp \
    ../../src/update/update_service.cpp

HEADERS += \
    ../../src/ui/update_settings_section.h \
    ../../src/ui/ui_style.h \
    ../../src/update/semantic_version.h \
    ../../src/update/update_manifest.h \
    ../../src/update/update_service.h
