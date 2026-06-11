QT += core network
CONFIG += console c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = captcha_smoke

SOURCES += captcha_smoke.cpp \
    ../src/credentialstore.cpp \
    ../src/wequotaservice.cpp
HEADERS += ../src/credentialstore.h \
    ../src/wequotaservice.h

msvc:QMAKE_CXXFLAGS += /utf-8
win32:LIBS += -lCrypt32
