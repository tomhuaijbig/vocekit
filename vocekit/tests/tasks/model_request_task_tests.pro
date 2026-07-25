QT += core testlib
CONFIG += console testcase c++11
CONFIG -= app_bundle
TEMPLATE = app
TARGET = model_request_task_tests

INCLUDEPATH += ../..

SOURCES += \
    model_request_task_tests.cpp \
    ../../src/tasks/cancellation_token.cpp \
    ../../src/tasks/model_request_task.cpp

HEADERS += \
    ../../src/domain/execution_types.h \
    ../../src/domain/operation_error.h \
    ../../src/providers/model_provider.h \
    ../../src/providers/provider_types.h \
    ../../src/tasks/cancellation_token.h \
    ../../src/tasks/model_request_task.h
