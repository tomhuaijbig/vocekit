QT += core testlib
CONFIG += c++11 console testcase
CONFIG -= app_bundle
TEMPLATE = app
TARGET = classic_auto_write_executor_tests

SOURCES += \
    classic_auto_write_executor_tests.cpp \
    ../../src/output/classic_auto_write_executor.cpp

HEADERS += \
    ../../src/output/classic_auto_write_executor.h \
    ../../src/output/clipboard_writer.h
