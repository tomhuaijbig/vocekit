QT += core testlib
CONFIG += console testcase c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = selection_context_policy_tests

INCLUDEPATH += ../..

SOURCES += \
    selection_context_policy_tests.cpp \
    ../../src/controllers/selection_context_policy.cpp

HEADERS += \
    ../../src/controllers/selection_context_policy.h \
    ../../src/input/selection_snapshot.h
