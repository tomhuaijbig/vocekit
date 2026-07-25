QT += core testlib
CONFIG += console testcase c++11
CONFIG -= app_bundle
TEMPLATE = app
TARGET = model_catalog_tests

INCLUDEPATH += ../..

SOURCES += \
    model_catalog_tests.cpp \
    ../../src/config/app_settings_defaults.cpp \
    ../../src/config/secret_config.cpp \
    ../../src/config/secret_store.cpp \
    ../../src/file_utils.cpp \
    ../../src/providers/model_catalog.cpp

HEADERS += \
    ../../src/config/app_settings_defaults.h \
    ../../src/config/secret_config.h \
    ../../src/config/secret_store.h \
    ../../src/domain/app_legacy_types.h \
    ../../src/file_utils.h \
    ../../src/providers/model_catalog.h
