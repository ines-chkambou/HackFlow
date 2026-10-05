#ifndef GSMARTHACKATHON_H
#define GSMARTHACKATHON_H

#include <QMainWindow>

class QCheckBox;
class QMenu;
class QAction;
class QWidget;
namespace Ui { class GSmartHackathon; }

// Interface graphique (un seul fichier .ui, vues = pages de QStackedWidget).
// Metiers innovants realises :
//  1. Authentification avec gestion des privileges  (authmanager.*  + applyPrivileges())
//  2. Mot de passe oublie par e-mail                (forgotpassworddialog.*, mailer.*)
//  3. Attribution automatique des employes par CV   (cvanalyzer.*, cvassigndialog.*)
// Les autres boutons d'action (Ajouter, Vider, Trier, Exporter, modifier, supprimer...)
// restent visuels : aucun traitement n'est branche dessus.
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

    // ---- authentification / privileges
    void setupAuth();           // connexion, oeil, mot de passe oublie, menu Admin
    void doLogin();
    void doLogout();
    void applySession();        // nom de l'utilisateur + privileges apres connexion
    void applyPrivileges();     // masque / desactive ce que le role ne permet pas
    // ---- attribution automatique (CV)
    void onAssignToTeam();

    QMenu *m_adminMenu = nullptr;
    QAction *m_actWho = nullptr, *m_actUsers = nullptr, *m_actSmtp = nullptr, *m_actLogout = nullptr;
};

#endif // GSMARTHACKATHON_H
