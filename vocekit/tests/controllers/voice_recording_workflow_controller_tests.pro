QT += core testlib widgets multimedia concurrent

CONFIG += c++11 console testcase
CONFIG -= app_bundle
DEFINES += VOCEKIT_TESTING

TEMPLATE = app
TARGET = voice_recording_workflow_controller_tests

SOURCES += \
    voice_recording_workflow_controller_tests.cpp \
    ../../src/config/app_settings_defaults.cpp \
    ../../src/providers/windows_speech_helper_protocol.cpp \
    ../../src/controllers/voice_recording_workflow_controller.cpp \
    ../../src/domain/voice_run_session.cpp \
    ../../src/recording/segmented_recording.cpp \
    ../../src/recording/voice_long_recording_session.cpp \
    ../../src/recording/voice_recording_capture.cpp \
    ../../src/recording/voice_recording_coordinator.cpp \
    ../../src/recording/voice_recording_countdown.cpp \
    ../../src/recording/voice_recording_lifecycle.cpp \
    ../../src/tasks/cancellation_token.cpp \
    ../../src/tasks/voice_long_recording_completion_executor.cpp \
    ../../src/tasks/voice_long_recording_recognition_coordinator.cpp \
    ../../src/tasks/voice_long_recording_result_builder.cpp \
    ../../src/tasks/voice_long_recording_segment_executor.cpp \
    ../../src/tasks/voice_recording_completion_executor.cpp \
    ../../src/tasks/voice_speech_recognition_executor.cpp \
    ../../src/ui/floating_bar.cpp \
    ../../src/ui/floating_bar_surface.cpp \
    ../../src/ui/attention_message.cpp \
    ../../src/ui/ui_style.cpp

HEADERS += \
    ../../src/controllers/voice_recording_workflow_controller.h \
    ../../src/config/app_settings_defaults.h \
    ../../src/providers/windows_speech_helper_protocol.h \
    ../../src/domain/function_flow_compiler.h \
    ../../src/domain/function_flow_runtime_types.h \
    ../../src/domain/voice_run_session.h \
    ../../src/recording/segmented_recording.h \
    ../../src/recording/voice_audio_recorder_adapter.h \
    ../../src/recording/voice_long_recording_session.h \
    ../../src/recording/voice_recording_capture.h \
    ../../src/recording/voice_recording_coordinator.h \
    ../../src/recording/voice_recording_countdown.h \
    ../../src/recording/voice_recording_lifecycle.h \
    ../../src/tasks/cancellation_token.h \
    ../../src/tasks/speech_recognition_task.h \
    ../../src/tasks/voice_long_recording_completion_executor.h \
    ../../src/tasks/voice_long_recording_recognition_coordinator.h \
    ../../src/tasks/voice_long_recording_result_builder.h \
    ../../src/tasks/voice_long_recording_segment_executor.h \
    ../../src/tasks/voice_recording_completion_executor.h \
    ../../src/tasks/voice_speech_recognition_executor.h \
    ../../src/ui/floating_bar.h \
    ../../src/ui/floating_bar_surface.h \
    ../../src/ui/attention_message.h \
    ../../src/ui/ui_style.h
