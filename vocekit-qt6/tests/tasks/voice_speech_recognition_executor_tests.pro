QT += testlib core network concurrent
CONFIG += console c++17 testcase
TEMPLATE = app
TARGET = voice_speech_recognition_executor_tests

SOURCES += \
    voice_speech_recognition_executor_tests.cpp \
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
    ../../src/tasks/voice_speech_recognition_executor.cpp

HEADERS += \
    ../../src/recording/segmented_recording.h \
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
    ../../src/tasks/voice_speech_recognition_executor.h
