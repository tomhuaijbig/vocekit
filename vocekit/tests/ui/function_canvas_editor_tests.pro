QT += core gui widgets testlib
CONFIG += console testcase c++11
CONFIG -= app_bundle
TEMPLATE = app
TARGET = function_canvas_editor_tests

CONFIG(debug, debug|release) {
    MOC_DIR = $$OUT_PWD/debug/.moc-editor
    OBJECTS_DIR = $$OUT_PWD/debug/.obj-editor
} else {
    MOC_DIR = $$OUT_PWD/release/.moc-editor
    OBJECTS_DIR = $$OUT_PWD/release/.obj-editor
}

INCLUDEPATH += ../..

SOURCES += \
    function_canvas_editor_tests.cpp \
    ../../src/controllers/function_flow_editor_controller.cpp \
    ../../src/domain/function_flow_graph.cpp \
    ../../src/domain/function_flow_ports.cpp \
    ../../src/result_flow_config.cpp \
    ../../src/ui/function_canvas_edge_item.cpp \
    ../../src/ui/function_canvas_node_item.cpp \
    ../../src/ui/function_canvas_scene.cpp \
    ../../src/ui/function_canvas_view.cpp \
    ../../src/ui/function_canvas_palette.cpp \
    ../../src/ui/function_canvas_visual_style.cpp \
    ../../src/ui/function_canvas_inspector.cpp \
    ../../src/ui/function_canvas_editor.cpp \
    ../../src/ui/ui_style.cpp

HEADERS += \
    ../../src/controllers/function_flow_editor_controller.h \
    ../../src/domain/function_flow_graph.h \
    ../../src/domain/function_flow_ports.h \
    ../../src/domain/function_flow_publication_types.h \
    ../../src/domain/function_flow_runtime_types.h \
    ../../src/domain/function_flow_validation.h \
    ../../src/domain/function_settings.h \
    ../../src/ui/function_flow_settings_access.h \
    ../../src/ui/function_canvas_edge_item.h \
    ../../src/ui/function_canvas_node_item.h \
    ../../src/ui/function_canvas_scene.h \
    ../../src/ui/function_canvas_view.h \
    ../../src/ui/function_canvas_palette.h \
    ../../src/ui/function_canvas_visual_style.h \
    ../../src/ui/function_canvas_inspector.h \
    ../../src/ui/function_canvas_editor.h \
    ../../src/ui/ui_style.h
