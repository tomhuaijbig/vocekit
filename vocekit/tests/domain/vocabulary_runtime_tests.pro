QT += core testlib
CONFIG += console testcase c++11
CONFIG -= app_bundle
TEMPLATE = app
TARGET = vocabulary_runtime_tests

INCLUDEPATH += ../..

SOURCES += \
    vocabulary_runtime_tests.cpp \
    ../../src/domain/vocabulary_runtime.cpp \
    ../../src/storage/vocabulary_store.cpp \
    ../../src/config/app_paths.cpp \
    ../../src/file_utils.cpp \
    ../../src/result_flow_config.cpp

HEADERS += \
    ../../src/config/app_settings_data.h \
    ../../src/domain/vocabulary_runtime.h \
    ../../src/storage/vocabulary_store.h \
    ../../src/result_flow_config.h
