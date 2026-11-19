#include "jury.h"
#include "sqlutils.h"
#include <QRegularExpression>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlRecord>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QDateTime>
#include <QPrinter>
#include <QPageSize>
#include <QPageLayout>
#include <QMarginsF>
#include <QTextDocument>
#include <QDebug>
#include <utility>

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
    const QString nomNettoye = nom.trimmed();
    const QString prenomNettoye = prenom.trimmed();
    const QString emailNettoye = email.trimmed();
    const QString telNettoye = telephone.trimmed();
    const QString roleNettoye = role.trimmed();
    const QString equipeNettoyee = equipe.trimmed();
    const QString dispoNettoyee = disponibilite.trimmed();

    if (nomNettoye.isEmpty()) {
        messageErreur = "Le nom du juré est obligatoire.";
        return false;
    }
    if (nomNettoye.size() > NomMax) {
        messageErreur = QString("Le nom ne peut pas dépasser %1 caractères.").arg(NomMax);
        return false;
    }
    if (prenomNettoye.isEmpty()) {
        messageErreur = "Le prénom du juré est obligatoire.";
        return false;
    }
    if (prenomNettoye.size() > PrenomMax) {
        messageErreur = QString("Le prénom ne peut pas dépasser %1 caractères.").arg(PrenomMax);
        return false;
    }
    if (emailNettoye.isEmpty()) {
        messageErreur = "L'adresse email est obligatoire.";
        return false;
    }
    if (emailNettoye.size() > EmailMax) {
        messageErreur = QString("L'adresse email ne peut pas dépasser %1 caractères.").arg(EmailMax);
        return false;
    }

    static QRegularExpression emailRegex("^[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\\.[A-Za-z]{2,}$");
    if (!emailRegex.match(emailNettoye).hasMatch()) {
        messageErreur = "Le format de l'adresse email est invalide (ex: nom@domaine.com).";
        return false;
    }

    if (telNettoye.isEmpty()) {
        messageErreur = "Le numéro de téléphone est obligatoire.";
        return false;
    }
    if (telNettoye.size() > TelephoneMax) {
        messageErreur = QString("Le numéro de téléphone ne peut pas dépasser %1 caractères.").arg(TelephoneMax);
        return false;
    }

    // Seuls les chiffres et les séparateurs usuels sont acceptés.
    static QRegularExpression telCaracteresRegex("^[0-9+()\\- ]+$");
    if (!telCaracteresRegex.match(telNettoye).hasMatch()) {
        messageErreur = "Le numéro de téléphone ne peut contenir que des chiffres, espaces, +, - et parenthèses.";
        return false;
    }
    int chiffres = 0;
    for (const QChar &c : telNettoye) {
        if (c.isDigit()) ++chiffres;
    }
    if (chiffres < 8) {
        messageErreur = "Le numéro de téléphone doit contenir au moins 8 chiffres (ex: +216 98 123 456).";
        return false;
    }

    if (roleNettoye.isEmpty()) {
        messageErreur = "Le rôle du juré est obligatoire (ex: Expert Tech).";
        return false;
    }
    if (roleNettoye.size() > RoleMax) {
        messageErreur = QString("Le rôle ne peut pas dépasser %1 caractères.").arg(RoleMax);
        return false;
    }
    if (equipeNettoyee.isEmpty()) {
        messageErreur = "L'équipe du juré est obligatoire (ex: Technologie).";
        return false;
    }
    if (equipeNettoyee.size() > EquipeMax) {
        messageErreur = QString("L'équipe ne peut pas dépasser %1 caractères.").arg(EquipeMax);
        return false;
    }
    if (dispoNettoyee.isEmpty()) {
        messageErreur = "La disponibilité est obligatoire.";
        return false;
    }
    if (dispoNettoyee.size() > DisponibiliteMax) {
        messageErreur = QString("La disponibilité ne peut pas dépasser %1 caractères.").arg(DisponibiliteMax);
        return false;
    }

    if (emailExiste(emailNettoye, id)) {
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
    if (!q.exec()) {
        signalerErreurSql(QStringLiteral("Impossible de vérifier l'adresse email du juré"), q.lastError());
        return false;
    }
    if (q.next()) {
        return q.value(0).toInt() > 0;
    }
    return false;
}

