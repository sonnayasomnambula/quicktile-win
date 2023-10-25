TEMPLATE = app
CONFIG += c++17
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
        main.cpp

RC_FILE = resource.rc

QMAKE_LFLAGS_WINDOWS  += -municode

DEFINES += \
    "_WIN32_WINNT=0x0A00" \
    "WINVER=0x0A00" \
    "_WIN32_IE=0x0900" \

LIBS += -lcomctl32

HEADERS += \
    resource.h
