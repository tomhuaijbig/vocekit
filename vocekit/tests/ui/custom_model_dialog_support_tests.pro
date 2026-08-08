QT += core gui widgets testlib

CONFIG += c++11 console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = custom_model_dialog_support_tests

SOURCES += \
    custom_model_dialog_support_tests.cpp \
    ../../src/ui/custom_model_dialog_support.cpp \
    ../../src/api/api_client_utils.cpp

HEADERS += \
    ../../src/ui/custom_model_dialog_support.h \
    ../../src/api/api_client_utils.h
