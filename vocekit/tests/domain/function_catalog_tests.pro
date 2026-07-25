QT += core testlib
CONFIG += console testcase c++11
CONFIG -= app_bundle
TEMPLATE = app
TARGET = function_catalog_tests

INCLUDEPATH += ../..

SOURCES += \
    function_catalog_tests.cpp \
    ../../src/domain/function_catalog.cpp \
    ../../src/input/hotkey_definitions.cpp \
    ../../src/result_flow_config.cpp

HEADERS += \
    ../../src/config/app_settings_data.h \
    ../../src/domain/app_legacy_types.h \
    ../../src/domain/function_catalog.h \
    ../../src/domain/function_settings.h \
    ../../src/input/hotkey_definitions.h \
    ../../src/result_flow_config.h
