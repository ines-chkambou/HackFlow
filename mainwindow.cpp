#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QMessageBox>
#include <QPixmap>
#include <QIcon>
#include <QLabel>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // Texte riche : "Admin" en bleu clair
    ui->labelWelcome->setText(
        "Bienvenue, <span style='color:#38bdf8;'>Admin</span> !");

    // Logo HackFlow (bannière + icône de fenêtre)
    QPixmap logo(":/images/logo.png");
    if (!logo.isNull()) {
        ui->labelLogoImg->setPixmap(
            logo.scaled(56, 56, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        setWindowIcon(QIcon(logo));
    }

    // Illustration de la bannière
    QPixmap hero(":/images/hero.png");
    if (!hero.isNull()) {
        ui->labelIllustration->setPixmap(
            hero.scaledToHeight(250, Qt::SmoothTransformation));
    } else {
        ui->labelIllustration->setText("💡");
    }

    // Icônes des cartes
    auto setIcon = [](QLabel *l, const QString &path) {
        l->setPixmap(QPixmap(path).scaled(26, 26, Qt::KeepAspectRatio,
                                          Qt::SmoothTransformation));
        l->setAlignment(Qt::AlignCenter);
    };
    setIcon(ui->iconParticipants, ":/images/icon_participants.png");
    setIcon(ui->iconProjets,      ":/images/icon_projets.png");
    setIcon(ui->iconEmployes,     ":/images/icon_employes.png");
    setIcon(ui->iconJury,         ":/images/icon_jury.png");

    // Titre "Actions rapides" avec l'éclair
    ui->labelQuick->setText(
        "<img src=':/images/zap.png' width='18' height='18'> &nbsp;Actions rapides");

    appliquerStyle();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::appliquerStyle()
{
    setStyleSheet(R"(
        QMainWindow, #centralwidget {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                        stop:0 #0b1030, stop:1 #050818);
        }
        QLabel { color: white; background: transparent; }

        /* ---------- Bannière ---------- */
        #frameHero {
            border: 1px solid #1e3a8a;
            border-radius: 18px;
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                        stop:0 #0c1a4a, stop:0.6 #141a6b, stop:1 #1b1f8a);
        }
        #labelLogoImg { background: transparent; border-radius: 10px; }
        #labelLogo    { font-size: 22px; font-weight: bold; color: white; }
        #labelTagline { font-size: 8px; font-weight: bold; color: #22d3ee; letter-spacing: 1px; }
        #labelWelcome { font-size: 30px; font-weight: bold; color: white; }
        #labelSubtitle{ font-size: 13px; color: #e5e7eb; }
        #labelDesc    { font-size: 11px; color: #9ca3af; }
        #labelIllustration { font-size: 90px; }

        /* ---------- Titre section ---------- */
        #labelQuick { font-size: 15px; font-weight: bold; color: #e5e7eb; }

        /* ---------- Cartes ---------- */
        #cardParticipants, #cardProjets, #cardEmployes, #cardJury {
            border-radius: 16px;
        }
        #cardParticipants {
            border: 1px solid #6366f1;
            background: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #3b2a9c, stop:1 #1b1f6b);
        }
        #cardProjets {
            border: 1px solid #14b8a6;
            background: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #0f4c5c, stop:1 #12305a);
        }
        #cardEmployes {
            border: 1px solid #a855f7;
            background: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #5b21b6, stop:1 #2e1a7a);
        }
        #cardJury {
            border: 1px solid #3b82f6;
            background: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #1e3a8a, stop:1 #14235e);
        }
        #cardParticipants:hover { border: 2px solid #818cf8; }
        #cardProjets:hover      { border: 2px solid #2dd4bf; }
        #cardEmployes:hover     { border: 2px solid #c084fc; }
        #cardJury:hover         { border: 2px solid #60a5fa; }

        #iconParticipants, #iconProjets, #iconEmployes, #iconJury {
            font-size: 22px; background: rgba(255,255,255,0.12);
            border-radius: 12px; min-width: 48px; max-width: 48px;
            min-height: 48px; max-height: 48px; qproperty-alignment: AlignCenter;
        }
        #titleParticipants, #titleProjets, #titleEmployes, #titleJury {
            font-size: 15px; font-weight: bold; color: white;
        }
        #descParticipants, #descProjets, #descEmployes, #descJury {
            font-size: 10px; color: #cbd5e1;
        }

        /* ---------- Boutons flèche ronds ---------- */
        #btnParticipants, #btnProjets, #btnEmployes, #btnJury {
            color: white; font-size: 16px; font-weight: bold;
            border: none; border-radius: 18px;
            background: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #2563eb, stop:1 #4f46e5);
        }
        #btnParticipants:hover, #btnProjets:hover,
        #btnEmployes:hover, #btnJury:hover { background: #60a5fa; }
    )");
}

/* ---------- Slots (à remplacer par l'ouverture de vos vraies fenêtres) ---------- */
void MainWindow::on_btnParticipants_clicked()
{
    QMessageBox::information(this, "Participants", "Gestion des participants");
}

void MainWindow::on_btnProjets_clicked()
{
    QMessageBox::information(this, "Projets", "Gestion des projets");
}

void MainWindow::on_btnEmployes_clicked()
{
    QMessageBox::information(this, "Employés", "Gestion des employés");
}

void MainWindow::on_btnJury_clicked()
{
    QMessageBox::information(this, "Jury", "Gestion du jury");
}
