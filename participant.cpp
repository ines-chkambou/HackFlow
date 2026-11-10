#include "participant.h"
#include <QDebug>
#include <QSqlError>
#include <QRegularExpression>
#include <QStringList>
#include <algorithm>

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

// ===========================================================================
//  Contrôle de saisie : Nom / Prénom
//  Règle : uniquement des lettres (accents et arabe acceptés), mots séparés
//  par UN espace, un tiret ou une apostrophe. Ex. « abdul al said », « Ben-Ali ».
// ===========================================================================
bool Participant::nomValide(const QString &texte, QString *raison)
{
    const QString t = texte.trimmed();
    auto refuser = [&](const QString &r) { if (raison) *raison = r; return false; };

    if (t.isEmpty())
        return refuser("Champ obligatoire.");
    if (t.size() < 2)
        return refuser("Au moins 2 lettres.");
    if (t.size() > 50)
        return refuser("50 caractères maximum.");

    for (const QChar c : t)
        if (c.isDigit())
            return refuser("Les chiffres ne sont pas autorisés.");

    // \p{L} = n'importe quelle lettre ; [ '\-] = un séparateur suivi d'un autre mot
    static const QRegularExpression re(R"(^\p{L}+([ '\-]\p{L}+)*$)");
    if (!re.match(t).hasMatch())
        return refuser("Lettres uniquement (pas de symboles ni d'espaces doubles).");

    if (raison) raison->clear();
    return true;
}

// ---------------------------------------------------------------------------
//  Distance de Levenshtein : nombre minimum de modifications (ajout,
//  suppression, remplacement d'une lettre) pour passer de a à b.
//  Sert à détecter les fautes de frappe : « gmial.com » → « gmail.com ».
// ---------------------------------------------------------------------------
static int distanceEdition(const QString &a, const QString &b)
{
    QVector<int> prec(b.size() + 1), cour(b.size() + 1);
    for (int j = 0; j <= b.size(); ++j) prec[j] = j;
    for (int i = 1; i <= a.size(); ++i) {
        cour[0] = i;
        for (int j = 1; j <= b.size(); ++j) {
            const int cout = (a[i - 1] == b[j - 1]) ? 0 : 1;
            cour[j] = std::min({ prec[j] + 1, cour[j - 1] + 1, prec[j - 1] + cout });
        }
        std::swap(prec, cour);
    }
    return prec[b.size()];
}

// ===========================================================================
//  Contrôle de saisie : Email
//  Forme attendue :  partieLocale @ domaine . extension
//                    ex. nom.prenom@gmail.com, esm@yahoo.fr, x@esprit.tn
// ===========================================================================
bool Participant::emailValide(const QString &email, QString *raison, QString *suggestion)
{
    const QString e = email.trimmed().toLower();
    auto refuser = [&](const QString &r) { if (raison) *raison = r; return false; };
    if (suggestion) suggestion->clear();

    // 1) Structure générale : un seul « @ », rien de vide autour
    if (e.isEmpty())                 return refuser("Champ obligatoire.");
    if (e.contains(' '))             return refuser("L'email ne doit pas contenir d'espace.");
    if (e.count('@') != 1)           return refuser("L'email doit contenir un seul « @ ».");
    if (e.size() > 254)              return refuser("Email trop long.");

    const QString local   = e.section('@', 0, 0);   // avant @
    const QString domaine = e.section('@', 1, 1);   // après @

    // 2) Partie locale (avant @) : lettres, chiffres, . _ % + -
    if (local.isEmpty())             return refuser("Il manque le nom avant « @ ».");
    if (local.size() > 64)           return refuser("Partie avant « @ » trop longue.");
    static const QRegularExpression reLocal(R"(^[a-z0-9._%+\-]+$)");
    if (!reLocal.match(local).hasMatch())
        return refuser("Caractère non autorisé avant « @ ».");
    if (local.startsWith('.') || local.endsWith('.') || local.contains(".."))
        return refuser("Point mal placé avant « @ ».");
    static const QRegularExpression reLettre("[a-z]");
    if (!reLettre.match(local).hasMatch())
        return refuser("La partie avant « @ » doit contenir au moins une lettre.");

    // 3) Domaine (après @) : au moins « nom.extension »
    if (domaine.isEmpty())           return refuser("Il manque le domaine après « @ » (ex. gmail.com).");
    if (!domaine.contains('.'))      return refuser("Il manque l'extension (ex. .com, .fr, .tn).");

    const QStringList parties = domaine.split('.');   // « mail.esprit.tn » → [mail, esprit, tn]
    static const QRegularExpression reEtiquette(R"(^[a-z0-9]([a-z0-9\-]*[a-z0-9])?$)");
    for (const QString &part : parties) {
        if (part.isEmpty())          return refuser("Point mal placé dans le domaine.");
        if (!reEtiquette.match(part).hasMatch())
            return refuser("Domaine invalide (lettres, chiffres et tirets uniquement).");
    }

    const QString nomDomaine = parties.at(parties.size() - 2);   // « gmail »
    const QString extension  = parties.last();                   // « com »

    if (nomDomaine.size() < 2 || !reLettre.match(nomDomaine).hasMatch())
        return refuser("Nom de domaine invalide (ex. gmail, yahoo, esprit).");

    // 4) Extension : uniquement des lettres ET une extension qui existe
    static const QRegularExpression reExt("^[a-z]{2,10}$");
    if (!reExt.match(extension).hasMatch())
        return refuser("Extension invalide : lettres uniquement (ex. .com, .fr, .tn).");

    static const QStringList extensionsConnues = {
        "com", "net", "org", "edu", "gov", "info", "biz", "io", "co", "me",
        "app", "dev", "tech", "online", "eu", "tn", "fr", "be", "ch", "ca",
        "de", "it", "es", "uk", "us", "ma", "dz", "ly", "eg", "sa", "ae", "qa"
    };
    if (!extensionsConnues.contains(extension))
        return refuser(QString("Extension « .%1 » inconnue.").arg(extension));

    // 5) Faute de frappe sur un fournisseur connu ? (gmial.com, yahooo.fr...)
    static const QStringList domainesConnus = {
        "gmail.com", "yahoo.com", "yahoo.fr", "hotmail.com", "hotmail.fr",
        "outlook.com", "outlook.fr", "live.com", "icloud.com", "esprit.tn"
    };
    if (!domainesConnus.contains(domaine)) {
        for (const QString &connu : domainesConnus) {
            const int d = distanceEdition(domaine, connu);
            if (d >= 1 && d <= 2) {
                if (suggestion) *suggestion = local + "@" + connu;
                return refuser(QString("Vouliez-vous dire %1@%2 ?").arg(local, connu));
            }
        }
    }

    if (raison) raison->clear();
    return true;
}

