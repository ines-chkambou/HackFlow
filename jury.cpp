#include "jury.h"
#include <QRegularExpression>
#include <QSqlError>
#include <QSqlRecord>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QPrinter>
#include <QPageSize>
#include <QPageLayout>
#include <QMarginsF>
#include <QTextDocument>
#include <QDebug>

Jury::Jury() : id(-1), disponibilite("Disponible")
{
}

Jury::Jury(int id, const QString &nom, const QString &prenom, const QString &email,
           const QString &telephone, const QString &role, const QString &equipe,
           const QString &disponibilite)
    : id(id), nom(nom), prenom(prenom), email(email),
      telephone(telephone), role(role), equipe(equipe), disponibilite(disponibilite)
{
}

Jury::Jury(const QString &nom, const QString &prenom, const QString &email,
           const QString &telephone, const QString &role, const QString &equipe,
           const QString &disponibilite)
    : id(-1), nom(nom), prenom(prenom), email(email),
      telephone(telephone), role(role), equipe(equipe), disponibilite(disponibilite)
{
}

bool Jury::valider(QString &messageErreur) const
{
    if (nom.trimmed().isEmpty()) {
        messageErreur = "Le nom du juré est obligatoire.";
        return false;
    }
    if (prenom.trimmed().isEmpty()) {
        messageErreur = "Le prénom du juré est obligatoire.";
        return false;
    }
    if (email.trimmed().isEmpty()) {
        messageErreur = "L'adresse email est obligatoire.";
        return false;
    }

    static QRegularExpression emailRegex("^[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\\.[A-Za-z]{2,}$");
    if (!emailRegex.match(email.trimmed()).hasMatch()) {
        messageErreur = "Le format de l'adresse email est invalide (ex: nom@domaine.com).";
        return false;
    }

    static QRegularExpression telRegex("^\\+?[0-9 ]{8,17}$");
    if (!telRegex.match(telephone.trimmed()).hasMatch()) {
        messageErreur = "Le numéro de téléphone est invalide (ex: +216 98 123 456).";
        return false;
    }

    if (role.trimmed().isEmpty()) {
        messageErreur = "Le rôle du juré est obligatoire (ex: Expert Tech).";
        return false;
    }
    if (equipe.trimmed().isEmpty()) {
        messageErreur = "L'équipe du juré est obligatoire (ex: Technologie).";
        return false;
    }
    if (disponibilite.trimmed().isEmpty()) {
        messageErreur = "La disponibilité est obligatoire.";
        return false;
    }

    if (emailExiste(email.trimmed(), id)) {
        messageErreur = "Cette adresse email est déjà utilisée par un autre juré.";
        return false;
    }
    return true;
}

bool Jury::emailExiste(const QString &email, int exclureId)
{
    QSqlQuery q;
    if (exclureId > 0) {
        q.prepare("SELECT COUNT(*) FROM JURY WHERE LOWER(email) = LOWER(?) AND id_jury != ?");
        q.addBindValue(email.trimmed());
        q.addBindValue(exclureId);
    } else {
        q.prepare("SELECT COUNT(*) FROM JURY WHERE LOWER(email) = LOWER(?)");
        q.addBindValue(email.trimmed());
    }
    if (q.exec() && q.next()) {
        return q.value(0).toInt() > 0;
    }
    return false;
}

bool Jury::ajouter()
{
    QSqlQuery q;
    q.prepare("INSERT INTO JURY (nom, prenom, email, telephone, role, equipe, disponibilite) "
              "VALUES (?, ?, ?, ?, ?, ?, ?)");
    q.addBindValue(nom.trimmed());
    q.addBindValue(prenom.trimmed());
    q.addBindValue(email.trimmed());
    q.addBindValue(telephone.trimmed());
    q.addBindValue(role.trimmed());
    q.addBindValue(equipe.trimmed());
    q.addBindValue(disponibilite);
    if (!q.exec()) {
        qWarning() << "Erreur ajout juré:" << q.lastError().text();
        return false;
    }
    QSqlQuery().exec("COMMIT");
    return true;
}

