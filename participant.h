#ifndef PARTICIPANT_H
#define PARTICIPANT_H

#include <QString>
#include <QList>
#include <QSqlQuery>
#include <QSqlQueryModel>

class Participant
{
public:
    // Constructeurs
    Participant();
    Participant(int id, QString nom, QString prenom, QString email,
                QString telephone, QString pieceJointe,
                QString role, QString equipe, QString dispo);

    // Getters
    int     getId() const          { return id; }
    QString getNom() const         { return nom; }
    QString getPrenom() const      { return prenom; }
    QString getEmail() const       { return email; }
    QString getTelephone() const   { return telephone; }
    QString getPieceJointe() const { return pieceJointe; }
    QString getRole() const        { return role; }
    QString getEquipe() const      { return equipe; }
    QString getDispo() const       { return dispo; }

    // Setters
    void setId(int v)                     { id = v; }
    void setNom(const QString &v)         { nom = v; }
    void setPrenom(const QString &v)      { prenom = v; }
    void setEmail(const QString &v)       { email = v; }
    void setTelephone(const QString &v)   { telephone = v; }
    void setPieceJointe(const QString &v) { pieceJointe = v; }
    void setRole(const QString &v)        { role = v; }
    void setEquipe(const QString &v)      { equipe = v; }
    void setDispo(const QString &v)       { dispo = v; }

    // Fonctionnalités de base relatives à l'entité Participant
    bool ajouter();
    bool modifier();
    bool supprimer(int id);
    QSqlQueryModel *afficher();
    static Participant chercher(int id, bool *ok = nullptr);
    static QList<Participant> lister(bool *ok = nullptr);   // SELECT de tous les participants
    static int prochainId();          // MAX(ID) + 1

    // Contrôles de saisie (retournent false + la raison si invalide)
    static bool nomValide(const QString &texte, QString *raison = nullptr);
    static bool emailValide(const QString &email, QString *raison = nullptr,
                            QString *suggestion = nullptr);

private:
    int     id;
    QString nom, prenom, email, telephone, pieceJointe, role, equipe, dispo;
};

#endif // PARTICIPANT_H
