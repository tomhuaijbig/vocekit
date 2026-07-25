QT += core gui widgets testlib

CONFIG += c++11 console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = hub_page_router_tests

SOURCES += \
    hub_page_router_tests.cpp \
    ../../src/ui/hub_page_router.cpp

HEADERS += \
    ../../src/ui/hub_page_router.h
