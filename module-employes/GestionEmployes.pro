QT       += core gui widgets
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets
CONFIG   += c++17
TARGET    = GestionEmployes
TEMPLATE  = app

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    chartwidgets.cpp \
    checkcombobox.cpp

HEADERS += \
    mainwindow.h \
    chartwidgets.h \
    checkcombobox.h

FORMS += \
    mainwindow.ui

RESOURCES += \
    resources.qrc