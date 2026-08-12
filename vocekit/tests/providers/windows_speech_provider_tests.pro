QT += core testlib
CONFIG += console testcase c++11
CONFIG -= app_bundle
TEMPLATE = app
TARGET = windows_speech_provider_tests

INCLUDEPATH += ../..

SOURCES += \
    windows_speech_provider_tests.cpp \
    ../../src/config/app_settings_defaults.cpp \
    ../../src/providers/windows_speech_helper_client.cpp \
    ../../src/providers/windows_speech_helper_protocol.cpp \
    ../../src/providers/windows_speech_provider.cpp \
    ../../src/runtime_log.cpp \
    ../../src/tasks/cancellation_token.cpp

HEADERS += \
    ../../src/config/app_settings_defaults.h \
    ../../src/domain/execution_types.h \
    ../../src/domain/operation_error.h \
    ../../src/providers/provider_types.h \
    ../../src/providers/speech_provider.h \
    ../../src/providers/windows_speech_helper_client.h \
    ../../src/providers/windows_speech_helper_protocol.h \
    ../../src/providers/windows_speech_provider.h \
    ../../src/runtime_log.h \
    ../../src/tasks/cancellation_token.h
