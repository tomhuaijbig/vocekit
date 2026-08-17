QT += core gui testlib
CONFIG += console testcase c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = domain_types_tests

INCLUDEPATH += ../..

SOURCES += \
    domain_types_tests.cpp \
    ../../src/config/app_settings_defaults.cpp \
    ../../src/domain/function_settings.cpp \
    ../../src/domain/history_record_builder.cpp \
    ../../src/domain/history_types.cpp \
    ../../src/domain/voice_run_formatter.cpp \
    ../../src/domain/voice_run_context.cpp \
    ../../src/domain/voice_run_planner.cpp \
    ../../src/output/result_output_router.cpp \
    ../../src/result_flow_config.cpp

HEADERS += \
    ../../src/domain/execution_types.h \
    ../../src/domain/function_settings.h \
    ../../src/domain/history_record_builder.h \
    ../../src/domain/history_types.h \
    ../../src/domain/operation_error.h \
    ../../src/domain/voice_run_formatter.h \
    ../../src/domain/voice_run_context.h \
    ../../src/domain/voice_run_planner.h \
    ../../src/output/result_output_router.h \
    ../../src/ocr/ocr_types.h \
    ../../src/recording/segmented_recording.h \
    ../../src/result_flow_config.h