bool Jury::ajouter(QString *messageErreur)
{
    if (messageErreur) messageErreur->clear();
    id = -1;
    QString validation;
    if (!valider(validation)) {
        if (messageErreur) *messageErreur = validation;
        return false;
    }

    QSqlQuery q;
    q.prepare("INSERT INTO JURY (nom, prenom, email, telephone, role, equipe, disponibilite) "
              "VALUES (?, ?, ?, ?, ?, ?, ?)");
    q.addBindValue(nom.trimmed());
    q.addBindValue(prenom.trimmed());
    q.addBindValue(email.trimmed());
    q.addBindValue(telephone.trimmed());
    q.addBindValue(role.trimmed());
    q.addBindValue(equipe.trimmed());
    q.addBindValue(disponibilite.trimmed());
    if (!q.exec()) {
        const QString message = QStringLiteral("Impossible d'ajouter le juré : %1").arg(q.lastError().text());
        qWarning() << "Erreur ajout juré:" << q.lastError().text();
        if (messageErreur) *messageErreur = message;
        return false;
    }
    if (messageErreur) messageErreur->clear();
    return true;
}

bool Jury::modifier(int idJury, QString *messageErreur)
{
    if (messageErreur) messageErreur->clear();
    if (idJury <= 0) {
        if (messageErreur) *messageErreur = QStringLiteral("Identifiant de juré invalide.");
        return false;
    }

    id = idJury;
    QString validation;
    if (!valider(validation)) {
        if (messageErreur) *messageErreur = validation;
        return false;
    }

    QSqlQuery q;
    q.prepare("UPDATE JURY SET nom = ?, prenom = ?, email = ?, telephone = ?, "
              "role = ?, equipe = ?, disponibilite = ? WHERE id_jury = ?");
    q.addBindValue(nom.trimmed());
    q.addBindValue(prenom.trimmed());
    q.addBindValue(email.trimmed());
    q.addBindValue(telephone.trimmed());
    q.addBindValue(role.trimmed());
    q.addBindValue(equipe.trimmed());
    q.addBindValue(disponibilite.trimmed());
    q.addBindValue(idJury);
    if (!q.exec()) {
        const QString message = QStringLiteral("Impossible de modifier le juré : %1").arg(q.lastError().text());
        qWarning() << "Erreur modification juré:" << q.lastError().text();
        if (messageErreur) *messageErreur = message;
        return false;
    }
    if (messageErreur) messageErreur->clear();
    return true;
}

bool Jury::supprimer(int idJury)
{
    // Suppression en cascade manuelle dans une transaction :
    // les clés étrangères SQLite ne sont pas activées par défaut, et sous Oracle
    // les trois suppressions doivent être atomiques.
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.transaction()) {
        qWarning() << "Erreur ouverture transaction suppression juré:" << db.lastError().text();
        return false;
    }

    QSqlQuery q0;
    q0.prepare("DELETE FROM SESSION_EVALUATION WHERE id_jury = ?");
    q0.addBindValue(idJury);
    if (!q0.exec()) {
        qWarning() << "Erreur suppression sessions juré:" << q0.lastError().text();
        db.rollback();
        return false;
    }

    QSqlQuery q1;
    q1.prepare("DELETE FROM EVALUATION WHERE id_jury = ?");
    q1.addBindValue(idJury);
    if (!q1.exec()) {
        qWarning() << "Erreur suppression évaluations juré:" << q1.lastError().text();
        db.rollback();
        return false;
    }

    QSqlQuery q2;
    q2.prepare("DELETE FROM JURY WHERE id_jury = ?");
    q2.addBindValue(idJury);
    if (!q2.exec()) {
        qWarning() << "Erreur suppression juré:" << q2.lastError().text();
        db.rollback();
        return false;
    }

    if (!db.commit()) {
        qWarning() << "Erreur validation suppression juré:" << db.lastError().text();
        db.rollback();
        return false;
    }
    return true;
}

