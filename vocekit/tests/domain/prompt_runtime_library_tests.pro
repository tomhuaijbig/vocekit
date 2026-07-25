QT += core testlib
CONFIG += console testcase c++11
CONFIG -= app_bundle
TEMPLATE = app
TARGET = prompt_runtime_library_tests

INCLUDEPATH += ../..

SOURCES += \
    prompt_runtime_library_tests.cpp \
    ../../src/domain/prompt_runtime_library.cpp \
    ../../src/config/app_paths.cpp \
    ../../src/file_utils.cpp \
    ../../src/result_flow_config.cpp

HEADERS += \
    ../../src/config/app_settings_data.h \
    ../../src/domain/app_legacy_types.h \
    ../../src/domain/prompt_runtime_library.h \
    ../../src/result_flow_config.h
