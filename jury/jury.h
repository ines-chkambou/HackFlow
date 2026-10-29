#ifndef JURY_H
#define JURY_H

#include <QString>
#include <QStringList>
#include <QSqlQuery>
#include <QSqlQueryModel>
#include <QMap>
#include <QVector>
#include <QPair>

/**
 * @brief Entité métier Jury (module 6 du cahier des spécifications Smart Hackathon).
 * Attributs : id_jury (PK), nom, prenom, email, telephone, role, equipe, disponibilite.
 *
 * Métiers de base :
 *  1. CRUD complet (ajouter, modifier, supprimer, afficher)
 *  2. Recherche multicritère (texte, rôle, équipe, disponibilité)
 *  3. Tri (nom, prénom, rôle, équipe, disponibilité, nombre d'évaluations, note moyenne)
 *  4. Exports PDF / CSV et statistiques (répartition par disponibilité et par rôle)
 */
class Jury
{
private:
    int id;
    QString nom;
    QString prenom;
    QString email;
    QString telephone;
    QString role;
    QString equipe;
    QString disponibilite;

public:
    Jury();
    Jury(int id, const QString &nom, const QString &prenom, const QString &email,
         const QString &telephone, const QString &role, const QString &equipe,
         const QString &disponibilite = "Disponible");
    Jury(const QString &nom, const QString &prenom, const QString &email,
         const QString &telephone, const QString &role, const QString &equipe,
         const QString &disponibilite = "Disponible");

    // Getters
    int getId() const { return id; }
    QString getNom() const { return nom; }
    QString getPrenom() const { return prenom; }
    QString getEmail() const { return email; }
    QString getTelephone() const { return telephone; }
    QString getRole() const { return role; }
    QString getEquipe() const { return equipe; }
    QString getDisponibilite() const { return disponibilite; }

    // Setters
    void setId(int newId) { id = newId; }
    void setNom(const QString &v) { nom = v; }
    void setPrenom(const QString &v) { prenom = v; }
    void setEmail(const QString &v) { email = v; }
    void setTelephone(const QString &v) { telephone = v; }
    void setRole(const QString &v) { role = v; }
    void setEquipe(const QString &v) { equipe = v; }
    void setDisponibilite(const QString &v) { disponibilite = v; }

    // Validation des données
    bool valider(QString &messageErreur) const;
    static bool emailExiste(const QString &email, int exclureId = -1);

    // CRUD
    bool ajouter();
    bool modifier(int id);
    static bool supprimer(int id);
    static Jury obtenirParId(int id);

    // Affichage / recherche / tri.
    // Colonnes du modèle : 0 id, 1 nom, 2 prénom, 3 email, 4 téléphone, 5 rôle, 6 équipe,
    //                      7 disponibilité, 8 nb évaluations, 9 note moyenne donnée
    static QSqlQueryModel* afficher();
    static QSqlQueryModel* rechercher(const QString &recherche = "", const QString &role = "",
                                      const QString &equipe = "", const QString &dispo = "",
                                      const QString &critere = "ID", const QString &ordre = "ASC");

    // Listes pour les filtres
    static QStringList rolesDistincts();
    static QStringList equipesDistinctes();

    // Statistiques
    static int compterTotal();
    static int compterDisponibles();
    static int compterParDisponibilite(const QString &dispo);
    // Top 5 des rôles (par effectif) + « Autres » regroupant le reste
    static QVector<QPair<QString, int>> repartitionParRole();
    static QMap<QString, double> moyennesParJury();
    static int nombreEvaluationsParJury(int idJury);
    static double noteMoyenneDonneeParJury(int idJury);

    // Exports
    static bool exporterCSV(const QString &cheminFichier);
    static bool exporterPDF(const QString &cheminFichier);
};

#endif // JURY_H