Jury Jury::obtenirParId(int idJury)
{
    Jury j;
    QSqlQuery q;
    q.prepare("SELECT id_jury, nom, prenom, email, telephone, role, equipe, disponibilite FROM JURY WHERE id_jury = ?");
    q.addBindValue(idJury);
    if (!q.exec()) {
        signalerErreurSql(QStringLiteral("Impossible de lire le juré #%1").arg(idJury), q.lastError());
        return j;
    }
    if (q.next()) {
        j = Jury(q.value(0).toInt(), q.value(1).toString(), q.value(2).toString(), q.value(3).toString(),
                 q.value(4).toString(), q.value(5).toString(), q.value(6).toString(), q.value(7).toString());
    }
    return j;
}

QSqlQueryModel* Jury::afficher()
{
    return rechercher();
}

static QString echapperLike(QString s)
{
    // Échappe les jokers SQL LIKE, puis le caractère d'échappement lui-même.
    s.replace("\\", "\\\\");
    s.replace("%", "\\%");
    s.replace("_", "\\_");
    return s;
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

    QList<QVariant> valeurs;

    const QString rech = recherche.trimmed();
    if (!rech.isEmpty()) {
        const QString motif = "%" + echapperLike(rech) + "%";
        req += "AND (LOWER(j.nom) LIKE LOWER(?) ESCAPE '\\' "
               "OR LOWER(j.prenom) LIKE LOWER(?) ESCAPE '\\' "
               "OR LOWER(j.prenom || ' ' || j.nom) LIKE LOWER(?) ESCAPE '\\' "
               "OR LOWER(j.email) LIKE LOWER(?) ESCAPE '\\' "
               "OR LOWER(j.telephone) LIKE LOWER(?) ESCAPE '\\' "
               "OR LOWER(j.role) LIKE LOWER(?) ESCAPE '\\' "
               "OR LOWER(j.equipe) LIKE LOWER(?) ESCAPE '\\') ";
        for (int i = 0; i < 7; ++i) valeurs.append(motif);
    }
    if (!role.trimmed().isEmpty()) {
        req += "AND j.role = ? ";
        valeurs.append(role.trimmed());
    }
    if (!equipe.trimmed().isEmpty()) {
        req += "AND j.equipe = ? ";
        valeurs.append(equipe.trimmed());
    }
    if (!dispo.trimmed().isEmpty()) {
        req += "AND j.disponibilite = ? ";
        valeurs.append(dispo.trimmed());
    }

    req += "GROUP BY j.id_jury, j.nom, j.prenom, j.email, j.telephone, j.role, j.equipe, j.disponibilite ";

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

    QSqlQuery q;
    if (!q.prepare(req)) {
        signalerErreurSql(QStringLiteral("Impossible de préparer la recherche des jurés"), q.lastError());
        return model;
    }
    for (const QVariant &valeur : valeurs) q.addBindValue(valeur);
    if (!q.exec()) {
        signalerErreurSql(QStringLiteral("Impossible de rechercher les jurés"), q.lastError());
        return model;
    }

    model->setQuery(std::move(q));
    if (model->lastError().isValid()) {
        signalerErreurSql(QStringLiteral("Impossible de charger le tableau des jurés"), model->lastError());
    }
    return model;
}

QStringList Jury::rolesDistincts()
{
    QStringList l;
    QSqlQuery q;
    if (!q.exec(QStringLiteral("SELECT role FROM JURY GROUP BY role ORDER BY MIN(id_jury) ASC"))) {
        signalerErreurSql(QStringLiteral("Impossible de charger la liste des rôles"), q.lastError());
        return l;
    }
    while (q.next()) l << q.value(0).toString();
    return l;
}

QStringList Jury::equipesDistinctes()
{
    QStringList l;
    QSqlQuery q;
    if (!q.exec(QStringLiteral("SELECT equipe FROM JURY GROUP BY equipe ORDER BY MIN(id_jury) ASC"))) {
        signalerErreurSql(QStringLiteral("Impossible de charger la liste des équipes"), q.lastError());
        return l;
    }
    while (q.next()) l << q.value(0).toString();
    return l;
}

