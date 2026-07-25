QT += core gui widgets testlib

CONFIG += c++11 console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = function_pages_controller_tests

SOURCES += \
    function_pages_controller_tests.cpp \
    ../../src/ui/function_pages_controller.cpp

HEADERS += \
    ../../src/domain/app_legacy_types.h \
    ../../src/domain/prompt_runtime_library.h \
    ../../src/ui/function_command_page.h \
    ../../src/ui/function_management_page.h \
    ../../src/ui/function_pages_access_factory.h \
    ../../src/ui/function_pages_controller.h \
    ../../src/ui/prompt_settings_adapter.h
