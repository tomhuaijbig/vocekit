QT += core gui network testlib concurrent

CONFIG += c++11 console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = ocr_core_tests

SOURCES += \
    ocr_core_tests.cpp \
    ../../src/capture/screenshot_types.cpp \
    ../../src/config/app_paths.cpp \
    ../../src/config/app_settings_defaults.cpp \
    ../../src/config/app_settings_json.cpp \
    ../../src/config/app_settings_store.cpp \
    ../../src/config/baidu_sample_parser.cpp \
    ../../src/config/secret_config.cpp \
    ../../src/config/secret_store.cpp \
    ../../src/file_utils.cpp \
    ../../src/input/hotkey_definitions.cpp \
    ../../src/domain/function_settings.cpp \
    ../../src/ocr/ocr_batch_queue.cpp \
    ../../src/ocr/ocr_batch_text.cpp \
    ../../src/ocr/ocr_cloud_client.cpp \
    ../../src/ocr/ocr_helper_process.cpp \
    ../../src/ocr/ocr_manager.cpp \
    ../../src/ocr/screenshot_ocr_config.cpp \
    ../../src/platform/windows_autostart.cpp \
    ../../src/providers/model_catalog.cpp \
    ../../src/recording/segmented_recording.cpp \
    ../../src/result_flow_config.cpp \
    ../../src/tasks/cancellation_token.cpp

HEADERS += \
    ../../src/capture/screenshot_types.h \
    ../../src/config/app_paths.h \
    ../../src/config/app_settings_data.h \
    ../../src/config/app_settings_defaults.h \
    ../../src/config/app_settings_json.h \
    ../../src/config/app_settings_store.h \
    ../../src/config/baidu_sample_parser.h \
    ../../src/config/secret_config.h \
    ../../src/config/secret_store.h \
    ../../src/file_utils.h \
    ../../src/input/hotkey_definitions.h \
    ../../src/domain/function_settings.h \
    ../../src/ocr/ocr_cloud_client.h \
    ../../src/ocr/ocr_batch_queue.h \
    ../../src/ocr/ocr_batch_text.h \
    ../../src/ocr/ocr_helper_process.h \
    ../../src/ocr/ocr_manager.h \
    ../../src/ocr/screenshot_ocr_config.h \
    ../../src/ocr/ocr_types.h \
    ../../src/platform/windows_autostart.h \
    ../../src/providers/model_catalog.h \
    ../../src/recording/segmented_recording.h \
    ../../src/result_flow_config.h \
    ../../src/tasks/cancellation_token.h
