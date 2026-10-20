QT       += core gui widgets network concurrent
QT       += gui-private          # lecture des CV .docx (QZipReader)
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets
CONFIG   += c++17
TARGET    = SmartHackathon
TEMPLATE  = app

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    participantscontroller.cpp \
    jurycontroller.cpp \
    employeescontroller.cpp \
    projectspage.cpp \
    authmanager.cpp \
    mailer.cpp \
    forgotpassworddialog.cpp \
    createaccountdialog.cpp \
    admindialogs.cpp \
    cvanalyzer.cpp \
    cvassigndialog.cpp

HEADERS += \
    mainwindow.h \
    participantscontroller.h \
    jurycontroller.h \
    employeescontroller.h \
    projectspage.h \
    scrollhelpers.h \
    authmanager.h \
    mailer.h \
    forgotpassworddialog.h \
    createaccountdialog.h \
    admindialogs.h \
    cvanalyzer.h \
    cvassigndialog.h \
    dialogstyle.h

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
