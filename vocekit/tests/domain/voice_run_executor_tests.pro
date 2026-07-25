QT += core testlib
CONFIG += console testcase c++11
CONFIG -= app_bundle
TEMPLATE = app
TARGET = voice_run_executor_tests

INCLUDEPATH += ../..

SOURCES += \
    voice_run_executor_tests.cpp \
    ../../src/domain/voice_run_context.cpp \
    ../../src/domain/voice_run_executor.cpp \
    ../../src/domain/voice_run_planner.cpp \
    ../../src/tasks/cancellation_token.cpp

HEADERS += \
    ../../src/config/app_settings_data.h \
    ../../src/domain/function_settings.h \
    ../../src/domain/prompt_runtime_library.h \
    ../../src/domain/voice_run_context.h \
    ../../src/domain/voice_run_executor.h \
    ../../src/domain/voice_run_planner.h \
    ../../src/tasks/cancellation_token.h \
    ../../src/tasks/voice_model_processing_task.h \
    ../../src/tasks/voice_model_runtime_settings.h
