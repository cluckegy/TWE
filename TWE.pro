QT += core gui widgets network concurrent

CONFIG += c++17
TEMPLATE = app
TARGET = TWE

msvc {
    QMAKE_CFLAGS += /utf-8
    QMAKE_CXXFLAGS += /utf-8
}

# Npcap SDK
INCLUDEPATH += $$PWD/npcap-sdk/Include
win32:contains(QMAKE_TARGET.arch, x86_64) {
    LIBS += -L$$PWD/npcap-sdk/Lib/x64
} else {
    LIBS += -L$$PWD/npcap-sdk/Lib
}

SOURCES += \
    src/arpspoofer.cpp \
    src/credentialstore.cpp \
    src/main.cpp \
    src/mainwindow.cpp \
    src/networkscanner.cpp \
    src/wequotaservice.cpp

HEADERS += \
    src/arpspoofer.h \
    src/credentialstore.h \
    src/mainwindow.h \
    src/networkscanner.h \
    src/wequotaservice.h

RC_ICONS = src/resources/twe.ico

RESOURCES += resources.qrc

win32:LIBS += -lCrypt32 -lIphlpapi -lWs2_32 -lwpcap -lPacket
