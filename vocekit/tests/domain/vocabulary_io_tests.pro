QT += core testlib

CONFIG += console testcase c++11
CONFIG -= app_bundle
TEMPLATE = app
TARGET = vocabulary_io_tests

INCLUDEPATH += ../..

SOURCES += \
    vocabulary_io_tests.cpp \
    ../../src/config/app_paths.cpp \
    ../../src/file_utils.cpp \
    ../../src/storage/vocabulary_store.cpp \
    ../../src/domain/vocabulary_io.cpp

HEADERS += \
    ../../src/config/app_paths.h \
    ../../src/domain/app_legacy_types.h \
    ../../src/domain/vocabulary_io.h \
    ../../src/file_utils.h \
    ../../src/storage/vocabulary_store.h
