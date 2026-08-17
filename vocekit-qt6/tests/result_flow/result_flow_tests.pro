QT += testlib core network
CONFIG += console c++17 testcase
TEMPLATE = app
TARGET = result_flow_tests

SOURCES += \
    result_flow_tests.cpp \
    ../../src/config/app_settings_defaults.cpp \
    ../../src/domain/voice_function_execution_pipeline.cpp \
    ../../src/domain/voice_input_processing_pipeline.cpp \
    ../../src/domain/voice_result_completion_executor.cpp \
    ../../src/domain/voice_result_rerun_executor.cpp \
    ../../src/domain/voice_result_stream_executor.cpp \
    ../../src/domain/voice_run_context.cpp \
    ../../src/domain/voice_run_formatter.cpp \
    ../../src/output/result_output_router.cpp \
    ../../src/output/voice_result_popup_builder.cpp \
    ../../src/output/voice_result_output_dispatcher.cpp \
    ../../src/result_flow_config.cpp

HEADERS += \
    ../../src/config/app_settings_defaults.h \
    ../../src/domain/voice_function_execution_pipeline.h \
    ../../src/domain/voice_input_processing_pipeline.h \
    ../../src/domain/voice_result_completion_executor.h \
    ../../src/domain/voice_result_rerun_executor.h \
    ../../src/domain/voice_result_stream_executor.h \
    ../../src/domain/voice_run_context.h \
    ../../src/domain/voice_run_formatter.h \
    ../../src/output/result_output_router.h \
    ../../src/output/voice_result_popup_builder.h \
    ../../src/output/voice_result_output_dispatcher.h \
    ../../src/result_flow_config.h
