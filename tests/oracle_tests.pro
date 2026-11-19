QT += core gui widgets sql printsupport

CONFIG += c++17 console
CONFIG -= app_bundle

TEMPLATE = app
TARGET = oracle_tests

INCLUDEPATH += ..

SOURCES += \
    oracle_tests.cpp \
    ../connection.cpp \
    ../jury.cpp \
    ../evaluation.cpp \
    ../planningevaluations.cpp \
    ../parametresjury.cpp

HEADERS += \
    ../connection.h \
    ../jury.h \
    ../evaluation.h \
    ../planningevaluations.h \
    ../parametresjury.h \
    ../sqlutils.h \
    ../uihelpers.h
