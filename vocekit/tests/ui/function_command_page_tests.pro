QT += core gui widgets testlib
CONFIG += console testcase c++11
CONFIG -= app_bundle
TEMPLATE = app
TARGET = function_command_page_tests

INCLUDEPATH += ../..

SOURCES += \
    function_command_page_tests.cpp \
    ../../src/capture/screenshot_types.cpp \
    ../../src/config/app_paths.cpp \
    ../../src/config/app_settings_defaults.cpp \
    ../../src/config/selection_context_action_customization.cpp \
    ../../src/config/prompt_save_route.cpp \
    ../../src/config/secret_config.cpp \
    ../../src/config/secret_store.cpp \
    ../../src/controllers/function_flow_editor_controller.cpp \
    ../../src/domain/function_catalog.cpp \
    ../../src/domain/function_flow_graph.cpp \
    ../../src/domain/function_flow_ports.cpp \
    ../../src/domain/function_settings.cpp \
    ../../src/domain/prompt_runtime_library.cpp \
    ../../src/domain/selection_context_actions.cpp \
    ../../src/file_utils.cpp \
    ../../src/input/hotkey_definitions.cpp \
    ../../src/providers/model_catalog.cpp \
    ../../src/result_flow_config.cpp \
    ../../src/ui/attention_message.cpp \
    ../../src/ui/command_center_shell.cpp \
    ../../src/ui/command_search_router.cpp \
    ../../src/ui/function_canvas_edge_item.cpp \
    ../../src/ui/function_canvas_editor.cpp \
    ../../src/ui/function_canvas_inspector.cpp \
    ../../src/ui/function_canvas_node_item.cpp \
    ../../src/ui/function_canvas_palette.cpp \
    ../../src/ui/function_canvas_scene.cpp \
    ../../src/ui/function_canvas_visual_style.cpp \
    ../../src/ui/function_canvas_view.cpp \
    ../../src/ui/function_command_page.cpp \
    ../../src/ui/floating_bar_style_selector.cpp \
    ../../src/ui/hub_settings_state.cpp \
    ../../src/ui/prompt_settings_adapter.cpp \
    ../../src/ui/reorderable_card_column.cpp \
    ../../src/ui/shortcut_display.cpp \
    ../../src/ui/ui_style.cpp

HEADERS += \
    ../../src/config/secret_config.h \
    ../../src/config/secret_store.h \
    ../../src/config/selection_context_action_customization.h \
    ../../src/domain/selection_context_actions.h \
    ../../src/file_utils.h \
    ../../src/providers/model_catalog.h \
    ../../src/controllers/function_flow_editor_controller.h \
    ../../src/domain/function_flow_graph.h \
    ../../src/domain/function_flow_ports.h \
    ../../src/domain/function_flow_publication_types.h \
    ../../src/domain/function_flow_runtime_types.h \
    ../../src/domain/function_settings.h \
    ../../src/ui/function_canvas_editor.h \
    ../../src/ui/function_canvas_inspector.h \
    ../../src/ui/function_canvas_node_item.h \
    ../../src/ui/function_canvas_palette.h \
    ../../src/ui/function_canvas_scene.h \
    ../../src/ui/function_canvas_visual_style.h \
    ../../src/ui/function_canvas_view.h \
    ../../src/ui/function_command_page.h \
    ../../src/ui/floating_bar_style_selector.h \
    ../../src/ui/function_flow_settings_access.h \
    ../../src/ui/hub_settings_state.h
