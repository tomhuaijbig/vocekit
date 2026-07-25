QT += core testlib
CONFIG += console testcase c++11
CONFIG -= app_bundle
TEMPLATE = app
TARGET = voice_model_runtime_settings_tests

INCLUDEPATH += ../..

SOURCES += \
    voice_model_runtime_settings_tests.cpp \
    ../../src/tasks/voice_model_runtime_settings.cpp \
    ../../src/result_flow_config.cpp

HEADERS += \
    ../../src/config/app_settings_data.h \
    ../../src/domain/function_settings.h \
    ../../src/tasks/voice_model_runtime_settings.h \
    ../../src/result_flow_config.h
