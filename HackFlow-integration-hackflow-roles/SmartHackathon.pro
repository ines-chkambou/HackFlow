QT       += core gui widgets network concurrent sql
QT       += gui-private          # lecture des CV .docx (QZipReader)
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets
CONFIG   += c++17
TARGET    = SmartHackathon
TEMPLATE  = app

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    connection.cpp \
    employe.cpp \
    participantscontroller.cpp \
    jurycontroller.cpp \
    projectspage.cpp

HEADERS += \
    mainwindow.h \
    connection.h \
    employe.h \
    participantscontroller.h \
    jurycontroller.h \
    projectspage.h \
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
