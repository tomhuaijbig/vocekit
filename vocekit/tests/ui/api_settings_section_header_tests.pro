QT += core gui widgets network concurrent testlib

CONFIG += c++11 console testcase
CONFIG -= app_bundle
DEFINES += VOCEKIT_TESTING

TEMPLATE = app
TARGET = api_settings_section_header_tests

SOURCES += \
    api_settings_section_header_tests.cpp \
    ../../src/api/api_client_utils.cpp \
    ../../src/config/app_paths.cpp \
    ../../src/config/app_settings_defaults.cpp \
    ../../src/config/baidu_sample_parser.cpp \
    ../../src/config/secret_config.cpp \
    ../../src/config/secret_store.cpp \
    ../../src/file_utils.cpp \
    ../../src/providers/network_error_messages.cpp \
    ../../src/providers/network_request_executor.cpp \
    ../../src/providers/openai_compatible_model_provider.cpp \
    ../../src/providers/provider_network_transport.cpp \
    ../../src/providers/windows_speech_helper_client.cpp \
    ../../src/providers/windows_speech_helper_protocol.cpp \
    ../../src/result_flow_config.cpp \
    ../../src/runtime_log.cpp \
    ../../src/tasks/cancellation_token.cpp \
    ../../src/tasks/diagnostic_task_runner.cpp \
    ../../src/ui/api_settings_section.cpp \
    ../../src/ui/app_dialogs.cpp \
    ../../src/ui/attention_message.cpp \
    ../../src/ui/custom_model_dialog_support.cpp \
    ../../src/ui/ui_style.cpp \
    ../../src/ui/windows_speech_settings_card.cpp

HEADERS += \
    ../../src/config/secret_config.h \
    ../../src/ui/attention_message.h \
    ../../src/ui/api_settings_section.h
