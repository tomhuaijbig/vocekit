QT += core testlib

CONFIG += c++17 console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = command_search_router_tests

SOURCES += \
    command_search_router_tests.cpp \
    ../../src/ui/command_search_router.cpp

HEADERS += \
    ../../src/ui/command_search_router.h
