#ifndef EMPLOYEESCONTROLLER_H
#define EMPLOYEESCONTROLLER_H

#include <QMap>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

class QCheckBox;
class QMainWindow;
class QMenu;
namespace Ui { class MainWindow; }

// ===========================================================================
//  Module « Employés » : logique de la page (formulaire, tableau, filtres, tri, export, graphiques)
//  Les donnees sont gardees dans <AppData>/HackFlow/employees.json (donnees d'exemple au 1er lancement).
//  Les droits (ajouter / modifier / supprimer / exporter / affecter) viennent de AuthManager.
// ===========================================================================
struct EmployeeRecord
{
    int id = 0;
    QString nom, prenom, email, role, dept, dispo;   // dispo : "Disponible" | "Indisponible"
    QStringList teams;
};

class EmployeesController : public QObject
{
    Q_OBJECT
public:
    EmployeesController(Ui::MainWindow *ui, QMainWindow *host, QCheckBox *headerCheck);

    void refresh();                         // reconstruit le tableau (filtres + tri + droits) et les graphiques
    void setSearchText(const QString &t);   // barre de recherche du haut de l'application
    void assignToTeam();                    // bouton « Affecter à une équipe » (analyse du CV)

private:
    // formulaire
    void onAdd();
    void onClear();
    void startEdit(int id);
    void cancelEdit();
    void setFormMode(bool editing);
    void resetForm();
    bool validateForm(QString *error, QWidget **focus) const;
    void setInfo(const QString &title, const QString &text, const char *kind);   // encadre d'information

    // actions
    void showProfile(int id);
    void onProfileButton();
    void deleteEmployee(int id);
    void onExport();
    void onSortMenu();

    // tableau
    QVector<int> visibleIds() const;
    QVector<int> checkedIds() const;
    int           singleTarget(const QString &caption);        // id choisi (case cochee sinon ligne), 0 si aucun
    void          fillRow(int row, const EmployeeRecord &e);
    void          updateHeaderCheck();
    void          updateCharts();
    int           indexOfId(int id) const;
    bool          emailTaken(const QString &email, int exceptId) const;

    // donnees
    void seed();
    void load();
    void save() const;

    Ui::MainWindow *ui;
    QMainWindow *m_host;
    QCheckBox *m_headCheck;
    QMenu *m_sortMenu = nullptr;

    QVector<EmployeeRecord> m_all;
    QSet<int> m_checked;
    int m_nextId = 1;
    int m_editId = 0;               // 0 = mode ajout, sinon id de l'employe modifie
    int m_sortKey = 0;              // 0 id, 1 nom, 2 prenom, 3 role, 4 dept, 5 disponibilite
    bool m_sortAsc = true;
    bool m_updatingChecks = false;
};

#endif // EMPLOYEESCONTROLLER_H
