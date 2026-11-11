#-------------------------------------------------
#
# Project created for Smart Hackathon Management (HackFlow)
# Module: Gestion du Jury et des Évaluations (Module 6)
# Équipe ChronoHack / NovaCrew - AU : 25-26
#
#-------------------------------------------------

QT       += core gui widgets sql printsupport

CONFIG += c++17

TARGET = SmartHackathon
TEMPLATE = app

# The main window class is GJury (following university guidelines: "G" + Topic)
SOURCES += \
    main.cpp \
    gjury.cpp \
    connection.cpp \
    jury.cpp \
    evaluation.cpp \
    chartwidgets.cpp \
    parametresjury.cpp \
    planningevaluations.cpp \
    evaluationprojets.cpp

HEADERS += \
    gjury.h \
    connection.h \
    jury.h \
    evaluation.h \
    chartwidgets.h \
    parametresjury.h \
    planningevaluations.h \
    evaluationprojets.h \
    uihelpers.h

# Strictly one single .ui file as required by project specifications
FORMS += \
    gjury.ui

RESOURCES += \
    resources.qrc

# Windows application icon and metadata
win32:exists($$PWD/resources/app_icon.ico) {
    RC_ICONS = resources/app_icon.ico
}
