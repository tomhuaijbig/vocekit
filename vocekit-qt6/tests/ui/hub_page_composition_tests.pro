QT += core gui widgets testlib

CONFIG += c++17 console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = hub_page_composition_tests

SOURCES += \
    hub_page_composition_tests.cpp \
    ../../src/ui/hub_page_composition.cpp \
    ../../src/ui/hub_page_router.cpp

HEADERS += \
    ../../src/ui/hub_page_composition.h \
    ../../src/ui/hub_page_router.h
