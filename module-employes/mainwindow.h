#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QVector>
#include <QStringList>

class QCheckBox;
namespace Ui { class MainWindow; }

struct Employe {
    int         id = 0;
    QString     nom;
    QString     prenom;
    QString     email;
    QString     role;
    QString     dept;
    QStringList equipes;
    QString     dispo;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onAjouter();
    void onVider();
    void onProfil();
    void onAffecter();
    void onSortChosen(QAction *a);
    void onExport(QAction *a);
    void refreshTable();

private:
    Ui::MainWindow *ui;
    QCheckBox *m_headCheck = nullptr;
    QVector<Employe> m_data;
    QStringList m_teams;
    int     m_nextId  = 1;
    int     m_editId  = -1;
    QString m_sortKey = "id";

    void seedData();
    void setupTable();
    void setupMenus();
    void setupNavigation();
    void positionHeaderCheck();
    void refreshStats();
    void setFormTitle(bool editing);
    QVector<Employe> filtered() const;
    QVector<int> checkedIds() const;
    int  indexOfId(int id) const;
    int  selectedId() const;
    bool readForm(Employe &e);
    void loadToForm(const Employe &e);
    void startEdit(int id);
    void deleteEmploye(int id);
    void showProfile(int id);
    void exportCsv(const QVector<Employe> &rows);
    void exportPdf(const QVector<Employe> &rows);
    static QStringList splitTeams(const QString &s);
};

#endif // MAINWINDOW_H