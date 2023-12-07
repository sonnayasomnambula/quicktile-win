TEMPLATE = app
CONFIG += c++17
CONFIG -= app_bundle
CONFIG -= qt

RC_FILE = rsc/resource.rc

QMAKE_LFLAGS_WINDOWS  += -municode

DEFINES += \
    "_WIN32_WINNT=0x0A00" \
    "WINVER=0x0A00" \
    "_WIN32_IE=0x0900" \
    UNICODE \

LIBS += -lcomctl32

INCLUDEPATH += \
    rsc \
    src

SOURCES += \
    src/main.cpp \
    src/display.cpp \
    src/rect.cpp

HEADERS += \
    rsc/resource.h \
    src/display.h \
    src/rect.h
