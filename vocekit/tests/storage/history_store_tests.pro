QT += core testlib
CONFIG += console testcase c++11
CONFIG -= app_bundle
TEMPLATE = app
TARGET = history_store_tests
DEFINES += VOCEKIT_HISTORY_STORE_TEST_ACCESS

INCLUDEPATH += ../..

SOURCES += \
    history_store_tests.cpp \
    ../../src/domain/history_record_builder.cpp \
    ../../src/domain/history_types.cpp \
    ../../src/domain/voice_history_recorder.cpp \
    ../../src/domain/voice_history_request_builder.cpp \
    ../../src/domain/voice_run_context.cpp \
    ../../src/file_utils.cpp \
    ../../src/storage/history_favorites.cpp \
    ../../src/storage/history_record_service.cpp \
    ../../src/storage/history_store.cpp

HEADERS += \
    ../../src/domain/history_record_builder.h \
    ../../src/domain/history_types.h \
    ../../src/domain/voice_history_recorder.h \
    ../../src/domain/voice_history_request_builder.h \
    ../../src/domain/voice_run_context.h \
    ../../src/file_utils.h \
    ../../src/ocr/ocr_types.h \
    ../../src/recording/segmented_recording.h \
    ../../src/result_flow_config.h \
    ../../src/storage/history_favorites.h \
    ../../src/storage/history_record_service.h \
    ../../src/storage/history_store.h
