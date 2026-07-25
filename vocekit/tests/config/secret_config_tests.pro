QT += core testlib
CONFIG += console testcase c++11
CONFIG -= app_bundle
TEMPLATE = app
TARGET = secret_config_tests

INCLUDEPATH += ../..

SOURCES += \
    secret_config_tests.cpp \
    ../../src/config/baidu_sample_parser.cpp \
    ../../src/config/secret_config.cpp \
    ../../src/config/secret_store.cpp \
    ../../src/file_utils.cpp

HEADERS += \
    ../../src/config/baidu_sample_parser.h \
    ../../src/config/secret_config.h \
    ../../src/config/secret_store.h \
    ../../src/file_utils.h
