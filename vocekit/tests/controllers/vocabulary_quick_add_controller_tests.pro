QT += core testlib

CONFIG += c++11 console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = vocabulary_quick_add_controller_tests

SOURCES += \
    vocabulary_quick_add_controller_tests.cpp \
    ../../src/config/app_settings_defaults.cpp \
    ../../src/controllers/vocabulary_quick_add_controller.cpp

HEADERS += \
    ../../src/config/app_settings_data.h \
    ../../src/controllers/vocabulary_quick_add_controller.h \
    ../../src/domain/app_legacy_types.h \
    ../../src/tasks/vocabulary_suggestion_task.h
