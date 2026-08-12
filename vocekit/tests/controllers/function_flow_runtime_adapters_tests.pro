QT += core gui widgets network websockets testlib concurrent
CONFIG += console testcase c++11
CONFIG -= app_bundle
TEMPLATE = app
TARGET = function_flow_runtime_adapters_tests

INCLUDEPATH += ../..

SOURCES += \
    function_flow_runtime_adapters_tests.cpp \
    ../../src/api/api_client_utils.cpp \
    ../../src/config/app_paths.cpp \
    ../../src/config/app_settings_defaults.cpp \
    ../../src/config/secret_config.cpp \
    ../../src/config/secret_store.cpp \
    ../../src/controllers/function_flow_runtime_adapters.cpp \
    ../../src/domain/function_flow_graph.cpp \
    ../../src/domain/function_flow_model_message.cpp \
    ../../src/domain/function_flow_ports.cpp \
    ../../src/domain/function_flow_runtime_types.cpp \
    ../../src/domain/prompt_runtime_library.cpp \
    ../../src/file_utils.cpp \
    ../../src/providers/built_in_provider_factory.cpp \
    ../../src/providers/baidu_speech_provider.cpp \
    ../../src/providers/claude_model_provider.cpp \
    ../../src/providers/custom_speech_provider.cpp \
    ../../src/providers/deepseek_model_provider.cpp \
    ../../src/providers/model_catalog.cpp \
    ../../src/providers/network_error_messages.cpp \
    ../../src/providers/network_request_executor.cpp \
    ../../src/providers/openai_compatible_model_provider.cpp \
    ../../src/providers/provider_configuration.cpp \
    ../../src/providers/provider_network_transport.cpp \
    ../../src/providers/provider_websocket_transport.cpp \
    ../../src/providers/provider_registry.cpp \
    ../../src/providers/windows_speech_helper_client.cpp \
    ../../src/providers/windows_speech_helper_protocol.cpp \
    ../../src/providers/windows_speech_provider.cpp \
    ../../src/providers/xfyun_speech_protocol.cpp \
    ../../src/providers/xfyun_speech_provider.cpp \
    ../../src/result_flow_config.cpp \
    ../../src/runtime_log.cpp \
    ../../src/tasks/function_flow_model_task_runner.cpp \
    ../../src/tasks/cancellation_token.cpp \
    ../../src/tasks/diagnostic_helpers.cpp \
    ../../src/tasks/model_provider_request_task.cpp \
    ../../src/tasks/model_request_task.cpp

HEADERS += \
    ../../src/config/app_settings_data.h \
    ../../src/config/secret_config.h \
    ../../src/config/secret_store.h \
    ../../src/controllers/function_flow_runtime_adapters.h \
    ../../src/controllers/selected_text_workflow_controller.h \
    ../../src/domain/execution_types.h \
    ../../src/domain/function_flow_graph.h \
    ../../src/domain/function_flow_model_message.h \
    ../../src/domain/function_flow_ports.h \
    ../../src/domain/function_flow_runtime_types.h \
    ../../src/domain/function_settings.h \
    ../../src/domain/operation_error.h \
    ../../src/file_utils.h \
    ../../src/providers/model_catalog.h \
    ../../src/providers/windows_speech_helper_client.h \
    ../../src/providers/windows_speech_helper_protocol.h \
    ../../src/providers/windows_speech_provider.h \
    ../../src/domain/prompt_runtime_library.h \
    ../../src/tasks/cancellation_token.h \
    ../../src/tasks/function_flow_model_task_runner.h \
    ../../src/tasks/model_request_task.h
