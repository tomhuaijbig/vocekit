QT += core concurrent testlib
CONFIG += console testcase c++11
CONFIG -= app_bundle
TEMPLATE = app
TARGET = selection_context_model_runner_tests

INCLUDEPATH += ../..

SOURCES += \
    selection_context_model_runner_tests.cpp \
    ../../src/tasks/selection_context_model_runner.cpp \
    ../../src/tasks/cancellation_token.cpp

HEADERS += \
    ../../src/tasks/selection_context_model_runner.h \
    ../../src/tasks/model_request_task.h \
    ../../src/tasks/cancellation_token.h \
    ../../src/providers/model_provider.h \
    ../../src/providers/provider_types.h