bool Jury::modifier(int idJury)
{
    QSqlQuery q;
    q.prepare("UPDATE JURY SET nom = ?, prenom = ?, email = ?, telephone = ?, "
              "role = ?, equipe = ?, disponibilite = ? WHERE id_jury = ?");
    q.addBindValue(nom.trimmed());
    q.addBindValue(prenom.trimmed());
    q.addBindValue(email.trimmed());
    q.addBindValue(telephone.trimmed());
    q.addBindValue(role.trimmed());
    q.addBindValue(equipe.trimmed());
    q.addBindValue(disponibilite);
    q.addBindValue(idJury);
    if (!q.exec()) {
        qWarning() << "Erreur modification juré:" << q.lastError().text();
        return false;
    }
    QSqlQuery().exec("COMMIT");
    return true;
}

bool Jury::supprimer(int idJury)
{
    // Suppression en cascade manuelle (les clés étrangères SQLite ne sont pas activées par défaut)
    QSqlQuery q1;
    q1.prepare("DELETE FROM EVALUATION WHERE id_jury = ?");
    q1.addBindValue(idJury);
    q1.exec();

    QSqlQuery q2;
    q2.prepare("DELETE FROM JURY WHERE id_jury = ?");
    q2.addBindValue(idJury);
    if (!q2.exec()) {
        qWarning() << "Erreur suppression juré:" << q2.lastError().text();
        return false;
    }
    QSqlQuery().exec("COMMIT");
    return true;
}

Jury Jury::obtenirParId(int idJury)
{
    Jury j;
    QSqlQuery q;
    q.prepare("SELECT id_jury, nom, prenom, email, telephone, role, equipe, disponibilite FROM JURY WHERE id_jury = ?");
    q.addBindValue(idJury);
    if (q.exec() && q.next()) {
        j = Jury(q.value(0).toInt(), q.value(1).toString(), q.value(2).toString(), q.value(3).toString(),
                 q.value(4).toString(), q.value(5).toString(), q.value(6).toString(), q.value(7).toString());
    }
    return j;
}

QSqlQueryModel* Jury::afficher()
{
    return rechercher();
}

static QString echapperSql(const QString &s)
{
    QString r = s;
    return r.replace("'", "''");
}

QSqlQueryModel* Jury::rechercher(const QString &recherche, const QString &role, const QString &equipe,
                                 const QString &dispo, const QString &critere, const QString &ordre)
{
    QSqlQueryModel *model = new QSqlQueryModel();

    QString req = "SELECT j.id_jury, j.nom, j.prenom, j.email, j.telephone, j.role, j.equipe, j.disponibilite, "
                  "COUNT(e.id_evaluation) AS nb_evals, "
                  "COALESCE(ROUND(AVG(e.note), 2), 0.0) AS note_moyenne "
                  "FROM JURY j "
                  "LEFT JOIN EVALUATION e ON j.id_jury = e.id_jury "
                  "WHERE 1=1 ";

    const QString rech = echapperSql(recherche.trimmed());
    if (!rech.isEmpty()) {
        req += QString("AND (LOWER(j.nom) LIKE LOWER('%%1%') "
                       "OR LOWER(j.prenom) LIKE LOWER('%%1%') "
                       "OR LOWER(j.prenom || ' ' || j.nom) LIKE LOWER('%%1%') "
                       "OR LOWER(j.email) LIKE LOWER('%%1%') "
                       "OR LOWER(j.telephone) LIKE LOWER('%%1%') "
                       "OR LOWER(j.role) LIKE LOWER('%%1%') "
                       "OR LOWER(j.equipe) LIKE LOWER('%%1%')) ").arg(rech);
    }
    if (!role.isEmpty())   req += QString("AND j.role = '%1' ").arg(echapperSql(role));
    if (!equipe.isEmpty()) req += QString("AND j.equipe = '%1' ").arg(echapperSql(equipe));
    if (!dispo.isEmpty())  req += QString("AND j.disponibilite = '%1' ").arg(echapperSql(dispo));

    req += "GROUP BY j.id_jury ";

    QString colonne = "j.id_jury";
    if (critere == "Nom")                  colonne = "LOWER(j.nom)";
    else if (critere == "Prénom")          colonne = "LOWER(j.prenom)";
    else if (critere == "Rôle")            colonne = "LOWER(j.role)";
    else if (critere == "Équipe")          colonne = "LOWER(j.equipe)";
    else if (critere == "Disponibilité")   colonne = "LOWER(j.disponibilite)";
    else if (critere == "Évaluations")     colonne = "nb_evals";
    else if (critere == "Note Moyenne")    colonne = "note_moyenne";

    const QString sens = (ordre.toUpper() == "DESC") ? "DESC" : "ASC";
    req += QString("ORDER BY %1 %2, j.id_jury ASC").arg(colonne, sens);

    model->setQuery(req);
    if (model->lastError().isValid()) {
        qWarning() << "Erreur requête jurys:" << model->lastError().text();
    }
    return model;
}

