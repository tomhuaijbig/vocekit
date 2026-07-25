QT += core network testlib
CONFIG += console testcase c++11
CONFIG -= app_bundle
TEMPLATE = app
TARGET = vocabulary_diagnostic_task_tests

INCLUDEPATH += ../..

SOURCES += \
    vocabulary_diagnostic_task_tests.cpp \
    ../../src/config/app_paths.cpp \
    ../../src/config/app_settings_defaults.cpp \
    ../../src/file_utils.cpp \
    ../../src/storage/vocabulary_store.cpp \
    ../../src/tasks/cancellation_token.cpp \
    ../../src/tasks/diagnostic_helpers.cpp \
    ../../src/tasks/vocabulary_diagnostic_task.cpp

HEADERS += \
    ../../src/config/app_paths.h \
    ../../src/config/app_settings_data.h \
    ../../src/config/app_settings_defaults.h \
    ../../src/domain/app_legacy_types.h \
    ../../src/file_utils.h \
    ../../src/storage/vocabulary_store.h \
    ../../src/tasks/cancellation_token.h \
    ../../src/tasks/diagnostic_helpers.h \
    ../../src/tasks/vocabulary_diagnostic_task.h
