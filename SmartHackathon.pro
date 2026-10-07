QT       += core gui widgets
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets
CONFIG   += c++17
TARGET    = SmartHackathon
TEMPLATE  = app

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    participantscontroller.cpp \
    jurycontroller.cpp

HEADERS += \
    mainwindow.h \
    participantscontroller.h \
    jurycontroller.h \
    scrollhelpers.h

FORMS += \
    mainwindow.ui

RESOURCES += \
    resources.qrc

# Module Participants : Qt PDF (facultatif) pour lire le texte du CV et choisir l'equipe automatiquement.
# Sans Qt PDF le module utilise un lecteur de secours (PDF simples).
qtHaveModule(pdf) {
    QT += pdf
    DEFINES += HAVE_QTPDF
}

# MSVC : lire les fichiers sources en UTF-8 (accents)
msvc: QMAKE_CXXFLAGS += /utf-8
