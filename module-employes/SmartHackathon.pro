QT       += core gui widgets
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets
CONFIG   += c++17
TARGET    = SmartHackathon
TEMPLATE  = app

SOURCES += \
    main.cpp \
    gsmarthackathon.cpp \
    chartwidgets.cpp \
    checkcombobox.cpp

HEADERS += \
    gsmarthackathon.h \
    chartwidgets.h \
    checkcombobox.h

FORMS += \
    gsmarthackathon.ui

RESOURCES += \
    resources.qrc
