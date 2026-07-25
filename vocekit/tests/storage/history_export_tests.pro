QT += core testlib
CONFIG += console testcase c++11
CONFIG -= app_bundle
TEMPLATE = app
TARGET = history_export_tests

INCLUDEPATH += ../..

SOURCES += \
    history_export_tests.cpp \
    ../../src/domain/history_record_builder.cpp \
    ../../src/domain/history_text.cpp \
    ../../src/domain/history_types.cpp \
    ../../src/domain/voice_history_request_builder.cpp \
    ../../src/domain/voice_run_context.cpp \
    ../../src/file_utils.cpp \
    ../../src/storage/history_export.cpp \
    ../../src/storage/history_store.cpp

HEADERS += \
    ../../src/domain/history_record_builder.h \
    ../../src/domain/history_text.h \
    ../../src/domain/history_types.h \
    ../../src/domain/voice_history_request_builder.h \
    ../../src/domain/voice_run_context.h \
    ../../src/file_utils.h \
    ../../src/ocr/ocr_types.h \
    ../../src/recording/segmented_recording.h \
    ../../src/result_flow_config.h \
    ../../src/storage/history_export.h \
    ../../src/storage/history_store.h
