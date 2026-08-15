QT += core testlib
CONFIG += console testcase c++11
CONFIG -= app_bundle
TEMPLATE = app
TARGET = app_settings_json_tests

INCLUDEPATH += ../..

SOURCES += \
    app_settings_json_tests.cpp \
    ../../src/config/app_settings_defaults.cpp \
    ../../src/config/app_settings_json.cpp \
    ../../src/config/app_settings_store.cpp \
    ../../src/config/function_flow_json.cpp \
    ../../src/config/selection_context_action_customization.cpp \
    ../../src/domain/function_flow_graph.cpp \
    ../../src/domain/function_flow_ports.cpp \
    ../../src/domain/selection_context_actions.cpp \
    ../../src/domain/function_settings.cpp \
    ../../src/result_flow_config.cpp

HEADERS += \
    ../../src/config/app_settings_data.h \
    ../../src/config/app_settings_defaults.h \
    ../../src/config/app_settings_json.h \
    ../../src/config/app_settings_store.h \
    ../../src/config/function_flow_json.h \
    ../../src/config/selection_context_action_customization.h \
    ../../src/domain/function_flow_graph.h \
    ../../src/domain/function_flow_ports.h \
    ../../src/domain/selection_context_actions.h \
    ../../src/domain/function_settings.h \
    ../../src/domain/operation_error.h \
    ../../src/result_flow_config.h
