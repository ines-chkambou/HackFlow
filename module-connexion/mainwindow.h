#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

namespace Ui { class MainWindow; }

// ===========================================================================
//  Fenetre principale (anciennement GSmartHackathon) : interface de CONNEXION
// ===========================================================================
//  Seule la page de connexion est presente. Les boutons (Se connecter, oeil du mot de
//  passe, mot de passe oublie, creer un compte...) sont uniquement visuels.
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;

    void showConnexion();       // page Connexion (fenetre 960 x 640)
};

#endif // MAINWINDOW_H
