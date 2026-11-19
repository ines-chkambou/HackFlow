#ifndef EVALUATION_H
#define EVALUATION_H

#include <QString>
#include <QSqlQuery>
#include <QSqlQueryModel>
#include <QVector>
#include <QDateTime>

/**
 * @brief Structure représentant une ligne du classement des projets
 * avec gestion des ex æquo (Métier innovant 1)
 */
struct ClassementProjet {
    int rang;
    int idProjet;
    QString titre;
    QString equipe;
    double noteMoyenne;
    double scoreInnovation;
    double scoreTechnique;
    double scorePresentation;
    int nbEvaluations;
    QString mention;
    QString departageInfo; // Explication si ex æquo départagé
};

/**
 * @brief Structure représentant une alerte de divergence de notation (Métier innovant 2)
 */
struct AlerteDivergence {
    int idProjet;
    QString titreProjet;
    QString equipe;
    double noteMin;
    double noteMax;
    double ecart;
    double ecartType;
    QString jureNoteMin;
    QString jureNoteMax;
    QString niveauAlerte; // "CRITIQUE", "MODÉRÉE", "NORMAL"
    QString recommandation;
};

/**
 * @brief Entité métier Évaluation (Module 6)
 * id_evaluation (PK), id_jury (FK), id_projet (FK), note, commentaire, date_evaluation
 */
class Evaluation
{
private:
    int id;
    int idJury;
    int idProjet;
    double note;
    double critereInnovation;
    double critereTechnique;
    double criterePresentation;
    double criterePitch;
    double critQualite;
    double critImpact;
    double critBusiness;
    double critEquipe;
    // Vrai lorsque les quatre colonnes crit_* sont gérées par la grille à 7 critères.
    // Dans ce cas, critere_pitch n'est pas géré par le formulaire et doit être préservé.
    bool criteresEtendus;
    QString commentaire;
    QString dateEvaluation;

public:
    // Longueur maximale de la colonne Oracle EVALUATION.commentaire (VARCHAR2(1000 CHAR)).
    static const int CommentaireMax = 1000;

    Evaluation();
    Evaluation(int id, int idJury, int idProjet, double note,
               double innovation, double technique, double presentation, double pitch,
               const QString &commentaire, const QString &dateEvaluation);
    Evaluation(int idJury, int idProjet, double note,
               double innovation, double technique, double presentation, double pitch,
               const QString &commentaire);

    // Getters & Setters
    int getId() const { return id; }
    int getIdJury() const { return idJury; }
    int getIdProjet() const { return idProjet; }
    double getNote() const { return note; }
    double getCritereInnovation() const { return critereInnovation; }
    double getCritereTechnique() const { return critereTechnique; }
    double getCriterePresentation() const { return criterePresentation; }
    double getCriterePitch() const { return criterePitch; }
    double getCritQualite() const { return critQualite; }
    double getCritImpact() const { return critImpact; }
    double getCritBusiness() const { return critBusiness; }
    double getCritEquipe() const { return critEquipe; }
    bool hasCriteresEtendus() const { return criteresEtendus; }
    QString getCommentaire() const { return commentaire; }
    QString getDateEvaluation() const { return dateEvaluation; }

    void setId(int val) { id = val; }
    void setIdJury(int val) { idJury = val; }
    void setIdProjet(int val) { idProjet = val; }
    void setNote(double val) { note = val; }
    void setCritereInnovation(double val) { critereInnovation = val; }
    void setCritereTechnique(double val) { critereTechnique = val; }
    void setCriterePresentation(double val) { criterePresentation = val; }
    void setCriterePitch(double val) { criterePitch = val; }
    void setCritQualite(double val) { critQualite = val; }
    void setCritImpact(double val) { critImpact = val; }
    void setCritBusiness(double val) { critBusiness = val; }
    void setCritEquipe(double val) { critEquipe = val; }
    void setCriteresEtendus(bool val) { criteresEtendus = val; }
    void setCommentaire(const QString &val) { commentaire = val; }
    void setDateEvaluation(const QString &val) { dateEvaluation = val; }

    // Règle de gestion : note entre 0 et 20, unicité juré-projet
    bool valider(QString &messageErreur) const;
    static bool evaluationExiste(int idJury, int idProjet, int exclureId = -1);

    // Écriture unique des 13 colonnes métier (hors clé primaire) :
    // id_jury, id_projet, note, critere_innovation, critere_technique,
    // critere_presentation, critere_pitch, commentaire, date_evaluation,
    // crit_qualite, crit_impact, crit_business, crit_equipe.
    // Les colonnes non gérées par le formulaire sont NULL à l'insertion et
    // préservées lors d'une modification.
    static bool enregistrer(const Evaluation &e, bool modification,
                            int *idCree = nullptr, QString *messageErreur = nullptr);

    // CRUD
    bool ajouter(QString *messageErreur = nullptr);
    bool modifier(int id, QString *messageErreur = nullptr);
    static bool supprimer(int id);
    static Evaluation obtenirParId(int id);

    // Affichage & Recherches
    static QSqlQueryModel* afficher();
    static QSqlQueryModel* afficherParProjet(int idProjet);
    static QSqlQueryModel* afficherParJury(int idJury);

    // ==========================================
    // MÉTIERS INNOVANTS
    // ==========================================

    // 1. Classement automatique avec départage des ex æquo
    static QVector<ClassementProjet> calculerClassementAutomatique();

    // 2. Détection d'écart entre jurés (divergence de notation)
    static QVector<AlerteDivergence> detecterEcartsEntreJures(double seuilEcart = 4.0);

    // Export PDF du classement officiel
    static bool exporterClassementPDF(const QString &cheminFichier);
};

#endif // EVALUATION_H
