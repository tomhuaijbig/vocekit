QT += core gui widgets testlib
CONFIG += console testcase c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = function_canvas_view_tests

CONFIG(debug, debug|release) {
    MOC_DIR = $$OUT_PWD/debug/.moc-flow-view
    OBJECTS_DIR = $$OUT_PWD/debug/.obj-flow-view
} else {
    MOC_DIR = $$OUT_PWD/release/.moc-flow-view
    OBJECTS_DIR = $$OUT_PWD/release/.obj-flow-view
}

SOURCES += \
    function_canvas_view_tests.cpp \
    ../../src/domain/function_flow_graph.cpp \
    ../../src/domain/function_flow_ports.cpp \
    ../../src/ui/function_canvas_edge_item.cpp \
    ../../src/ui/function_canvas_node_item.cpp \
    ../../src/ui/function_canvas_scene.cpp \
    ../../src/ui/function_canvas_view.cpp \
    ../../src/ui/function_canvas_visual_style.cpp \
    ../../src/ui/ui_style.cpp

HEADERS += \
    ../../src/domain/function_flow_graph.h \
    ../../src/domain/function_flow_ports.h \
    ../../src/domain/function_flow_runtime_types.h \
    ../../src/ui/function_canvas_edge_item.h \
    ../../src/ui/function_canvas_node_item.h \
    ../../src/ui/function_canvas_scene.h \
    ../../src/ui/function_canvas_view.h \
    ../../src/ui/function_canvas_visual_style.h \
    ../../src/ui/ui_style.h
