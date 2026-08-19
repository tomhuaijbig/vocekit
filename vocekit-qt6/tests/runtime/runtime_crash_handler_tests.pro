QT += core testlib
CONFIG += console testcase c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = runtime_crash_handler_tests

INCLUDEPATH += ../..

SOURCES += \
    runtime_crash_handler_tests.cpp \
    ../../src/runtime_crash_handler.cpp \
    ../../src/runtime_session.cpp \
    ../../src/runtime_log.cpp

HEADERS += \
    ../../src/runtime_crash_handler.h \
    ../../src/runtime_session.h \
    ../../src/runtime_log.h

win32:LIBS += -ldbghelp
