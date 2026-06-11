QT += widgets network multimedia concurrent websockets

CONFIG += c++11

TARGET = vocekit
TEMPLATE = app

SOURCES += \
    src/main.cpp \
    src/voiceassistant.cpp

HEADERS += \
    src/voiceassistant.h

win32:LIBS += -luser32 -lole32 -loleaut32 -luuid
