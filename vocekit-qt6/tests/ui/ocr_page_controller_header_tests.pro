QT += core gui widgets testlib

CONFIG += c++17 console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = ocr_page_controller_header_tests

SOURCES += ocr_page_controller_header_tests.cpp

HEADERS += \
    ../../src/config/app_settings_data.h \
    ../../src/domain/app_legacy_types.h \
    ../../src/domain/function_settings.h \
    ../../src/ocr/ocr_batch_queue.h \
    ../../src/ocr/ocr_types.h \
    ../../src/ui/ocr_page_controller.h