QStringList Jury::rolesDistincts()
{
    QStringList l;
    QSqlQuery q("SELECT role FROM JURY GROUP BY role ORDER BY MIN(id_jury) ASC");
    while (q.next()) l << q.value(0).toString();
    return l;
}

QStringList Jury::equipesDistinctes()
{
    QStringList l;
    QSqlQuery q("SELECT equipe FROM JURY GROUP BY equipe ORDER BY MIN(id_jury) ASC");
    while (q.next()) l << q.value(0).toString();
    return l;
}

int Jury::compterTotal()
{
    QSqlQuery q("SELECT COUNT(*) FROM JURY");
    return q.next() ? q.value(0).toInt() : 0;
}

int Jury::compterDisponibles()
{
    return compterParDisponibilite("Disponible");
}

int Jury::compterParDisponibilite(const QString &dispo)
{
    QSqlQuery q;
    q.prepare("SELECT COUNT(*) FROM JURY WHERE disponibilite = :d");
    q.bindValue(":d", dispo);
    return (q.exec() && q.next()) ? q.value(0).toInt() : 0;
}

QVector<QPair<QString, int>> Jury::repartitionParRole()
{
    QVector<QPair<QString, int>> tous;
    QSqlQuery q("SELECT role, COUNT(*) AS n FROM JURY GROUP BY role ORDER BY n DESC, MIN(id_jury) ASC");
    while (q.next()) tous.append(qMakePair(q.value(0).toString(), q.value(1).toInt()));

    QVector<QPair<QString, int>> res;
    int autres = 0;
    for (int i = 0; i < tous.size(); ++i) {
        if (i < 5) res.append(tous[i]);
        else autres += tous[i].second;
    }
    if (autres > 0) res.append(qMakePair(QString("Autres"), autres));
    return res;
}

QMap<QString, double> Jury::moyennesParJury()
{
    QMap<QString, double> m;
    QSqlQuery q("SELECT j.prenom || ' ' || j.nom, COUNT(e.id_evaluation) "
                "FROM JURY j LEFT JOIN EVALUATION e ON j.id_jury = e.id_jury "
                "GROUP BY j.id_jury ORDER BY j.id_jury ASC");
    while (q.next()) m.insert(q.value(0).toString(), q.value(1).toDouble());
    return m;
}

int Jury::nombreEvaluationsParJury(int idJury)
{
    QSqlQuery q;
    q.prepare("SELECT COUNT(*) FROM EVALUATION WHERE id_jury = :id");
    q.bindValue(":id", idJury);
    return (q.exec() && q.next()) ? q.value(0).toInt() : 0;
}

double Jury::noteMoyenneDonneeParJury(int idJury)
{
    QSqlQuery q;
    q.prepare("SELECT COALESCE(AVG(note), 0.0) FROM EVALUATION WHERE id_jury = :id");
    q.bindValue(":id", idJury);
    return (q.exec() && q.next()) ? q.value(0).toDouble() : 0.0;
}

