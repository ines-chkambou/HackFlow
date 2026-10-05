#ifndef CVASSIGNDIALOG_H
#define CVASSIGNDIALOG_H

#include <QDialog>
#include <QMap>
#include <QStringList>
#include "cvanalyzer.h"

class QLabel;
class QPushButton;
class QTableWidget;
class QComboBox;

struct EmployeeInfo {
    QString id, nom, prenom, email, role, dept, dispo;
    QStringList teams;
};

// Dialogue "Affecter a une equipe" : l'utilisateur charge le CV de l'employe, l'application
// analyse les competences, classe les equipes par compatibilite et propose la meilleure.
class CvAssignDialog : public QDialog
{
    Q_OBJECT
public:
    CvAssignDialog(const EmployeeInfo &emp, const QMap<QString, int> &teamLoad,
                   int maxTeamSize, QWidget *parent = nullptr);
    QString chosenTeam() const { return m_chosen; }
    // pour les tests : analyse un fichier sans passer par le selecteur
    void loadCvFile(const QString &path);

private slots:
    void onBrowse();
    void onConfirm();

private:
    void setStatus(const QString &text, const char *kind);   // "dlgError" | "dlgInfo" | "dlgOk"
    void showAnalysis(const CvAnalysis &a);

    EmployeeInfo m_emp;
    QMap<QString, int> m_load;
    int m_max;
    QString m_chosen;

    QLabel *m_cvName, *m_status, *m_suggest;
    QTableWidget *m_skills, *m_teams;
    QComboBox *m_combo;
    QPushButton *m_btnOk;
};

#endif
