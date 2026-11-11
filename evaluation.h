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
    QString commentaire;
    QString dateEvaluation;

public:
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
    void setCommentaire(const QString &val) { commentaire = val; }
    void setDateEvaluation(const QString &val) { dateEvaluation = val; }

    // Règle de gestion : note entre 0 et 20, unicité juré-projet
    bool valider(QString &messageErreur) const;
    static bool evaluationExiste(int idJury, int idProjet, int exclureId = -1);

    // CRUD
    bool ajouter();
    bool modifier(int id);
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
