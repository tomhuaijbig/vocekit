QT += core gui widgets testlib

CONFIG += c++17 console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = history_entry_actions_controller_tests

SOURCES += \
    history_entry_actions_controller_tests.cpp \
    ../../src/config/app_paths.cpp \
    ../../src/domain/history_record_builder.cpp \
    ../../src/file_utils.cpp \
    ../../src/storage/history_favorites.cpp \
    ../../src/storage/history_paths.cpp \
    ../../src/storage/history_record_service.cpp \
    ../../src/storage/history_store.cpp \
    ../../src/ui/app_dialogs.cpp \
    ../../src/ui/attention_message.cpp \
    ../../src/ui/history_entry_actions_controller.cpp \
    ../../src/ui/ui_style.cpp

HEADERS += \
    ../../src/config/app_paths.h \
    ../../src/domain/history_record_builder.h \
    ../../src/domain/history_types.h \
    ../../src/file_utils.h \
    ../../src/storage/history_favorites.h \
    ../../src/storage/history_paths.h \
    ../../src/storage/history_record_service.h \
    ../../src/storage/history_store.h \
    ../../src/ui/app_dialogs.h \
    ../../src/ui/attention_message.h \
    ../../src/ui/history_entry_actions_controller.h \
    ../../src/ui/ui_style.h
