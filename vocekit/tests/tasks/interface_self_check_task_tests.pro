QT += core gui network websockets concurrent testlib
CONFIG += console testcase c++11
CONFIG -= app_bundle
TEMPLATE = app
TARGET = interface_self_check_task_tests

INCLUDEPATH += ../..

SOURCES += \
    interface_self_check_task_tests.cpp \
    ../../src/api/api_client_utils.cpp \
    ../../src/config/app_settings_defaults.cpp \
    ../../src/config/secret_config.cpp \
    ../../src/config/secret_store.cpp \
    ../../src/file_utils.cpp \
    ../../src/ocr/ocr_cloud_client.cpp \
    ../../src/ocr/ocr_helper_process.cpp \
    ../../src/ocr/ocr_manager.cpp \
    ../../src/providers/built_in_provider_factory.cpp \
    ../../src/providers/baidu_speech_provider.cpp \
    ../../src/providers/claude_model_provider.cpp \
    ../../src/providers/custom_speech_provider.cpp \
    ../../src/providers/deepseek_model_provider.cpp \
    ../../src/providers/network_error_messages.cpp \
    ../../src/providers/network_request_executor.cpp \
    ../../src/providers/openai_compatible_model_provider.cpp \
    ../../src/providers/provider_network_transport.cpp \
    ../../src/providers/provider_websocket_transport.cpp \
    ../../src/providers/provider_registry.cpp \
    ../../src/providers/xfyun_speech_protocol.cpp \
    ../../src/providers/xfyun_speech_provider.cpp \
    ../../src/result_flow_config.cpp \
    ../../src/runtime_log.cpp \
    ../../src/tasks/cancellation_token.cpp \
    ../../src/tasks/diagnostic_task_runner.cpp \
    ../../src/tasks/diagnostic_helpers.cpp \
    ../../src/tasks/interface_self_check_task.cpp \
    ../../src/tasks/network_diagnostics_task.cpp

HEADERS += \
    ../../src/api/api_client_utils.h \
    ../../src/config/app_settings_defaults.h \
    ../../src/config/secret_config.h \
    ../../src/config/secret_store.h \
    ../../src/domain/app_legacy_types.h \
    ../../src/domain/execution_types.h \
    ../../src/domain/operation_error.h \
    ../../src/file_utils.h \
    ../../src/ocr/ocr_cloud_client.h \
    ../../src/ocr/ocr_helper_process.h \
    ../../src/ocr/ocr_manager.h \
    ../../src/ocr/ocr_types.h \
    ../../src/providers/built_in_provider_factory.h \
    ../../src/providers/baidu_speech_provider.h \
    ../../src/providers/claude_model_provider.h \
    ../../src/providers/custom_speech_provider.h \
    ../../src/providers/deepseek_model_provider.h \
    ../../src/providers/model_provider.h \
    ../../src/providers/network_error_messages.h \
    ../../src/providers/network_request_executor.h \
    ../../src/providers/openai_compatible_model_provider.h \
    ../../src/providers/provider_registry.h \
    ../../src/providers/provider_network_transport.h \
    ../../src/providers/provider_websocket_transport.h \
    ../../src/providers/provider_types.h \
    ../../src/providers/speech_provider.h \
    ../../src/providers/xfyun_speech_protocol.h \
    ../../src/providers/xfyun_speech_provider.h \
    ../../src/result_flow_config.h \
    ../../src/runtime_log.h \
    ../../src/tasks/cancellation_token.h \
    ../../src/tasks/diagnostic_task_runner.h \
    ../../src/tasks/diagnostic_helpers.h \
    ../../src/tasks/interface_self_check_task.h \
    ../../src/tasks/network_diagnostics_task.h
