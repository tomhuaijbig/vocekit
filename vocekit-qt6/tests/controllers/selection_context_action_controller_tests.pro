QT += core gui widgets concurrent testlib
CONFIG += console testcase c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = selection_context_action_controller_tests

INCLUDEPATH += ../..

SOURCES += \
    selection_context_action_controller_tests.cpp \
    ../../src/controllers/selection_context_action_controller.cpp \
    ../../src/tasks/selection_context_model_request.cpp \
    ../../src/tasks/selection_context_model_runner.cpp \
    ../../src/tasks/cancellation_token.cpp \
    ../../src/domain/selection_context_actions.cpp \
    ../../src/domain/prompt_runtime_library.cpp \
    ../../src/config/app_settings_defaults.cpp \
    ../../src/config/app_paths.cpp \
    ../../src/config/secret_config.cpp \
    ../../src/config/secret_store.cpp \
    ../../src/config/selection_context_action_customization.cpp \
    ../../src/file_utils.cpp \
    ../../src/providers/model_catalog.cpp \
    ../../src/result_flow_config.cpp

HEADERS += \
    ../../src/controllers/selection_context_action_controller.h \
    ../../src/config/selection_context_action_customization.h \
    ../../src/tasks/selection_context_model_request.h \
    ../../src/tasks/selection_context_model_runner.h \
    ../../src/input/selection_snapshot.h
