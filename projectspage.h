#ifndef PROJECTSPAGE_H
#define PROJECTSPAGE_H

#include <QDate>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QStackedWidget;
class QVBoxLayout;

struct Project
{
    int id;
    QString name;
    QString desc;
    QString team;
    QString status;      // "En cours", "Terminé", "En attente"
    int members;
    int progress;        // 0..100
    QDate start;
    QDate end;
    QString icon;
    QString color;
};

struct Activity
{
    QString who;
    QString text;
    QString when;
};

// Donut du "Vue d'ensemble"
class DonutWidget : public QWidget
{
public:
    explicit DonutWidget(QWidget *parent = nullptr) : QWidget(parent)
    {
        setMinimumSize(170, 170);
    }
    void setValues(int running, int done, int waiting)
    {
        m_run = running; m_done = done; m_wait = waiting;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override;

private:
    int m_run = 0, m_done = 0, m_wait = 0;
};

// ===========================================================================
//  Module « Gestion des projets » (integre dans la page « Projets » de HackFlow)
//  Liste / grille de projets, filtres (recherche, statut, equipe), creation, modification,
//  changement de statut, suppression, statistiques, donut et activites recentes.
// ===========================================================================
class ProjectsPage : public QWidget
{
    Q_OBJECT

public:
    explicit ProjectsPage(QWidget *parent = nullptr);
    void setSearchText(const QString &text);    // barre de recherche du haut de HackFlow
    QPushButton *newProjectButton() const { return m_newBtn; }   // place dans l'en-tete de la page

private:
    // construction de l'interface
    QWidget *buildProjectsPage();
    QWidget *buildRightPanel();
    QWidget *makeStat(const QString &icon, const QString &color,
                      QLabel *&number, const QString &caption);
    QWidget *makeCard(const Project &p);

    // logique
    bool loadProjectsFromDatabase();
    void seedData();
    void refresh();
    void refreshList();
    void refreshStats();
    void refreshSide();
    void refreshTeamBox();
    QStringList knownTeams() const;
    int indexOfProject(int id) const;
    bool projectDialog(Project &pr, const QString &title);
    void showProject(int id);
    void addProject();
    void editProject(int id);
    void deleteProject(int id);
    void setStatus(int id, const QString &status);
    void addActivity(const QString &text);
    void showAllActivities();

    QVector<Project> m_projects;
    QVector<Activity> m_activities;
    int m_nextId = 1;
    int m_notif = 0;
    bool m_gridMode = false;

    QLineEdit *m_projectSearch = nullptr;
    QComboBox *m_statusBox = nullptr;
    QComboBox *m_teamBox = nullptr;
    QPushButton *m_newBtn = nullptr;
    QPushButton *m_gridBtn = nullptr;
    QPushButton *m_listBtn = nullptr;
    QLabel *m_totalLbl = nullptr;
    QLabel *m_runLbl = nullptr;
    QLabel *m_doneLbl = nullptr;
    QLabel *m_waitLbl = nullptr;
    QLabel *m_lgRun = nullptr;
    QLabel *m_lgDone = nullptr;
    QLabel *m_lgWait = nullptr;
    DonutWidget *m_donut = nullptr;
    QVBoxLayout *m_listLayout = nullptr;
    QVBoxLayout *m_teamLayout = nullptr;
    QVBoxLayout *m_actLayout = nullptr;
};

#endif // PROJECTSPAGE_H
