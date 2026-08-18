QT += core testlib
CONFIG += console testcase c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = model_advanced_request_tests

INCLUDEPATH += ../..

SOURCES += \
    model_advanced_request_tests.cpp \
    ../../src/config/app_paths.cpp \
    ../../src/config/model_advanced_settings.cpp \
    ../../src/file_utils.cpp \
    ../../src/storage/model_request_log.cpp

HEADERS += \
    ../../src/config/app_paths.h \
    ../../src/config/model_advanced_settings.h \
    ../../src/domain/execution_types.h \
    ../../src/domain/model_sampling_settings.h \
    ../../src/domain/operation_error.h \
    ../../src/file_utils.h \
    ../../src/providers/model_request_customization.h \
    ../../src/providers/model_response_metadata.h \
    ../../src/providers/provider_types.h \
    ../../src/storage/model_request_log.h
