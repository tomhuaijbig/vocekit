QT += core gui widgets testlib

CONFIG += c++11 console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = hub_navigation_controller_tests

SOURCES += \
    hub_navigation_controller_tests.cpp \
    ../../src/ui/command_center_shell.cpp \
    ../../src/ui/command_search_router.cpp \
    ../../src/ui/hub_navigation_controller.cpp \
    ../../src/ui/hub_page_router.cpp

HEADERS += \
    ../../src/ui/command_center_shell.h \
    ../../src/ui/hub_navigation_controller.h \
    ../../src/ui/hub_page_router.h