bool Jury::exporterCSV(const QString &cheminFichier)
{
    QFile file(cheminFichier);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return false;

    QTextStream out(&file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    out.setCodec("UTF-8");
#endif
    // BOM UTF-8 pour Excel
    out.setGenerateByteOrderMark(true);

    auto champ = [](QString s) { return "\"" + s.replace("\"", "\"\"") + "\""; };

    out << "ID;Nom;Prénom;Email;Téléphone;Rôle;Équipe;Disponibilité;Nb Évaluations;Note Moyenne Donnée\n";

    QSqlQueryModel *model = afficher();
    while (model->canFetchMore()) model->fetchMore();
    for (int i = 0; i < model->rowCount(); ++i) {
        QSqlRecord r = model->record(i);
        out << r.value(0).toInt() << ";"
            << champ(r.value(1).toString()) << ";" << champ(r.value(2).toString()) << ";"
            << champ(r.value(3).toString()) << ";" << champ(r.value(4).toString()) << ";"
            << champ(r.value(5).toString()) << ";" << champ(r.value(6).toString()) << ";"
            << champ(r.value(7).toString()) << ";"
            << r.value(8).toInt() << ";"
            << QString::number(r.value(9).toDouble(), 'f', 2).replace('.', ',') << "\n";
    }
    delete model;
    file.close();
    return true;
}

bool Jury::exporterPDF(const QString &cheminFichier)
{
    QSqlQueryModel *model = afficher();
    while (model->canFetchMore()) model->fetchMore();

    const QString dateGeneration = QDateTime::currentDateTime().toString("dd/MM/yyyy HH:mm");

    QString html =
        "<html><head><style>"
        "body { font-family: 'Segoe UI', Arial, sans-serif; color: #0F172A; }"
        "h1 { color: #0369A1; font-size: 20pt; margin-bottom: 2px; }"
        "p.sub { color: #475569; font-size: 10pt; margin-top: 0; }"
        "table { border-collapse: collapse; width: 100%; }"
        "th { background-color: #0F2A63; color: #FFFFFF; font-size: 9pt; padding: 6px; text-align: left; }"
        "td { border-bottom: 1px solid #CBD5E1; font-size: 9pt; padding: 5px; }"
        "</style></head><body>"
        "<h1>HackFlow — Membres du Jury</h1>"
        "<p class='sub'>Document généré le " + dateGeneration + " — " + QString::number(model->rowCount()) + " juré(s)</p>"
        "<table><tr><th>ID</th><th>Nom</th><th>Prénom</th><th>Email</th><th>Téléphone</th>"
        "<th>Rôle</th><th>Équipe</th><th>Disponibilité</th><th>Évals</th><th>Moyenne</th></tr>";

    for (int i = 0; i < model->rowCount(); ++i) {
        QSqlRecord r = model->record(i);
        html += "<tr><td>#" + r.value(0).toString() + "</td>"
                "<td><b>" + r.value(1).toString().toHtmlEscaped() + "</b></td>"
                "<td>" + r.value(2).toString().toHtmlEscaped() + "</td>"
                "<td>" + r.value(3).toString().toHtmlEscaped() + "</td>"
                "<td>" + r.value(4).toString().toHtmlEscaped() + "</td>"
                "<td>" + r.value(5).toString().toHtmlEscaped() + "</td>"
                "<td>" + r.value(6).toString().toHtmlEscaped() + "</td>"
                "<td>" + r.value(7).toString().toHtmlEscaped() + "</td>"
                "<td>" + r.value(8).toString() + "</td>"
                "<td>" + QString::number(r.value(9).toDouble(), 'f', 1) + " / 20</td></tr>";
    }
    html += "</table></body></html>";
    delete model;

    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(cheminFichier);
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    printer.setPageSize(QPageSize(QPageSize::A4));
    printer.setPageMargins(QMarginsF(15, 15, 15, 15), QPageLayout::Millimeter);
#else
    printer.setPageSize(QPrinter::A4);
    printer.setPageMargins(15, 15, 15, 15, QPrinter::Millimeter);
#endif

    QTextDocument doc;
    doc.setHtml(html);
    doc.print(&printer);
    return true;
}
