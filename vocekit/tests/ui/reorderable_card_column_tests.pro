QT += widgets testlib

CONFIG += testcase c++11
TEMPLATE = app
TARGET = reorderable_card_column_tests

INCLUDEPATH += ../..

SOURCES += \
    reorderable_card_column_tests.cpp \
    ../../src/ui/reorderable_card_column.cpp

HEADERS += \
    ../../src/ui/reorderable_card_column.h
