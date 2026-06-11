QT += core network concurrent
CONFIG += console c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = networkscanner_smoke

SOURCES += \
    networkscanner_smoke.cpp \
    ../src/networkscanner.cpp

HEADERS += ../src/networkscanner.h

msvc:QMAKE_CXXFLAGS += /utf-8
win32:LIBS += -lIphlpapi -lWs2_32

