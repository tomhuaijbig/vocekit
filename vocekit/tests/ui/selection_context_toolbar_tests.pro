QT += core gui widgets testlib
CONFIG += console testcase c++11
CONFIG -= app_bundle
TEMPLATE = app
TARGET = selection_context_toolbar_tests

INCLUDEPATH += ../..

SOURCES += \
    selection_context_toolbar_tests.cpp \
    ../../src/ui/selection_context_placement.cpp \
    ../../src/ui/selection_context_toolbar.cpp \
    ../../src/config/app_settings_defaults.cpp \
    ../../src/config/secret_config.cpp \
    ../../src/config/secret_store.cpp \
    ../../src/domain/selection_context_actions.cpp \
    ../../src/config/selection_context_action_customization.cpp \
    ../../src/file_utils.cpp \
    ../../src/providers/model_catalog.cpp

HEADERS += \
    ../../src/ui/selection_context_placement.h \
    ../../src/ui/selection_context_toolbar.h \
    ../../src/domain/selection_context_actions.h \
    ../../src/config/app_settings_defaults.h \
    ../../src/config/secret_config.h \
    ../../src/config/secret_store.h \
    ../../src/config/selection_context_action_customization.h \
    ../../src/file_utils.h \
    ../../src/providers/model_catalog.h \
    ../../src/input/selection_snapshot.h
