#ifndef EVALUATIONPROJETS_H
#define EVALUATIONPROJETS_H

#include <QDate>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

class QComboBox;
class QDateEdit;
class QDoubleSpinBox;
class QHBoxLayout;
class QLabel;
class QLineEdit;
class QMenu;
class QPlainTextEdit;
class QPushButton;
class QTableWidget;

/**
 * @brief Une évaluation de projet saisie avec la grille à 7 critères.
 * Stockée dans la table EVALUATION (la note finale reste dans « note », ce qui garde le classement
 * et les alertes de divergence des autres pages cohérents).
 */
struct EvalProjet
{
    static const int NbCriteres = 7;

    int id = -1;
    int idProjet = -1;
    QString titreProjet;
    QString descriptionProjet;
    QString equipe;
    int idJury = -1;
    QString nomJury;
    QString prenomJury;
    QString dateHeure;                 // « yyyy-MM-dd hh:mm »
    double note = 0;                   // note finale (/20)
    double criteres[NbCriteres] = {0, 0, 0, 0, 0, 0, 0};
    QString commentaire;

    QString nomCompletJury() const { return nomJury + " " + prenomJury; }
    QDate date() const { return QDate::fromString(dateHeure.left(10), "yyyy-MM-dd"); }
};

/**
 * @brief Page « Saisie et Grille d'Évaluation des Projets » (ouverte depuis le raccourci « Voir les
 * évaluations » de la page Jury) : formulaire de saisie à gauche, liste des projets évalués à droite.
 */
class EvaluationProjetsPage : public QWidget
{
    Q_OBJECT

public:
    explicit EvaluationProjetsPage(QWidget *parent = nullptr);

    void rafraichir();
    // Un non-administrateur peut saisir et consulter, mais pas modifier ni supprimer
    void definirModificationAutorisee(bool autorisee);

    // ---- Grille (constantes de la maquette) ----
    static QStringList nomsCriteres();
    static QStringList descriptionsCriteres();
    static QVector<int> poidsCriteres();                       // en %, dans l'ordre des critères
    static double calculerNoteFinale(const double notes[EvalProjet::NbCriteres]);

    // ---- Accès aux données (statiques : testables sans l'interface) ----
    static QVector<EvalProjet> lireEvaluations();
    static EvalProjet lireEvaluation(int id);
    static bool enregistrer(EvalProjet &e, QString *messageErreur = nullptr); // id < 0 : ajout, sinon modification
    static bool supprimer(int id);

signals:
    void retourDemande();
    void evaluationsModifiees();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void surProjetChange(int index);
    void surEquipeChange(int index);
    void surNoteChange();
    void surCommentaireChange();
    void enregistrerEvaluation();
    void reinitialiserFormulaire();
    void surFiltreChange();
    void surTriDemande(int colonne);
    void surCaseLigne();
    void allerPage(int page);

private:
    // Formulaire
    QLabel *m_lblTitreForm;
    QComboBox *m_cbProjet;
    QComboBox *m_cbEquipe;
    QComboBox *m_cbJury;
    QDateEdit *m_dateEval;
    QDoubleSpinBox *m_spins[EvalProjet::NbCriteres];
    QLabel *m_lblScores[EvalProjet::NbCriteres];
    QLabel *m_lblPoidsTotal;
    QLabel *m_lblNoteFinale;
    QPlainTextEdit *m_txtCommentaire;
    QLabel *m_lblCompteur;
    QPushButton *m_btnEnregistrer;
    QPushButton *m_btnReinitialiser;

    // Liste
    QLabel *m_lblNombre;
    QLineEdit *m_inputRecherche;
    QComboBox *m_cbFiltreJury;
    QComboBox *m_cbFiltreEquipe;
    QComboBox *m_cbFiltreDate;
    QPushButton *m_btnFiltrer;
    QMenu *m_menuFiltre;
    QTableWidget *m_table;
    QLabel *m_lblAffichage;
    QPushButton *m_btnPrecedent;
    QPushButton *m_btnSuivant;
    QHBoxLayout *m_pagesLayout;

    // Statistiques
    QLabel *m_lblMoyenne;
    QLabel *m_lblNbEvals;
    QLabel *m_lblDerniere;

    // État
    int m_idEnEdition;          // -1 : nouvelle évaluation
    QString m_filtreNote;       // "", "haute" (>= 16), "moyenne" (13 à 16), "basse" (< 13)
    int m_triColonne;           // 4 = Date, 5 = Note finale
    bool m_triCroissant;
    int m_page;
    int m_taillePage;
    bool m_modificationAutorisee;
    bool m_chargement;          // vrai pendant le remplissage des listes (évite les signaux en cascade)

    void construireInterface();
    QWidget *construireEnTete();
    QWidget *construireCarteFormulaire();
    QWidget *construireCarteListe();
    QWidget *construireCartesStats();
    void rechargerListesFormulaire();
    void rechargerFiltres();
    void chargerListe();
    void chargerStatistiques();
    void mettreAJourPagination(int total, int debut, int fin, int nbPages);
    void reconstruireMenuFiltre();
    void definirToutCoche(bool coche);
    bool toutCoche() const;
    QVector<EvalProjet> evaluationsFiltrees() const;
    void chargerDansFormulaire(const EvalProjet &e);
    void definirModeEdition(int id);
    void notesParDefaut();
    void detailEvaluation(int id);
    void confirmerSuppression(int id);
    void editer(int id);
};

#endif // EVALUATIONPROJETS_H
