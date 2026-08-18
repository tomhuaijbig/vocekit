QT += core gui widgets network concurrent testlib

CONFIG += c++17 console testcase
CONFIG -= app_bundle
DEFINES += VOCEKIT_TESTING

TEMPLATE = app
TARGET = settings_panel_access_factory_tests

SOURCES += \
    settings_panel_access_factory_tests.cpp \
    ../../src/api/api_client_utils.cpp \
    ../../src/capture/screenshot_types.cpp \
    ../../src/config/app_paths.cpp \
    ../../src/config/app_settings_defaults.cpp \
    ../../src/config/model_advanced_settings.cpp \
    ../../src/config/baidu_sample_parser.cpp \
    ../../src/config/secret_config.cpp \
    ../../src/config/secret_store.cpp \
    ../../src/config/selection_context_action_customization.cpp \
    ../../src/domain/function_settings.cpp \
    ../../src/domain/selection_context_actions.cpp \
    ../../src/file_utils.cpp \
    ../../src/input/hotkey_definitions.cpp \
    ../../src/platform/windows_autostart.cpp \
    ../../src/providers/network_error_messages.cpp \
    ../../src/providers/network_request_executor.cpp \
    ../../src/providers/claude_model_provider.cpp \
    ../../src/providers/deepseek_model_provider.cpp \
    ../../src/providers/model_catalog.cpp \
    ../../src/providers/openai_compatible_model_provider.cpp \
    ../../src/providers/provider_network_transport.cpp \
    ../../src/providers/windows_speech_helper_client.cpp \
    ../../src/providers/windows_speech_helper_protocol.cpp \
    ../../src/result_flow_config.cpp \
    ../../src/runtime_log.cpp \
    ../../src/storage/history_paths.cpp \
    ../../src/storage/history_store.cpp \
    ../../src/storage/model_request_log.cpp \
    ../../src/tasks/cancellation_token.cpp \
    ../../src/tasks/diagnostic_task_runner.cpp \
    ../../src/tasks/model_api_diagnostics_task.cpp \
    ../../src/ui/advanced_api_dialog.cpp \
    ../../src/ui/api_settings_section.cpp \
    ../../src/ui/app_dialogs.cpp \
    ../../src/ui/attention_message.cpp \
    ../../src/ui/basic_settings_section.cpp \
    ../../src/ui/custom_model_dialog_support.cpp \
    ../../src/ui/floating_bar_style_selector.cpp \
    ../../src/ui/history_directory_menu.cpp \
    ../../src/ui/history_settings_section.cpp \
    ../../src/ui/hub_settings_state.cpp \
    ../../src/ui/selection_context_action_editor.cpp \
    ../../src/ui/selection_context_settings_card.cpp \
    ../../src/ui/settings_panel.cpp \
    ../../src/ui/settings_panel_access_factory.cpp \
    ../../src/ui/shortcut_display.cpp \
    ../../src/ui/shortcut_settings_section.cpp \
    ../../src/ui/ui_style.cpp \
    ../../src/ui/windows_speech_settings_card.cpp

HEADERS += \
    ../../src/config/model_advanced_settings.h \
    ../../src/capture/screenshot_types.h \
    ../../src/config/app_settings_data.h \
    ../../src/config/selection_context_action_customization.h \
    ../../src/domain/app_legacy_types.h \
    ../../src/domain/function_settings.h \
    ../../src/domain/selection_context_actions.h \
    ../../src/domain/prompt_runtime_library.h \
    ../../src/providers/model_catalog.h \
    ../../src/ui/hub_settings_state.h \
    ../../src/ui/floating_bar_style_selector.h \
    ../../src/ui/selection_context_action_editor.h \
    ../../src/ui/selection_context_settings_card.h \
    ../../src/ui/settings_panel.h \
    ../../src/ui/settings_panel_access_factory.h
