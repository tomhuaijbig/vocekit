QT += core gui widgets testlib

CONFIG += c++11 console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = function_editor_dialog_access_factory_tests

SOURCES += \
    function_editor_dialog_access_factory_tests.cpp \
    ../../src/ui/function_editor_dialog_access_factory.cpp

HEADERS += \
    ../../src/config/app_settings_data.h \
    ../../src/domain/app_legacy_types.h \
    ../../src/domain/function_settings.h \
    ../../src/domain/prompt_runtime_library.h \
    ../../src/ui/function_editor_dialog.h \
    ../../src/ui/function_editor_dialog_access_factory.h \
    ../../src/ui/prompt_settings_adapter.h
