QT += core network websockets testlib
CONFIG += console testcase c++11
CONFIG -= app_bundle
TEMPLATE = app
TARGET = xfyun_speech_provider_tests

INCLUDEPATH += ../..

SOURCES += \
    xfyun_speech_provider_tests.cpp \
    ../../src/api/api_client_utils.cpp \
    ../../src/config/app_settings_defaults.cpp \
    ../../src/config/secret_config.cpp \
    ../../src/config/secret_store.cpp \
    ../../src/file_utils.cpp \
    ../../src/providers/network_error_messages.cpp \
    ../../src/providers/provider_websocket_transport.cpp \
    ../../src/providers/xfyun_speech_provider.cpp \
    ../../src/result_flow_config.cpp \
    ../../src/runtime_log.cpp \
    ../../src/tasks/cancellation_token.cpp

HEADERS += \
    ../../src/api/api_client_utils.h \
    ../../src/config/app_settings_defaults.h \
    ../../src/config/secret_config.h \
    ../../src/config/secret_store.h \
    ../../src/domain/app_legacy_types.h \
    ../../src/domain/execution_types.h \
    ../../src/domain/operation_error.h \
    ../../src/file_utils.h \
    ../../src/providers/network_error_messages.h \
    ../../src/providers/provider_types.h \
    ../../src/providers/provider_websocket_transport.h \
    ../../src/providers/speech_provider.h \
    ../../src/providers/xfyun_speech_provider.h \
    ../../src/result_flow_config.h \
    ../../src/runtime_log.h \
    ../../src/tasks/cancellation_token.h