// ===========================================================================
//  Unicité : Email
//  Cherche dans la base un AUTRE participant (MATRICULE <> idIgnore) ayant
//  le même email. La comparaison ignore les majuscules et les espaces :
//  « Ines@Gmail.com » et « ines@gmail.com » sont le même email.
// ===========================================================================
bool Participant::emailExiste(const QString &email, int idIgnore)
{
    QSqlQuery query;
    query.prepare("SELECT COUNT(*) FROM PARTICIPANT "
                  "WHERE LOWER(TRIM(EMAIL)) = :email AND MATRICULE <> :id");
    query.bindValue(":email", email.trimmed().toLower());
    query.bindValue(":id", idIgnore);

    if (!query.exec() || !query.next()) {
        qDebug() << "Erreur vérification email :" << query.lastError().text();
        return false;
    }
    return query.value(0).toInt() > 0;
}

// ---------------------------------------------------------------------------
//  Met un numéro sous une forme unique pour pouvoir le comparer :
//  « +216 20 123 456 », « 0021620123456 », « 20 123 456 » → « 20123456 »
// ---------------------------------------------------------------------------
QString Participant::normaliserTelephone(const QString &tel)
{
    QString chiffres;
    for (const QChar c : tel)
        if (c.isDigit()) chiffres += c;                 // on garde uniquement les chiffres

    if (chiffres.startsWith("00216") && chiffres.size() == 13)
        chiffres.remove(0, 5);                          // 00216XXXXXXXX → XXXXXXXX
    else if (chiffres.startsWith("216") && chiffres.size() == 11)
        chiffres.remove(0, 3);                          // 216XXXXXXXX   → XXXXXXXX
    return chiffres;
}

// ===========================================================================
//  Unicité : Téléphone
//  Le même numéro peut être écrit de plusieurs façons (espaces, +216...),
//  donc on compare les numéros normalisés et non le texte brut.
// ===========================================================================
bool Participant::telephoneExiste(const QString &tel, int idIgnore)
{
    const QString cible = normaliserTelephone(tel);

    QSqlQuery query;
    query.prepare("SELECT TEL FROM PARTICIPANT WHERE MATRICULE <> :id");
    query.bindValue(":id", idIgnore);

    if (!query.exec()) {
        qDebug() << "Erreur vérification téléphone :" << query.lastError().text();
        return false;
    }
    while (query.next())
        if (normaliserTelephone(query.value(0).toString()) == cible)
            return true;
    return false;
}
