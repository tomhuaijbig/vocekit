QT += core gui testlib
CONFIG += console testcase c++11
CONFIG -= app_bundle
TEMPLATE = app
TARGET = function_canvas_visual_style_tests

CONFIG(debug, debug|release) {
    MOC_DIR = $$OUT_PWD/debug/.moc
    OBJECTS_DIR = $$OUT_PWD/debug/.obj
} else {
    MOC_DIR = $$OUT_PWD/release/.moc
    OBJECTS_DIR = $$OUT_PWD/release/.obj
}

INCLUDEPATH += ../..

SOURCES += \
    function_canvas_visual_style_tests.cpp \
    ../../src/domain/function_flow_graph.cpp \
    ../../src/domain/function_flow_ports.cpp \
    ../../src/ui/function_canvas_visual_style.cpp

HEADERS += \
    function_canvas_visual_style_tests.h \
    ../../src/domain/function_flow_graph.h \
    ../../src/domain/function_flow_ports.h \
    ../../src/domain/function_flow_runtime_types.h \
    ../../src/ui/function_canvas_visual_style.h
