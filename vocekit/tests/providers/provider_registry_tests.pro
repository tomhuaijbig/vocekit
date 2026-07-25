QT += core testlib
CONFIG += console testcase c++11
CONFIG -= app_bundle
TEMPLATE = app
TARGET = provider_registry_tests

INCLUDEPATH += ../..

SOURCES += \
    provider_registry_tests.cpp \
    ../../src/providers/provider_registry.cpp \
    ../../src/tasks/cancellation_token.cpp

HEADERS += \
    ../../src/domain/execution_types.h \
    ../../src/domain/operation_error.h \
    ../../src/providers/model_provider.h \
    ../../src/providers/provider_registry.h \
    ../../src/providers/provider_types.h \
    ../../src/providers/speech_provider.h \
    ../../src/tasks/cancellation_token.h
