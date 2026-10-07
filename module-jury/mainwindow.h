#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QHash>
#include <QList>
#include <QMainWindow>
#include <QString>

class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSlider;
class QTableWidgetItem;
class QWidget;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

struct JuryMember {
    QString nom;
    QString prenom;
    QString email;
    QString telephone;
    QString specialite;
    QString entreprise;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

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

    void setupNavigation();
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
    QWidget *makePlaceholderPage(const QString &title);

    Ui::MainWindow *ui;
    QList<JuryMember> m_all;
    QList<int> m_filtered;      // indices dans m_all (apres recherche)
    int m_page = 0;
    const int m_pageSize = 5;

    // Fiche d'evaluation (page Jury)
    QList<QSlider *> m_scoreSliders;
    QComboBox *m_cmbProject = nullptr;
    QComboBox *m_cmbJuror = nullptr;
    QComboBox *m_cmbReco = nullptr;
    QLineEdit *m_editTeam = nullptr;
    QPlainTextEdit *m_editComment = nullptr;
};

#endif // MAINWINDOW_H
