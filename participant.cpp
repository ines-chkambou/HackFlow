#include "participant.h"
#include <QDebug>
#include <QSqlError>

Participant::Participant() : id(0) {}

Participant::Participant(int id, QString nom, QString prenom, QString email,
                         QString telephone, QString pieceJointe,
                         QString role, QString equipe, QString dispo)
    : id(id), nom(nom), prenom(prenom), email(email), telephone(telephone),
      pieceJointe(pieceJointe), role(role), equipe(equipe), dispo(dispo)
{
}

bool Participant::ajouter()
{
    QSqlQuery query;
    query.prepare("INSERT INTO PARTICIPANT "
                  "(MATRICULE, NOM, PRENOM, EMAIL, TEL, CV, ROLE, EQUIPE, DISPO) "
                  "VALUES (:id, :nom, :prenom, :email, :tel, :pj, :role, :equipe, :dispo)");
    query.bindValue(":id", id);
    query.bindValue(":nom", nom);
    query.bindValue(":prenom", prenom);
    query.bindValue(":email", email);
    query.bindValue(":tel", telephone);
    query.bindValue(":pj", pieceJointe);
    query.bindValue(":role", role);
    query.bindValue(":equipe", equipe);
    query.bindValue(":dispo", dispo);

    if (!query.exec()) {
        qDebug() << "Erreur ajout :" << query.lastError().text();
        return false;
    }
    return true;
}

bool Participant::modifier()
{
    QSqlQuery query;
    query.prepare("UPDATE PARTICIPANT SET NOM=:nom, PRENOM=:prenom, EMAIL=:email, "
                  "TEL=:tel, CV=:pj, ROLE=:role, EQUIPE=:equipe, DISPO=:dispo "
                  "WHERE MATRICULE=:id");
    query.bindValue(":id", id);
    query.bindValue(":nom", nom);
    query.bindValue(":prenom", prenom);
    query.bindValue(":email", email);
    query.bindValue(":tel", telephone);
    query.bindValue(":pj", pieceJointe);
    query.bindValue(":role", role);
    query.bindValue(":equipe", equipe);
    query.bindValue(":dispo", dispo);

    if (!query.exec()) {
        qDebug() << "Erreur modification :" << query.lastError().text();
        return false;
    }
    return query.numRowsAffected() > 0;
}

bool Participant::supprimer(int id)
{
    QSqlQuery query;
    query.prepare("DELETE FROM PARTICIPANT WHERE MATRICULE = :id");
    query.bindValue(":id", id);

    if (!query.exec()) {
        qDebug() << "Erreur suppression :" << query.lastError().text();
        return false;
    }
    return query.numRowsAffected() > 0;
}

QSqlQueryModel *Participant::afficher()
{
    QSqlQueryModel *model = new QSqlQueryModel();
    model->setQuery("SELECT MATRICULE, NOM, PRENOM, EMAIL, TEL, CV, ROLE, EQUIPE, DISPO "
                    "FROM PARTICIPANT ORDER BY MATRICULE");
    model->setHeaderData(0, Qt::Horizontal, QObject::tr("ID"));
    model->setHeaderData(1, Qt::Horizontal, QObject::tr("Nom"));
    model->setHeaderData(2, Qt::Horizontal, QObject::tr("Prénom"));
    model->setHeaderData(3, Qt::Horizontal, QObject::tr("Email"));
    model->setHeaderData(4, Qt::Horizontal, QObject::tr("Téléphone"));
    model->setHeaderData(5, Qt::Horizontal, QObject::tr("CV"));
    model->setHeaderData(6, Qt::Horizontal, QObject::tr("Rôle"));
    model->setHeaderData(7, Qt::Horizontal, QObject::tr("Équipe"));
    model->setHeaderData(8, Qt::Horizontal, QObject::tr("Disponibilité"));
    return model;
}

Participant Participant::chercher(int id, bool *ok)
{
    Participant p;
    QSqlQuery query;
    query.prepare("SELECT NOM, PRENOM, EMAIL, TEL, CV, ROLE, EQUIPE, DISPO "
                  "FROM PARTICIPANT WHERE MATRICULE = :id");
    query.bindValue(":id", id);

    const bool found = query.exec() && query.next();
    if (found) {
        p = Participant(id, query.value(0).toString(), query.value(1).toString(),
                        query.value(2).toString(), query.value(3).toString(),
                        query.value(4).toString(), query.value(5).toString(),
                        query.value(6).toString(), query.value(7).toString());
    }
    if (ok) *ok = found;
    return p;
}

QList<Participant> Participant::lister(bool *ok)
{
    QList<Participant> liste;
    QSqlQuery query;
    const bool success = query.exec("SELECT MATRICULE, NOM, PRENOM, EMAIL, TEL, CV, ROLE, EQUIPE, DISPO "
                                    "FROM PARTICIPANT ORDER BY MATRICULE");
    if (!success) {
        qDebug() << "Erreur lecture :" << query.lastError().text();
    } else {
        while (query.next()) {
            liste.append(Participant(query.value(0).toInt(),    query.value(1).toString(),
                                     query.value(2).toString(), query.value(3).toString(),
                                     query.value(4).toString(), query.value(5).toString(),
                                     query.value(6).toString(), query.value(7).toString(),
                                     query.value(8).toString()));
        }
    }
    if (ok) *ok = success;
    return liste;
}

int Participant::prochainId()
{
    QSqlQuery query("SELECT NVL(MAX(MATRICULE), 0) + 1 FROM PARTICIPANT");
    return query.next() ? query.value(0).toInt() : 1;
}
