QT += core gui testlib
CONFIG += console testcase c++11
CONFIG -= app_bundle
TEMPLATE = app
TARGET = hotkey_settings_snapshot_tests

INCLUDEPATH += ../..

SOURCES += \
    hotkey_settings_snapshot_tests.cpp \
    ../../src/capture/screenshot_types.cpp \
    ../../src/domain/function_flow_graph.cpp \
    ../../src/domain/function_flow_ports.cpp \
    ../../src/domain/selection_context_actions.cpp \
    ../../src/input/hotkey_definitions.cpp \
    ../../src/input/hotkey_settings_snapshot.cpp \
    ../../src/result_flow_config.cpp

HEADERS += \
    ../../src/capture/screenshot_types.h \
    ../../src/config/app_settings_data.h \
    ../../src/domain/app_legacy_types.h \
    ../../src/domain/function_flow_graph.h \
    ../../src/domain/function_flow_ports.h \
    ../../src/domain/selection_context_actions.h \
    ../../src/domain/function_settings.h \
    ../../src/input/global_hotkeys.h \
    ../../src/input/hotkey_definitions.h \
    ../../src/input/hotkey_settings_snapshot.h \
    ../../src/result_flow_config.h
