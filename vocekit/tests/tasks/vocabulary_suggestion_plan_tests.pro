QT += core testlib
CONFIG += console testcase c++11
CONFIG -= app_bundle
TEMPLATE = app
TARGET = vocabulary_suggestion_plan_tests

INCLUDEPATH += ../..

SOURCES += \
    vocabulary_suggestion_plan_tests.cpp \
    ../../src/config/app_paths.cpp \
    ../../src/tasks/vocabulary_suggestion_plan.cpp \
    ../../src/storage/vocabulary_store.cpp \
    ../../src/file_utils.cpp \
    ../../src/result_flow_config.cpp

HEADERS += \
    ../../src/tasks/vocabulary_suggestion_plan.h \
    ../../src/storage/vocabulary_store.h \
    ../../src/domain/app_legacy_types.h \
    ../../src/result_flow_config.h
