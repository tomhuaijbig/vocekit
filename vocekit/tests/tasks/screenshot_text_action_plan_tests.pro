QT += core testlib
CONFIG += console testcase c++11
CONFIG -= app_bundle
TEMPLATE = app
TARGET = screenshot_text_action_plan_tests

INCLUDEPATH += ../..

SOURCES += \
    screenshot_text_action_plan_tests.cpp \
    ../../src/tasks/screenshot_text_action_plan.cpp \
    ../../src/result_flow_config.cpp

HEADERS += \
    ../../src/config/app_settings_data.h \
    ../../src/domain/app_legacy_types.h \
    ../../src/domain/function_settings.h \
    ../../src/tasks/screenshot_text_action_plan.h \
    ../../src/result_flow_config.h
