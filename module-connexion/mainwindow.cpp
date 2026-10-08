#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QGuiApplication>
#include <QScreen>

// ===========================================================================
//  MainWindow (anciennement gsmarthackathon.cpp) : interface de CONNEXION
// ===========================================================================
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    showConnexion();            // l'application demarre sur l'ecran de connexion
}

MainWindow::~MainWindow()
{
    delete ui;
}

static void centerOnScreen(QWidget *w)
{
    QScreen *s = w->screen() ? w->screen() : QGuiApplication::primaryScreen();
    const QRect a = s->availableGeometry();
    w->move(a.center() - QPoint(w->width() / 2, w->height() / 2));
}

void MainWindow::showConnexion()
{
    ui->stackedMain->setCurrentWidget(ui->pageConnexion);
    setFixedSize(960, 640);                         // meme taille que la maquette de connexion
    centerOnScreen(this);
}
