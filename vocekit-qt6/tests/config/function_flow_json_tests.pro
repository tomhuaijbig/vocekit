QT += core testlib
CONFIG += console testcase c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = function_flow_json_tests

INCLUDEPATH += ../..

SOURCES += \
    function_flow_json_tests.cpp \
    ../../src/config/app_settings_defaults.cpp \
    ../../src/config/app_settings_json.cpp \
    ../../src/config/function_flow_json.cpp \
    ../../src/config/secret_config.cpp \
    ../../src/config/secret_store.cpp \
    ../../src/config/selection_context_action_customization.cpp \
    ../../src/domain/function_flow_graph.cpp \
    ../../src/domain/function_flow_ports.cpp \
    ../../src/domain/selection_context_actions.cpp \
    ../../src/domain/function_settings.cpp \
    ../../src/file_utils.cpp \
    ../../src/providers/model_catalog.cpp \
    ../../src/result_flow_config.cpp

HEADERS += \
    ../../src/config/app_settings_data.h \
    ../../src/config/app_settings_defaults.h \
    ../../src/config/app_settings_json.h \
    ../../src/config/function_flow_json.h \
    ../../src/config/secret_config.h \
    ../../src/config/secret_store.h \
    ../../src/config/selection_context_action_customization.h \
    ../../src/domain/function_flow_graph.h \
    ../../src/domain/function_flow_ports.h \
    ../../src/domain/selection_context_actions.h \
    ../../src/domain/function_settings.h \
    ../../src/file_utils.h \
    ../../src/providers/model_catalog.h \
    ../../src/result_flow_config.h
