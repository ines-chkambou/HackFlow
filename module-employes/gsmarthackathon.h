#ifndef GSMARTHACKATHON_H
#define GSMARTHACKATHON_H

#include <QMainWindow>

class QCheckBox;
namespace Ui { class GSmartHackathon; }

// Phase "interface graphique" (un seul fichier .ui, vues = pages de QStackedWidget) :
//  - seuls les boutons de NAVIGATION fonctionnent :
//      "Se connecter"            : Connexion  -> application (Tableau de bord)
//      menu de gauche            : change de page dans "stackedPages"
//      "Deconnexion"             : application -> Connexion
//  - tous les boutons d'ACTION (Ajouter, Vider, Trier, Exporter, modifier, supprimer,
//    oeil du mot de passe, mot de passe oublie, creer un compte...) sont uniquement
//    visuels : aucun traitement n'est branche dessus.
class GSmartHackathon : public QMainWindow
{
    Q_OBJECT
public:
    explicit GSmartHackathon(QWidget *parent = nullptr);
    ~GSmartHackathon();

private:
    Ui::GSmartHackathon *ui;
    QCheckBox *m_headCheck = nullptr;

    void setupNavigation();     // boutons de navigation -> pages des QStackedWidget
    void showConnexion();       // page Connexion (fenetre 960 x 640)
    void showApplication();     // application (menu + pages), fenetre agrandie
    void setupDecor();          // donnees d'exemple affichees (tableau, graphiques)
    void positionHeaderCheck(); // placement de la case de l'en-tete (affichage uniquement)
};

#endif // GSMARTHACKATHON_H