int Jury::compterTotal()
{
    QSqlQuery q;
    if (!q.exec(QStringLiteral("SELECT COUNT(*) FROM JURY"))) {
        signalerErreurSql(QStringLiteral("Impossible de compter les jurés"), q.lastError());
        return 0;
    }
    return q.next() ? q.value(0).toInt() : 0;
}

int Jury::compterDisponibles()
{
    return compterParDisponibilite("Disponible");
}

int Jury::compterParDisponibilite(const QString &dispo)
{
    QSqlQuery q;
    q.prepare("SELECT COUNT(*) FROM JURY WHERE disponibilite = ?");
    q.addBindValue(dispo);
    if (!q.exec()) {
        signalerErreurSql(QStringLiteral("Impossible de compter les jurés disponibles"), q.lastError());
        return 0;
    }
    return q.next() ? q.value(0).toInt() : 0;
}

QVector<QPair<QString, int>> Jury::repartitionParRole()
{
    QVector<QPair<QString, int>> tous;
    QSqlQuery q;
    if (!q.exec(QStringLiteral("SELECT role, COUNT(*) AS n FROM JURY GROUP BY role ORDER BY n DESC, MIN(id_jury) ASC"))) {
        signalerErreurSql(QStringLiteral("Impossible de calculer la répartition par rôle"), q.lastError());
        return {};
    }
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

int Jury::nombreEvaluationsParJury(int idJury)
{
    QSqlQuery q;
    q.prepare("SELECT COUNT(*) FROM EVALUATION WHERE id_jury = ?");
    q.addBindValue(idJury);
    if (!q.exec()) {
        signalerErreurSql(QStringLiteral("Impossible de compter les évaluations du juré #%1").arg(idJury), q.lastError());
        return 0;
    }
    return q.next() ? q.value(0).toInt() : 0;
}

double Jury::noteMoyenneDonneeParJury(int idJury)
{
    QSqlQuery q;
    q.prepare("SELECT COALESCE(AVG(note), 0.0) FROM EVALUATION WHERE id_jury = ?");
    q.addBindValue(idJury);
    if (!q.exec()) {
        signalerErreurSql(QStringLiteral("Impossible de calculer la moyenne du juré #%1").arg(idJury), q.lastError());
        return 0.0;
    }
    return q.next() ? q.value(0).toDouble() : 0.0;
}

bool Jury::exporterCSV(const QString &cheminFichier)
{
    if (cheminFichier.trimmed().isEmpty()) {
        qWarning() << "Export CSV des jurés : chemin vide.";
        return false;
    }
    QFile file(cheminFichier);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Export CSV des jurés : ouverture impossible :" << file.errorString();
        return false;
    }

    QTextStream out(&file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    out.setCodec("UTF-8");
#endif
    // BOM UTF-8 pour Excel
    out.setGenerateByteOrderMark(true);

    auto champ = [](QString s) { return "\"" + s.replace("\"", "\"\"") + "\""; };

    out << "ID;Nom;Prénom;Email;Téléphone;Rôle;Équipe;Disponibilité;Nb Évaluations;Note Moyenne Donnée\n";

    QSqlQueryModel *model = afficher();
    if (model->lastError().isValid()) {
        qWarning() << "Export CSV des jurés : lecture impossible :" << model->lastError().text();
        delete model;
        file.close();
        return false;
    }
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

    out.flush();
    const bool ok = out.status() == QTextStream::Ok && file.error() == QFileDevice::NoError;
    if (!ok) qWarning() << "Export CSV des jurés : écriture incomplète :" << file.errorString();
    file.close();
    return ok;
}

bool Jury::exporterPDF(const QString &cheminFichier)
{
    if (cheminFichier.trimmed().isEmpty()) {
        qWarning() << "Export PDF des jurés : chemin vide.";
        return false;
    }
    QSqlQueryModel *model = afficher();
    if (model->lastError().isValid()) {
        qWarning() << "Export PDF des jurés : lecture impossible :" << model->lastError().text();
        delete model;
        return false;
    }
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
    if (!QFile::exists(cheminFichier) || QFileInfo(cheminFichier).size() <= 0) {
        qWarning() << "Export PDF des jurés : le fichier n'a pas été créé ou est vide :" << cheminFichier;
        return false;
    }
    return true;
}
