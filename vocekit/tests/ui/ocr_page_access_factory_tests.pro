QT += core gui widgets testlib

CONFIG += c++11 console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = ocr_page_access_factory_tests

SOURCES += \
    ocr_page_access_factory_tests.cpp \
    ../../src/capture/screenshot_types.cpp \
    ../../src/config/app_paths.cpp \
    ../../src/config/app_settings_defaults.cpp \
    ../../src/domain/function_settings.cpp \
    ../../src/input/hotkey_definitions.cpp \
    ../../src/result_flow_config.cpp \
    ../../src/ui/hub_settings_state.cpp \
    ../../src/ui/ocr_page_access_factory.cpp

HEADERS += \
    ../../src/config/app_settings_data.h \
    ../../src/domain/app_legacy_types.h \
    ../../src/domain/function_settings.h \
    ../../src/domain/history_types.h \
    ../../src/ocr/ocr_batch_queue.h \
    ../../src/ui/hub_settings_state.h \
    ../../src/ui/ocr_page_access_factory.h \
    ../../src/ui/ocr_page_controller.h
