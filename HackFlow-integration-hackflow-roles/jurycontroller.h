#ifndef JURYCONTROLLER_H
#define JURYCONTROLLER_H

// ===========================================================================
//  Module « Jury et Evaluations » (integre dans HackFlow)
//  Les widgets sont dans mainwindow.ui (page « pageJury » du QStackedWidget
//  « stackedPages »). Cette classe contient toute la logique du module : tableau des
//  membres du jury (recherche, ajout, modification, suppression, tri, pagination,
//  export CSV) et fiche d'evaluation (criteres notes de 0 a 10). Elle ne change rien
//  aux autres pages de l'application.
// ===========================================================================

#include <QList>
#include <QObject>
#include <QString>

class QComboBox;
class QLineEdit;
class QMainWindow;
class QPlainTextEdit;
class QSlider;
class QTableWidgetItem;
class QWidget;

namespace Ui { class MainWindow; }

struct JuryMember {
    QString nom;
    QString prenom;
    QString email;
    QString telephone;
    QString specialite;
    QString entreprise;
};

class JuryController : public QObject
{
    Q_OBJECT

public:
    JuryController(Ui::MainWindow *ui, QMainWindow *host);

private slots:
    void onSearchChanged(const QString &text);
    void onAddMember();
    void onPrevPage();
    void onNextPage();
    void onItemChanged(QTableWidgetItem *item);
    void onEvalSave();
    void onEvalReset();

private:
    enum Column {
        ColNom = 0,
        ColPrenom,
        ColEmail,
        ColTelephone,
        ColSpecialite,
        ColEntreprise,
        ColActions,
        ColumnCount
    };

    void applyStyleSheet();
    void setupIcons();
    void setupTable();
    void setupEvaluationForm();
    void setupScrolling();
    void setupSortAndExport();
    void sortMembers(Column column, bool ascending);
    void exportMembers();
    void refreshJurorCombo();
    void updateTotalScore();
    void loadSampleData();
    void applyFilter();
    void refreshTable();
    void rebuildPagination(int pages);
    void deleteMember(int sourceIndex);
    bool askNewMember(JuryMember &m);
    int pageCount() const;

    Ui::MainWindow *ui;
    QWidget *m_root = nullptr;      // pageJury : parent des boites de dialogue (heritent du style)
    QList<JuryMember> m_all;
    QList<int> m_filtered;          // indices dans m_all (apres recherche)
    int m_page = 0;
    const int m_pageSize = 5;

    // Fiche d'evaluation
    QList<QSlider *> m_scoreSliders;
    QComboBox *m_cmbProject = nullptr;
    QComboBox *m_cmbJuror = nullptr;
    QComboBox *m_cmbReco = nullptr;
    QLineEdit *m_editTeam = nullptr;
    QPlainTextEdit *m_editComment = nullptr;
};

#endif // JURYCONTROLLER_H
