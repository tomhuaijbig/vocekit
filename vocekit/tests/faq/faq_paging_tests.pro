QT += testlib core
CONFIG += console c++11 testcase
TEMPLATE = app
TARGET = faq_paging_tests

SOURCES += \
    faq_paging_tests.cpp \
    ../../src/faq_paging.cpp

HEADERS += \
    ../../src/faq_paging.h
