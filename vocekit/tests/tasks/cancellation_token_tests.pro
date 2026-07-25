QT += core testlib
CONFIG += console testcase c++11
CONFIG -= app_bundle
TEMPLATE = app
TARGET = cancellation_token_tests

INCLUDEPATH += ../..

SOURCES += \
    cancellation_token_tests.cpp \
    ../../src/tasks/cancellation_token.cpp

HEADERS += \
    ../../src/domain/execution_types.h \
    ../../src/domain/operation_error.h \
    ../../src/tasks/cancellation_token.h
