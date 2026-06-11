QT += core
CONFIG += console c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = credentialstore_smoke

SOURCES += credentialstore_smoke.cpp \
    ../src/credentialstore.cpp
HEADERS += ../src/credentialstore.h

msvc:QMAKE_CXXFLAGS += /utf-8
win32:LIBS += -lCrypt32
