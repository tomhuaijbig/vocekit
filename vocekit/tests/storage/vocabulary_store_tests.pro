QT += core testlib
CONFIG += console testcase c++11
CONFIG -= app_bundle
TEMPLATE = app
TARGET = vocabulary_store_tests

INCLUDEPATH += ../..

SOURCES += \
    vocabulary_store_tests.cpp \
    ../../src/config/app_paths.cpp \
    ../../src/file_utils.cpp \
    ../../src/storage/vocabulary_store.cpp

HEADERS += \
    ../../src/config/app_paths.h \
    ../../src/domain/app_legacy_types.h \
    ../../src/file_utils.h \
    ../../src/storage/vocabulary_store.h
