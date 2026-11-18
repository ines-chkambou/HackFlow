#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QComboBox>
#include <QStringList>
#include <QWidget>
#include <QVector>
#include <QString>
#include <QColor>
#include <QFont>
#include <QSize>

class QCheckBox;
class QMenu;
class QAction;
class QStandardItemModel;
class ParticipantsController;   // module Participants (participantscontroller.h)
class EmployeesController;      // module Employes (employeescontroller.h)
class JuryController;           // module Jury (jurycontroller.h)
class ProjectsPage;             // module Gestion des projets (projectspage.h)
namespace Ui { class MainWindow; }

// ===========================================================================
//  Graphiques (anciennement chartwidgets.h / .cpp)
// ===========================================================================
struct ChartItem {
    QString label;
    double  value = 0;
    QColor  color;
};
typedef QVector<ChartItem> ChartData;

class ChartBase : public QWidget
{
    Q_OBJECT
public:
    explicit ChartBase(QWidget *parent = nullptr);
    void setData(const ChartData &data);
    QSize minimumSizeHint() const override { return QSize(200, 120); }
protected:
    ChartData m_data;
};

// Donut avec total au centre + legende (nom, effectif, pourcentage)
class PieChartWidget : public ChartBase
{
    Q_OBJECT
public:
    explicit PieChartWidget(QWidget *parent = nullptr) : ChartBase(parent) {}
    void setCenterCaption(const QString &c) { m_caption = c; update(); }
protected:
    void paintEvent(QPaintEvent *) override;
private:
    QString m_caption = "Employés";
};

// Histogramme
class BarChartWidget : public ChartBase
{
    Q_OBJECT
public:
    explicit BarChartWidget(QWidget *parent = nullptr) : ChartBase(parent) {}
protected:
    void paintEvent(QPaintEvent *) override;
};

// ===========================================================================
//  Liste deroulante a choix multiples (anciennement checkcombobox.h / .cpp)
//  utilisee pour "Equipe(s)"
// ===========================================================================
class CheckComboBox : public QComboBox
{
    Q_OBJECT
public:
    explicit CheckComboBox(QWidget *parent = nullptr);
    void        setItems(const QStringList &items);
    QStringList checkedItems() const;
    void        setCheckedItems(const QStringList &items);   // ajoute les valeurs inconnues
    void        clearChecked();
protected:
    bool eventFilter(QObject *obj, QEvent *ev) override;
private:
    QStandardItemModel *m_model;
    void updateText();
    void addItem_(const QString &t);
};

// ===========================================================================
//  Page "Choix du role" (entre la Connexion et l'application)
// ===========================================================================
// Fond decoratif : degrade nuit + orbites violette / cyan (palette du cahier des charges)
class RolesBackdrop : public QWidget
{
    Q_OBJECT
public:
    explicit RolesBackdrop(QWidget *parent = nullptr) : QWidget(parent) {}
protected:
    void paintEvent(QPaintEvent *) override;
};

// Titre "Choisissez votre role" : la 2e partie est en degrade bleu -> violet
class GradientTitle : public QWidget
{
    Q_OBJECT
public:
    explicit GradientTitle(QWidget *parent = nullptr);
    void  setParts(const QString &plain, const QString &accent);
    void  setAlignment(Qt::Alignment a) { m_align = a; update(); }   // Qt::AlignLeft ou Qt::AlignHCenter (defaut)
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override { return sizeHint(); }
protected:
    void paintEvent(QPaintEvent *) override;
private:
    QString m_plain, m_accent;
    Qt::Alignment m_align = Qt::AlignHCenter;
    QFont   titleFont() const;
};

// ===========================================================================
//  Fenetre principale (anciennement GSmartHackathon)
// ===========================================================================
// Phase "interface graphique" (un seul fichier .ui, vues = pages de QStackedWidget) :
//      stackedMain : 0 = Connexion, 1 = application, 2 = Choix du role
//  - metiers innovants (zip "Employes") : authentification + privileges, mot de passe oublie (e-mail),
//    creer un compte, attribution automatique des employes par CV.
//  - navigation :
//      "Se connecter"            : verification du compte, puis Connexion  -> Choix du role
//      boutons de la page role   : Choix du role -> application (Tableau de bord),
//                                  menu de gauche filtre selon le role (RBAC du cahier)
//                                  Participant, Jury et Employe : tous les modules
//      menu de gauche            : change de page dans "stackedPages"
//      "Deconnexion"             : application -> Connexion
//  - tous les boutons d'ACTION (Ajouter, Vider, Trier, Exporter, modifier, supprimer,
//    oeil du mot de passe, mot de passe oublie, creer un compte...) sont uniquement
//    visuels : aucun traitement n'est branche dessus.
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;
    QCheckBox *m_headCheck = nullptr;
    ParticipantsController *m_participants = nullptr;   // logique de la page Participants
    EmployeesController *m_employees = nullptr;         // logique de la page Employes
    JuryController *m_jury = nullptr;                   // logique de la page Jury
    ProjectsPage *m_projects = nullptr;                 // page Projets (module integre)

    enum class Role { Participant, Jury, Organisateur, Mentor, Technicien };
    Role m_role = Role::Organisateur;

    void setupNavigation();     // boutons de navigation -> pages des QStackedWidget
    void setupRoleMenu();       // page "Choix du role" (icones, signaux)
    void showConnexion();       // page Connexion (fenetre 960 x 640)
    void showRoles();           // page Choix du role (Participant / Jury / Employe)
    void showApplication(Role role);   // application (menu + pages), fenetre agrandie
    void applyRole(Role role);  // menu de gauche : seuls les modules autorises restent visibles
    void enterAsEmploye();      // Employe : Organisateur / Mentor / Technicien selectionne
    void setupDecor();          // donnees d'exemple affichees (tableau, graphiques)
    void positionHeaderCheck(); // placement de la case de l'en-tete (affichage uniquement)

    // ---- authentification / privileges (authmanager.*)
    void setupAuth();           // connexion, oeil, mot de passe oublie, creer un compte, menu Admin
    void doLogin();             // verifie le compte puis ouvre le Choix du role
    void doLogout();
    void applySession(const QString &profile);   // nom du compte + privileges (apres le choix du role)
    void applyPrivileges();     // masque / desactive ce que le role du compte ne permet pas
    // ---- attribution automatique des employes (traitement de CV)
    void onAssignToTeam();

    QMenu *m_adminMenu = nullptr;
    QAction *m_actWho = nullptr, *m_actUsers = nullptr, *m_actSmtp = nullptr, *m_actLogout = nullptr;
};

#endif // MAINWINDOW_H
