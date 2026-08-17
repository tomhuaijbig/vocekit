QT += core testlib

CONFIG += console testcase c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = vocabulary_candidates_tests

INCLUDEPATH += ../..

SOURCES += \
    vocabulary_candidates_tests.cpp \
    ../../src/config/app_paths.cpp \
    ../../src/file_utils.cpp \
    ../../src/result_flow_config.cpp \
    ../../src/storage/vocabulary_store.cpp \
    ../../src/domain/history_modes.cpp \
    ../../src/domain/vocabulary_candidates.cpp

HEADERS += \
    ../../src/config/app_paths.h \
    ../../src/domain/app_legacy_types.h \
    ../../src/domain/history_modes.h \
    ../../src/domain/history_types.h \
    ../../src/domain/vocabulary_candidates.h \
    ../../src/file_utils.h \
    ../../src/result_flow_config.h \
    ../../src/storage/vocabulary_store.h
