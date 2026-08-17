QT += core network testlib
CONFIG += console testcase c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = selected_text_diagnostic_task_tests

INCLUDEPATH += ../..

SOURCES += \
    selected_text_diagnostic_task_tests.cpp \
    ../../src/tasks/cancellation_token.cpp \
    ../../src/tasks/diagnostic_helpers.cpp \
    ../../src/tasks/selected_text_diagnostic_task.cpp

HEADERS += \
    ../../src/tasks/cancellation_token.h \
    ../../src/tasks/diagnostic_helpers.h \
    ../../src/tasks/selected_text_diagnostic_task.h
