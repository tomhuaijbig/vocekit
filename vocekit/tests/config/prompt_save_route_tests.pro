QT += core testlib
CONFIG += console testcase c++11
CONFIG -= app_bundle
TEMPLATE = app
TARGET = prompt_save_route_tests

INCLUDEPATH += ../..

SOURCES += \
    prompt_save_route_tests.cpp \
    ../../src/config/prompt_save_route.cpp \
    ../../src/domain/prompt_runtime_library.cpp \
    ../../src/config/app_paths.cpp \
    ../../src/file_utils.cpp \
    ../../src/result_flow_config.cpp

HEADERS += \
    ../../src/config/prompt_save_route.h \
    ../../src/domain/prompt_runtime_library.h \
    ../../src/result_flow_config.h
