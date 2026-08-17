QT += core gui widgets testlib

CONFIG += c++17 console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = result_choice_popup_header_tests

SOURCES += \
    result_choice_popup_header_tests.cpp \
    ../../src/config/app_settings_defaults.cpp \
    ../../src/config/secret_config.cpp \
    ../../src/config/secret_store.cpp \
    ../../src/file_utils.cpp \
    ../../src/output/clipboard_writer.cpp \
    ../../src/providers/model_catalog.cpp \
    ../../src/result_flow_config.cpp \
    ../../src/ui/app_dialogs.cpp \
    ../../src/ui/attention_message.cpp \
    ../../src/ui/result_choice_popup.cpp \
    ../../src/ui/ui_style.cpp

HEADERS += \
    ../../src/config/app_settings_defaults.h \
    ../../src/config/secret_config.h \
    ../../src/config/secret_store.h \
    ../../src/domain/app_legacy_types.h \
    ../../src/file_utils.h \
    ../../src/output/clipboard_writer.h \
    ../../src/providers/model_catalog.h \
    ../../src/result_flow_config.h \
    ../../src/ui/app_dialogs.h \
    ../../src/ui/attention_message.h \
    ../../src/ui/result_choice_popup.h \
    ../../src/ui/screen_position.h \
    ../../src/ui/ui_style.h
