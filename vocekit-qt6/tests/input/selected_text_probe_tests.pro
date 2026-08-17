QT += core gui widgets concurrent testlib
CONFIG += console testcase c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = selected_text_probe_tests

INCLUDEPATH += ../..

SOURCES += \
    selected_text_probe_tests.cpp \
    ../../src/input/selected_text_reader.cpp \
    ../../src/input/selection_coordinate_mapper.cpp \
    ../../src/input/selection_probe_runner.cpp

HEADERS += \
    ../../src/input/selected_text_reader.h \
    ../../src/input/selection_coordinate_mapper.h \
    ../../src/input/selection_probe_runner.h \
    ../../src/input/selection_snapshot.h

win32:LIBS += -luser32 -lole32 -loleaut32 -luuid
