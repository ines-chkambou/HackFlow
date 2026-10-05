QT       += core gui widgets network concurrent
QT       += gui-private          # lecture des CV .docx (QZipReader)
# Lecture des CV .pdf : module "Qt PDF" (Qt >= 6.6). Optionnel : s'il manque, .docx et .txt restent supportes.
qtHaveModule(pdf): greaterThan(QT_MINOR_VERSION, 5) {
    QT += pdf
    DEFINES += HAVE_QTPDF
}
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets
CONFIG   += c++17
TARGET    = SmartHackathon
TEMPLATE  = app

SOURCES += \
    main.cpp \
    gsmarthackathon.cpp \
    chartwidgets.cpp \
    checkcombobox.cpp \
    authmanager.cpp \
    mailer.cpp \
    forgotpassworddialog.cpp \
    createaccountdialog.cpp \
    admindialogs.cpp \
    cvanalyzer.cpp \
    cvassigndialog.cpp

HEADERS += \
    gsmarthackathon.h \
    chartwidgets.h \
    checkcombobox.h \
    authmanager.h \
    mailer.h \
    forgotpassworddialog.h \
    createaccountdialog.h \
    admindialogs.h \
    dialogstyle.h \
    cvanalyzer.h \
    cvassigndialog.h

FORMS += \
    gsmarthackathon.ui

RESOURCES += \
    resources.qrc
