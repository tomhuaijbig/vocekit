QT += core network testlib
CONFIG += console testcase c++11
CONFIG -= app_bundle
TEMPLATE = app
TARGET = api_client_utils_tests

INCLUDEPATH += ../..

SOURCES += \
    api_client_utils_tests.cpp \
    ../../src/api/api_client_utils.cpp

HEADERS += \
    ../../src/api/api_client_utils.h
