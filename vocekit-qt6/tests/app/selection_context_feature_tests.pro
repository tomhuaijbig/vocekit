QT += core gui widgets concurrent testlib
CONFIG += console testcase c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = selection_context_feature_tests

INCLUDEPATH += ../..

SOURCES += \
    selection_context_feature_tests.cpp \
    ../../src/app/selection_context_feature.cpp \
    ../../src/controllers/selection_context_action_controller.cpp \
    ../../src/controllers/selection_context_coordinator.cpp \
    ../../src/controllers/selection_context_policy.cpp \
    ../../src/domain/selection_context_actions.cpp \
    ../../src/domain/prompt_runtime_library.cpp \
    ../../src/input/selected_text_reader.cpp \
    ../../src/input/selection_coordinate_mapper.cpp \
    ../../src/input/selection_observer.cpp \
    ../../src/input/selection_probe_runner.cpp \
    ../../src/output/clipboard_writer.cpp \
    ../../src/tasks/cancellation_token.cpp \
    ../../src/tasks/selection_context_model_request.cpp \
    ../../src/tasks/selection_context_model_runner.cpp \
    ../../src/ui/selection_context_placement.cpp \
    ../../src/ui/selection_context_toolbar.cpp \
    ../../src/ui/selection_result_card.cpp \
    ../../src/config/app_settings_defaults.cpp \
    ../../src/config/app_paths.cpp \
    ../../src/config/secret_config.cpp \
    ../../src/config/secret_store.cpp \
    ../../src/config/selection_context_action_customization.cpp \
    ../../src/file_utils.cpp \
    ../../src/providers/model_catalog.cpp \
    ../../src/result_flow_config.cpp

HEADERS += \
    ../../src/app/selection_context_feature.h \
    ../../src/controllers/selection_context_action_controller.h \
    ../../src/controllers/selection_context_coordinator.h \
    ../../src/controllers/selection_context_policy.h \
    ../../src/config/secret_config.h \
    ../../src/config/secret_store.h \
    ../../src/config/selection_context_action_customization.h \
    ../../src/input/selection_observer.h \
    ../../src/input/selection_probe_runner.h \
    ../../src/tasks/selection_context_model_runner.h \
    ../../src/providers/model_catalog.h \
    ../../src/ui/selection_context_toolbar.h \
    ../../src/ui/selection_result_card.h

win32:LIBS += -luser32 -lwtsapi32 -lole32 -loleaut32 -luuid
