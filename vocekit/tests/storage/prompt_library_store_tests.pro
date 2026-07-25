QT += core testlib

CONFIG += c++11 console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = prompt_library_store_tests

SOURCES += \
    prompt_library_store_tests.cpp \
    ../../src/config/app_paths.cpp \
    ../../src/storage/prompt_library_store.cpp

HEADERS += \
    ../../src/domain/app_legacy_types.h \
    ../../src/domain/operation_error.h \
    ../../src/storage/prompt_library_store.h
