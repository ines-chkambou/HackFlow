#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
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

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    // construction de l'interface
    QWidget *buildSidebar();
    QWidget *buildTopBar();
    QWidget *buildProjectsPage();
    QWidget *buildPlaceholder(const QString &title, const QString &text);
    QWidget *buildRightPanel();
    QWidget *makeStat(const QString &icon, const QString &color,
                      QLabel *&number, const QString &caption);
    QWidget *makeCard(const Project &p);

    // logique
    bool loadProjectsFromDatabase();
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
    void gotoPage(int index);

    QVector<Project> m_projects;
    QVector<Activity> m_activities;
    int m_nextId = 1;
    int m_notif = 0;
    bool m_gridMode = false;

    QStackedWidget *m_stack = nullptr;
    QLineEdit *m_globalSearch = nullptr;
    QLineEdit *m_projectSearch = nullptr;
    QComboBox *m_statusBox = nullptr;
    QComboBox *m_teamBox = nullptr;
    QPushButton *m_gridBtn = nullptr;
    QPushButton *m_listBtn = nullptr;
    QPushButton *m_bellBtn = nullptr;
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

#endif // MAINWINDOW_H

