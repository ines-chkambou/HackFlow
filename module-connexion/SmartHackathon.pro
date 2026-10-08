QT       += core gui widgets
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets
CONFIG   += c++17
TARGET    = SmartHackathon
TEMPLATE  = app

SOURCES += \
    main.cpp \
    mainwindow.cpp

HEADERS += \
    mainwindow.h

FORMS += \
    mainwindow.ui

RESOURCES += \
    resources.qrc

# MSVC : lire les fichiers sources en UTF-8 (accents)
msvc: QMAKE_CXXFLAGS += /utf-8
