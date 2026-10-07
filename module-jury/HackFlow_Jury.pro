QT += core gui widgets

CONFIG += c++17

TARGET = HackFlow_Jury

SOURCES += \
    main.cpp \
    mainwindow.cpp

HEADERS += \
    scrollhelpers.h \
    mainwindow.h

FORMS += \
    mainwindow.ui

RESOURCES += \
    resources.qrc

# MSVC : lire les fichiers sources en UTF-8 (accents)
msvc: QMAKE_CXXFLAGS += /utf-8
