QT += core network testlib
CONFIG += console testcase c++11
CONFIG -= app_bundle
TEMPLATE = app
TARGET = openai_compatible_model_provider_tests

INCLUDEPATH += ../..

SOURCES += \
    openai_compatible_model_provider_tests.cpp \
    ../../src/api/api_client_utils.cpp \
    ../../src/config/secret_config.cpp \
    ../../src/config/secret_store.cpp \
    ../../src/file_utils.cpp \
    ../../src/providers/network_request_executor.cpp \
    ../../src/providers/openai_compatible_model_provider.cpp \
    ../../src/providers/provider_network_transport.cpp \
    ../../src/result_flow_config.cpp \
    ../../src/runtime_log.cpp \
    ../../src/tasks/cancellation_token.cpp

HEADERS += \
    ../../src/api/api_client_utils.h \
    ../../src/config/secret_config.h \
    ../../src/config/secret_store.h \
    ../../src/domain/execution_types.h \
    ../../src/domain/operation_error.h \
    ../../src/file_utils.h \
    ../../src/providers/model_provider.h \
    ../../src/providers/network_request_executor.h \
    ../../src/providers/openai_compatible_model_provider.h \
    ../../src/providers/provider_network_transport.h \
    ../../src/providers/provider_types.h \
    ../../src/result_flow_config.h \
    ../../src/runtime_log.h \
    ../../src/tasks/cancellation_token.h
