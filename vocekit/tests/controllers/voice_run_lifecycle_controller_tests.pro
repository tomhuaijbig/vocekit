QT += core testlib

CONFIG += c++11 console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = voice_run_lifecycle_controller_tests

SOURCES += \
    voice_run_lifecycle_controller_tests.cpp \
    ../../src/config/app_settings_defaults.cpp \
    ../../src/controllers/voice_run_lifecycle_controller.cpp \
    ../../src/domain/voice_run_context.cpp \
    ../../src/domain/voice_run_executor.cpp \
    ../../src/domain/voice_run_formatter.cpp \
    ../../src/domain/voice_run_planner.cpp \
    ../../src/domain/voice_run_session.cpp \
    ../../src/result_flow_config.cpp \
    ../../src/tasks/cancellation_token.cpp

HEADERS += \
    ../../src/config/app_settings_data.h \
    ../../src/controllers/voice_run_lifecycle_controller.h \
    ../../src/domain/voice_history_recorder.h \
    ../../src/domain/voice_run_context.h \
    ../../src/domain/voice_run_executor.h \
    ../../src/domain/voice_run_session.h \
    ../../src/tasks/cancellation_token.h \
    ../../src/tasks/voice_model_processing_task.h
