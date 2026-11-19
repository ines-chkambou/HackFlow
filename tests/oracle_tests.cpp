#include <QApplication>
#include <QDate>
#include <QDateTime>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlQueryModel>
#include <QStringConverter>
#include <QTextStream>
#include <QTime>
#include <QVariantList>
#include <QVector>

#include "connection.h"
#include "evaluation.h"
#include "jury.h"
#include "parametresjury.h"
#include "planningevaluations.h"

namespace {

struct ResultatTest
{
    QString nom;
    bool ok = false;
    QString message;
};

QString erreurSql(const QSqlError &e)
{
    return e.text().isEmpty() ? QStringLiteral("Erreur SQL inconnue") : e.text();
}

bool compter(QSqlDatabase &db, const QString &table, int &valeur, QString &erreur)
{
    QSqlQuery q(db);
    if (!q.exec(QStringLiteral("SELECT COUNT(*) FROM ") + table) || !q.next()) {
        erreur = erreurSql(q.lastError());
        return false;
    }
    valeur = q.value(0).toInt();
    return true;
}

bool executer(QSqlDatabase &db, const QString &sql, const QVariantList &valeurs, QString &erreur)
{
    QSqlQuery q(db);
    if (!q.prepare(sql)) {
        erreur = erreurSql(q.lastError());
        return false;
    }
    for (const QVariant &v : valeurs)
        q.addBindValue(v);
    if (!q.exec()) {
        erreur = erreurSql(q.lastError());
        return false;
    }
    return true;
}

int idLibre(QSqlDatabase &db, const QString &table, const QString &colonne, int depart, QString &erreur)
{
    for (int id = depart; id < depart + 1000; ++id) {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("SELECT COUNT(*) FROM ") + table
                  + QStringLiteral(" WHERE ") + colonne + QStringLiteral(" = ?"));
        q.addBindValue(id);
        if (!q.exec() || !q.next()) {
            erreur = erreurSql(q.lastError());
            return -1;
        }
        if (q.value(0).toInt() == 0)
            return id;
    }
    erreur = QStringLiteral("Aucun identifiant libre trouve pour ") + table;
    return -1;
}

bool insererJury(QSqlDatabase &db, int id, const QString &suffixe, QString &erreur)
{
    return executer(db,
                    QStringLiteral("INSERT INTO JURY "
                                   "(id_jury, nom, prenom, email, telephone, role, equipe, disponibilite) "
                                   "VALUES (?, ?, ?, ?, ?, ?, ?, ?)"),
                    {id,
                     QStringLiteral("TEST_Nom_") + suffixe,
                     QStringLiteral("TEST_Prenom_") + suffixe,
                     QStringLiteral("TEST_jury_") + suffixe.toLower() + QStringLiteral("@example.invalid"),
                     QStringLiteral("+216 90 123 456"),
                     QStringLiteral("TEST_Role"),
                     QStringLiteral("TEST_Equipe"),
                     QStringLiteral("Disponible")},
                    erreur);
}

bool insererProjet(QSqlDatabase &db, int id, QString &erreur)
{
    return executer(db,
                    QStringLiteral("INSERT INTO PROJET "
                                   "(id_projet, titre, nom_equipe, statut, description) "
                                   "VALUES (?, ?, ?, ?, ?)"),
                    {id,
                     QStringLiteral("TEST_Projet"),
                     QStringLiteral("TEST_EquipeProjet"),
                     QStringLiteral("En cours"),
                     QStringLiteral("TEST_Description")},
                    erreur);
}

bool insererSession(QSqlDatabase &db, int id, const QDate &date, const QTime &heure,
                    int idProjet, int idJury, const QString &salle, QString &erreur)
{
    return executer(db,
                    QStringLiteral("INSERT INTO SESSION_EVALUATION "
                                   "(id_session, date_session, heure, id_projet, id_jury, salle, statut) "
                                   "VALUES (?, ?, ?, ?, ?, ?, ?)"),
                    {id,
                     date.toString(QStringLiteral("yyyy-MM-dd")),
                     heure.toString(QStringLiteral("HH:mm")),
                     idProjet,
                     idJury,
                     salle,
                     QStringLiteral("Planifiée")},
                    erreur);
}

QString resultatConflit(const ResultatVerificationConflit &r)
{
    switch (r.etat) {
    case ResultatVerificationConflit::AucunConflit:
        return QStringLiteral("AucunConflit");
    case ResultatVerificationConflit::ConflitDetecte:
        return QStringLiteral("ConflitDetecte");
    case ResultatVerificationConflit::ErreurSql:
        return QStringLiteral("ErreurSql");
    }
    return QStringLiteral("Inconnu");
}

} // namespace

int main(int argc, char *argv[])
{
    QTextStream out(stdout);
    out.setEncoding(QStringConverter::Utf8);
    out << "Demarrage du harnais Oracle\n";
    out.flush();

    QApplication app(argc, argv);
    out << "QApplication initialisee\n";
    out.flush();

    out << "Ouverture de la connexion Oracle (configuration existante)\n";
    out.flush();
    Connection connexion;
    if (!connexion.createConnection()) {
        out << "KO | Connexion Oracle | " << connexion.getLastError() << "\n";
        out.flush();
        return 2;
    }
    out << "Connexion Oracle ouverte\n";
    out.flush();
    auto phase = [&out](const QString &texte) {
        out << "[phase] " << texte << "\n";
        out.flush();
    };

    QSqlDatabase db = QSqlDatabase::database();
    const QString pilote = db.driverName();
    if (pilote.compare(QStringLiteral("QODBC"), Qt::CaseInsensitive) != 0
        && pilote.compare(QStringLiteral("QOCI"), Qt::CaseInsensitive) != 0) {
        out << "KO | Connexion Oracle | moteur actif = " << pilote << "\n";
        return 2;
    }

    phase(QStringLiteral("SELECT USER FROM DUAL"));
    QString utilisateur;
    {
        QSqlQuery q(db);
        if (!q.exec(QStringLiteral("SELECT USER FROM DUAL")) || !q.next()) {
            out << "KO | SELECT USER FROM DUAL | " << erreurSql(q.lastError()) << "\n";
            return 3;
        }
        utilisateur = q.value(0).toString();
    }

    out << "Oracle user: " << utilisateur << "\n";
    out << "Driver: " << pilote << "\n";

    if (utilisateur.compare(QStringLiteral("INES"), Qt::CaseInsensitive) != 0) {
        out << "KO | Verification utilisateur | attendu INES, obtenu " << utilisateur << "\n";
        return 3;
    }

    phase(QStringLiteral("comptage initial"));
    int juryAvant = 0, projetAvant = 0, evaluationAvant = 0, sessionAvant = 0, parametresAvant = 0;
    QString erreur;
    if (!compter(db, QStringLiteral("JURY"), juryAvant, erreur)
        || !compter(db, QStringLiteral("PROJET"), projetAvant, erreur)
        || !compter(db, QStringLiteral("EVALUATION"), evaluationAvant, erreur)
        || !compter(db, QStringLiteral("SESSION_EVALUATION"), sessionAvant, erreur)
        || !compter(db, QStringLiteral("PARAMETRES_JURY"), parametresAvant, erreur)) {
        out << "KO | Comptage initial | " << erreur << "\n";
        return 4;
    }

    phase(QStringLiteral("ouverture transaction"));
    if (!db.transaction()) {
        out << "KO | Ouverture transaction | " << erreurSql(db.lastError()) << "\n";
        return 5;
    }

    QVector<ResultatTest> resultats;
    auto ajouter = [&resultats](const QString &nom, bool ok, const QString &message = QString()) {
        resultats.append({nom, ok, message});
    };

    phase(QStringLiteral("reservation des identifiants TEST_"));
    const int juryA = idLibre(db, QStringLiteral("JURY"), QStringLiteral("id_jury"), 990000001, erreur);
    const int juryB = idLibre(db, QStringLiteral("JURY"), QStringLiteral("id_jury"), 990000101, erreur);
    const int juryC = idLibre(db, QStringLiteral("JURY"), QStringLiteral("id_jury"), 990000201, erreur);
    const int projetId = idLibre(db, QStringLiteral("PROJET"), QStringLiteral("id_projet"), 990000001, erreur);
    const int evaluation9Id = idLibre(db, QStringLiteral("EVALUATION"), QStringLiteral("id_evaluation"), 990000001, erreur);
    const int evaluation7Id = idLibre(db, QStringLiteral("EVALUATION"), QStringLiteral("id_evaluation"), 990000101, erreur);
    const int sessionAId = idLibre(db, QStringLiteral("SESSION_EVALUATION"), QStringLiteral("id_session"), 990000001, erreur);
    const int sessionBId = idLibre(db, QStringLiteral("SESSION_EVALUATION"), QStringLiteral("id_session"), 990000101, erreur);

    if (juryA < 0 || juryB < 0 || juryC < 0 || projetId < 0
        || evaluation9Id < 0 || evaluation7Id < 0 || sessionAId < 0 || sessionBId < 0) {
        ajouter(QStringLiteral("Preparation des identifiants TEST_"), false, erreur);
    } else {
        ajouter(QStringLiteral("Preparation des identifiants TEST_"), true,
                QStringLiteral("IDs reserves sans utiliser les sequences"));
    }

    bool juryAOk = false, juryBOk = false, juryCOk = false;
    if (juryA > 0) {
        erreur.clear();
        juryAOk = insererJury(db, juryA, QStringLiteral("A"), erreur);
        ajouter(QStringLiteral("Jury : insertion TEST_A"), juryAOk, erreur);
    }
    if (juryB > 0) {
        erreur.clear();
        juryBOk = insererJury(db, juryB, QStringLiteral("B"), erreur);
        ajouter(QStringLiteral("Jury : insertion TEST_B"), juryBOk, erreur);
    }
    if (juryC > 0) {
        erreur.clear();
        juryCOk = insererJury(db, juryC, QStringLiteral("C"), erreur);
        ajouter(QStringLiteral("Jury : insertion TEST_C"), juryCOk, erreur);
    }

    if (juryAOk) {
        Jury valide(juryA, QStringLiteral("TEST_Nom_A"), QStringLiteral("TEST_Prenom_A"),
                    QStringLiteral("TEST_jury_a@example.invalid"), QStringLiteral("+216 90 123 456"),
                    QStringLiteral("TEST_Role"), QStringLiteral("TEST_Equipe"), QStringLiteral("Disponible"));
        QString message;
        ajouter(QStringLiteral("Jury : validation valide"), valide.valider(message), message);

        Jury modifie(juryA, QStringLiteral("TEST_Nom_A_Modifie"), QStringLiteral("TEST_Prenom_A"),
                     QStringLiteral("TEST_jury_a@example.invalid"), QStringLiteral("+216 90 123 456"),
                     QStringLiteral("TEST_Role"), QStringLiteral("TEST_Equipe"), QStringLiteral("Disponible"));
        message.clear();
        const bool modificationOk = modifie.modifier(juryA, &message);
        ajouter(QStringLiteral("Jury : modification TEST_A"), modificationOk, message);
        if (modificationOk) {
            const Jury relu = Jury::obtenirParId(juryA);
            ajouter(QStringLiteral("Jury : relecture apres modification"),
                    relu.getNom() == QStringLiteral("TEST_Nom_A_Modifie"), relu.getNom());
        }
    }

    {
        Jury doublon(QStringLiteral("TEST_Nom_Doublon"), QStringLiteral("TEST_Prenom_Doublon"),
                     QStringLiteral("TEST_jury_a@example.invalid"), QStringLiteral("+216 90 123 456"),
                     QStringLiteral("TEST_Role"), QStringLiteral("TEST_Equipe"), QStringLiteral("Disponible"));
        QString message;
        const bool refuse = !doublon.valider(message);
        ajouter(QStringLiteral("Jury : email en doublon refuse"), refuse,
                refuse ? QStringLiteral("refuse comme attendu") : QStringLiteral("doublon accepte"));
    }

    {
        Jury telephoneInvalide(QStringLiteral("TEST_Nom_Tel"), QStringLiteral("TEST_Prenom_Tel"),
                               QStringLiteral("TEST_tel@example.invalid"), QStringLiteral("123"),
                               QStringLiteral("TEST_Role"), QStringLiteral("TEST_Equipe"), QStringLiteral("Disponible"));
        QString message;
        const bool refuse = !telephoneInvalide.valider(message);
        ajouter(QStringLiteral("Jury : telephone invalide refuse"), refuse,
                refuse ? QStringLiteral("refuse comme attendu") : QStringLiteral("telephone accepte"));
    }

    {
        Jury nomTropLong(QString(101, QChar('X')), QStringLiteral("TEST_Prenom_Long"),
                         QStringLiteral("TEST_long@example.invalid"), QStringLiteral("+216 90 123 456"),
                         QStringLiteral("TEST_Role"), QStringLiteral("TEST_Equipe"), QStringLiteral("Disponible"));
        QString message;
        const bool refuse = !nomTropLong.valider(message);
        ajouter(QStringLiteral("Jury : nom > 100 refuse"), refuse,
                refuse ? QStringLiteral("refuse comme attendu") : QStringLiteral("nom trop long accepte"));
    }

    if (juryAOk) {
        QSqlQueryModel *modele = Jury::rechercher(QStringLiteral("TEST_"));
        const bool ok = modele && !modele->lastError().isValid() && modele->rowCount() >= 1;
        ajouter(QStringLiteral("Jury : recherche multicritere TEST_"), ok,
                modele ? (modele->lastError().isValid() ? erreurSql(modele->lastError()) : QString())
                       : QStringLiteral("modele nul"));
        delete modele;
    }

    phase(QStringLiteral("tests projet et evaluation"));
    bool projetOk = false;
    if (projetId > 0) {
        erreur.clear();
        projetOk = insererProjet(db, projetId, erreur);
        ajouter(QStringLiteral("Projet : insertion TEST_Projet"), projetOk, erreur);
    }

    if (projetOk && juryAOk) {
        erreur.clear();
        const bool ok9 = executer(db,
                                  QStringLiteral("INSERT INTO EVALUATION "
                                                 "(id_evaluation, id_jury, id_projet, note, "
                                                 " critere_innovation, critere_technique, critere_presentation, critere_pitch, "
                                                 " commentaire, date_evaluation, "
                                                 " crit_qualite, crit_impact, crit_business, crit_equipe) "
                                                 "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, NULL, NULL, NULL, NULL)"),
                                  {evaluation9Id, juryA, projetId, 15.5, 16.0, 15.0, 14.0, 17.0,
                                   QStringLiteral("TEST_commentaire_9"), QStringLiteral("2030-01-15 10:00")},
                                  erreur);
        ajouter(QStringLiteral("Evaluation : insertion 9 criteres"), ok9, erreur);

        if (ok9) {
            QSqlQuery q(db);
            q.prepare(QStringLiteral("SELECT critere_pitch, crit_qualite, crit_impact, crit_business, crit_equipe "
                                     "FROM EVALUATION WHERE id_evaluation = ?"));
            q.addBindValue(evaluation9Id);
            bool ok = q.exec() && q.next();
            QString message;
            if (!ok) {
                message = erreurSql(q.lastError());
            } else {
                ok = q.value(0).toDouble() == 17.0
                     && q.value(1).isNull() && q.value(2).isNull()
                     && q.value(3).isNull() && q.value(4).isNull();
                if (!ok) message = QStringLiteral("colonnes crit_* non NULL ou pitch incorrect");
            }
            ajouter(QStringLiteral("Evaluation : 9 criteres, crit_* NULL"), ok, message);

            const Evaluation relue = Evaluation::obtenirParId(evaluation9Id);
            ajouter(QStringLiteral("Evaluation : relecture 9 criteres"),
                    relue.getId() == evaluation9Id && relue.getCriterePitch() == 17.0
                        && relue.getCritQualite() == 0.0,
                    QStringLiteral("pitch=%1 qualite=%2").arg(relue.getCriterePitch()).arg(relue.getCritQualite()));
        }

        erreur.clear();
        const bool ok7 = executer(db,
                                  QStringLiteral("INSERT INTO EVALUATION "
                                                 "(id_evaluation, id_jury, id_projet, note, "
                                                 " critere_innovation, critere_technique, critere_presentation, critere_pitch, "
                                                 " commentaire, date_evaluation, "
                                                 " crit_qualite, crit_impact, crit_business, crit_equipe) "
                                                 "VALUES (?, ?, ?, ?, ?, ?, ?, NULL, ?, ?, ?, ?, ?, ?)"),
                                  {evaluation7Id, juryB, projetId, 16.5, 15.0, 16.0, 17.0,
                                   QStringLiteral("TEST_commentaire_7"), QStringLiteral("2030-01-15 11:00"),
                                   18.0, 17.0, 16.0, 15.0},
                                  erreur);
        ajouter(QStringLiteral("Evaluation : insertion 7 criteres"), ok7, erreur);

        if (ok7) {
            QSqlQuery q(db);
            q.prepare(QStringLiteral("SELECT critere_pitch, crit_qualite, crit_impact, crit_business, crit_equipe "
                                     "FROM EVALUATION WHERE id_evaluation = ?"));
            q.addBindValue(evaluation7Id);
            bool ok = q.exec() && q.next();
            QString message;
            if (!ok) {
                message = erreurSql(q.lastError());
            } else {
                ok = q.value(0).isNull()
                     && q.value(1).toDouble() == 18.0 && q.value(2).toDouble() == 17.0
                     && q.value(3).toDouble() == 16.0 && q.value(4).toDouble() == 15.0;
                if (!ok) message = QStringLiteral("colonnes crit_* incorrectes ou pitch non NULL");
            }
            ajouter(QStringLiteral("Evaluation : 7 criteres, pitch NULL"), ok, message);

            const Evaluation relue = Evaluation::obtenirParId(evaluation7Id);
            ajouter(QStringLiteral("Evaluation : relecture 7 criteres"),
                    relue.getId() == evaluation7Id && relue.getCritQualite() == 18.0
                        && relue.getCritImpact() == 17.0 && relue.getCritBusiness() == 16.0
                        && relue.getCritEquipe() == 15.0 && relue.getCriterePitch() == 0.0,
                    QStringLiteral("qualite=%1 impact=%2 business=%3 equipe=%4 pitch=%5")
                        .arg(relue.getCritQualite()).arg(relue.getCritImpact())
                        .arg(relue.getCritBusiness()).arg(relue.getCritEquipe())
                        .arg(relue.getCriterePitch()));
        }

        ajouter(QStringLiteral("Evaluation : detection doublon jury/projet"),
                Evaluation::evaluationExiste(juryA, projetId, -1),
                QStringLiteral("evaluationExiste doit retourner vrai"));

        QSqlQueryModel *modele = Evaluation::afficher();
        ajouter(QStringLiteral("Evaluation : affichage"),
                modele && !modele->lastError().isValid(),
                modele ? (modele->lastError().isValid() ? erreurSql(modele->lastError()) : QString())
                       : QStringLiteral("modele nul"));
        delete modele;

        const QVector<ClassementProjet> classement = Evaluation::calculerClassementAutomatique();
        ajouter(QStringLiteral("Evaluation : classement automatique"), !classement.isEmpty(),
                QStringLiteral("%1 projet(s) classe(s)").arg(classement.size()));

        const QVector<AlerteDivergence> alertes = Evaluation::detecterEcartsEntreJures(4.0);
        ajouter(QStringLiteral("Evaluation : detection ecarts"), true,
                QStringLiteral("%1 alerte(s)").arg(alertes.size()));
    }

    phase(QStringLiteral("tests planning"));
    const QDate dateSession(2030, 1, 15);
    const QTime heureSession(9, 0);
    bool sessionAOk = false, sessionBOk = false;
    if (projetOk && juryAOk) {
        erreur.clear();
        sessionAOk = insererSession(db, sessionAId, dateSession, heureSession, projetId, juryA,
                                    QStringLiteral("TEST_Salle_A"), erreur);
        ajouter(QStringLiteral("Session : insertion TEST_Salle_A"), sessionAOk, erreur);
    }
    if (projetOk && juryBOk) {
        erreur.clear();
        sessionBOk = insererSession(db, sessionBId, dateSession, heureSession, projetId, juryB,
                                    QStringLiteral("TEST_Salle_B"), erreur);
        ajouter(QStringLiteral("Session : insertion TEST_Salle_B"), sessionBOk, erreur);
    }

    if (sessionAOk && sessionBOk) {
        const int duree = qMax(1, ParametresJury::charger().dureeEvaluationMin);
        const QTime heureChevauchement = heureSession.addSecs((duree * 60) / 2);
        const QTime heureSuivante = heureSession.addSecs(duree * 60);

        const SessionEval relue = PlanningEvaluationsPage::lireSession(sessionAId);
        ajouter(QStringLiteral("Session : relecture TEST_Salle_A"),
                relue.id == sessionAId && relue.salle == QStringLiteral("TEST_Salle_A")
                    && relue.idJury == juryA && relue.idProjet == projetId,
                QStringLiteral("salle=%1 jury=%2 projet=%3").arg(relue.salle).arg(relue.idJury).arg(relue.idProjet));

        const ResultatVerificationConflit conflitJury =
            PlanningEvaluationsPage::verifierConflit(dateSession, heureChevauchement, juryA,
                                                     QStringLiteral("TEST_Salle_A"), -1);
        ajouter(QStringLiteral("Session : chevauchement meme jure detecte"),
                conflitJury.etat == ResultatVerificationConflit::ConflitDetecte,
                resultatConflit(conflitJury) + QStringLiteral(" | ") + conflitJury.message);

        const ResultatVerificationConflit conflitSalle =
            PlanningEvaluationsPage::verifierConflit(dateSession, heureChevauchement, juryC,
                                                     QStringLiteral("TEST_Salle_A"), -1);
        ajouter(QStringLiteral("Session : chevauchement meme salle detecte"),
                conflitSalle.etat == ResultatVerificationConflit::ConflitDetecte,
                resultatConflit(conflitSalle) + QStringLiteral(" | ") + conflitSalle.message);

        const ResultatVerificationConflit sansConflit =
            PlanningEvaluationsPage::verifierConflit(dateSession, heureSuivante, juryA,
                                                     QStringLiteral("TEST_Salle_A"), -1);
        ajouter(QStringLiteral("Session : creneau suivant sans conflit"),
                sansConflit.etat == ResultatVerificationConflit::AucunConflit,
                resultatConflit(sansConflit) + QStringLiteral(" | ") + sansConflit.message);

        SessionEval candidate;
        candidate.date = dateSession;
        candidate.heure = heureChevauchement;
        candidate.idProjet = projetId;
        candidate.idJury = juryA;
        candidate.salle = QStringLiteral("TEST_Salle_A");
        candidate.statut = QStringLiteral("Planifiée");
        QString message;
        const bool refusee = !PlanningEvaluationsPage::validerSession(candidate, -1, &message);
        ajouter(QStringLiteral("Session : validation refuse le chevauchement"), refusee, message);

        candidate.heure = heureSuivante;
        message.clear();
        const bool acceptee = PlanningEvaluationsPage::validerSession(candidate, -1, &message);
        ajouter(QStringLiteral("Session : validation accepte le creneau suivant"), acceptee, message);

        candidate.heure = heureChevauchement;
        candidate.idJury = juryC;
        candidate.salle = QStringLiteral("TEST_Salle_C");
        message.clear();
        const bool accepteeAutre = PlanningEvaluationsPage::validerSession(candidate, -1, &message);
        ajouter(QStringLiteral("Session : validation accepte autre jure/salle"), accepteeAutre, message);

        const QVector<SessionEval> sessions = PlanningEvaluationsPage::lireSessions();
        bool trouvee = false;
        for (const SessionEval &s : sessions) {
            if (s.id == sessionAId) {
                trouvee = true;
                break;
            }
        }
        ajouter(QStringLiteral("Session : liste globale"), trouvee,
                QStringLiteral("%1 session(s) lue(s)").arg(sessions.size()));
    }

    phase(QStringLiteral("tests parametres"));
    {
        const ParametresJury p = ParametresJury::charger();
        ajouter(QStringLiteral("Parametres : chargement"),
                p.dureeEvaluationMin > 0 && p.nombreJurys > 0,
                QStringLiteral("duree=%1 jurys=%2 membres=%3 note=%4")
                    .arg(p.dureeEvaluationMin).arg(p.nombreJurys)
                    .arg(p.membresParSession).arg(p.noteTotaleParProjet));
    }

    {
        erreur.clear();
        const bool insere = executer(db,
                                     QStringLiteral("INSERT INTO PARAMETRES_JURY (cle, valeur) VALUES (?, ?)"),
                                     {QStringLiteral("TEST_param"), QStringLiteral("TEST_valeur")},
                                     erreur);
        ajouter(QStringLiteral("Parametres : insertion TEST_param"), insere, erreur);

        if (insere) {
            QSqlQuery q(db);
            q.prepare(QStringLiteral("SELECT valeur FROM PARAMETRES_JURY WHERE cle = ?"));
            q.addBindValue(QStringLiteral("TEST_param"));
            const bool lu = q.exec() && q.next() && q.value(0).toString() == QStringLiteral("TEST_valeur");
            ajouter(QStringLiteral("Parametres : relecture TEST_param"), lu,
                    lu ? QString() : erreurSql(q.lastError()));

            erreur.clear();
            const bool modifie = executer(db,
                                          QStringLiteral("UPDATE PARAMETRES_JURY SET valeur = ? WHERE cle = ?"),
                                          {QStringLiteral("TEST_valeur2"), QStringLiteral("TEST_param")},
                                          erreur);
            ajouter(QStringLiteral("Parametres : modification TEST_param"), modifie, erreur);

            QSqlQuery q2(db);
            q2.prepare(QStringLiteral("SELECT valeur FROM PARAMETRES_JURY WHERE cle = ?"));
            q2.addBindValue(QStringLiteral("TEST_param"));
            const bool relu = q2.exec() && q2.next() && q2.value(0).toString() == QStringLiteral("TEST_valeur2");
            ajouter(QStringLiteral("Parametres : relecture apres modification"), relu,
                    relu ? QString() : erreurSql(q2.lastError()));
        }
    }

    phase(QStringLiteral("rollback final"));
    const bool rollbackOk = db.rollback();
    ajouter(QStringLiteral("Rollback final"), rollbackOk,
            rollbackOk ? QString() : erreurSql(db.lastError()));

    int juryApres = 0, projetApres = 0, evaluationApres = 0, sessionApres = 0, parametresApres = 0;
    erreur.clear();
    const bool comptageOk = compter(db, QStringLiteral("JURY"), juryApres, erreur)
                            && compter(db, QStringLiteral("PROJET"), projetApres, erreur)
                            && compter(db, QStringLiteral("EVALUATION"), evaluationApres, erreur)
                            && compter(db, QStringLiteral("SESSION_EVALUATION"), sessionApres, erreur)
                            && compter(db, QStringLiteral("PARAMETRES_JURY"), parametresApres, erreur);
    ajouter(QStringLiteral("Comptage apres rollback"), comptageOk, erreur);

    if (comptageOk) {
        ajouter(QStringLiteral("JURY inchange"), juryAvant == juryApres,
                QStringLiteral("%1 -> %2").arg(juryAvant).arg(juryApres));
        ajouter(QStringLiteral("PROJET inchange"), projetAvant == projetApres,
                QStringLiteral("%1 -> %2").arg(projetAvant).arg(projetApres));
        ajouter(QStringLiteral("EVALUATION inchangee"), evaluationAvant == evaluationApres,
                QStringLiteral("%1 -> %2").arg(evaluationAvant).arg(evaluationApres));
        ajouter(QStringLiteral("SESSION_EVALUATION inchangee"), sessionAvant == sessionApres,
                QStringLiteral("%1 -> %2").arg(sessionAvant).arg(sessionApres));
        ajouter(QStringLiteral("PARAMETRES_JURY inchange"), parametresAvant == parametresApres,
                QStringLiteral("%1 -> %2").arg(parametresAvant).arg(parametresApres));
    }

    auto compterTest = [&db, &ajouter](const QString &nom, const QString &sql) {
        QSqlQuery q(db);
        if (!q.exec(sql) || !q.next()) {
            ajouter(nom, false, erreurSql(q.lastError()));
            return;
        }
        const int n = q.value(0).toInt();
        ajouter(nom, n == 0, QStringLiteral("%1 ligne(s) TEST_ restante(s)").arg(n));
    };

    compterTest(QStringLiteral("Absence de TEST_ dans JURY"),
                QStringLiteral("SELECT COUNT(*) FROM JURY WHERE nom LIKE 'TEST\\_%' ESCAPE '\\' "
                               "OR email LIKE 'TEST\\_%' ESCAPE '\\'"));
    compterTest(QStringLiteral("Absence de TEST_ dans PROJET"),
                QStringLiteral("SELECT COUNT(*) FROM PROJET WHERE titre LIKE 'TEST\\_%' ESCAPE '\\' "
                               "OR nom_equipe LIKE 'TEST\\_%' ESCAPE '\\'"));
    compterTest(QStringLiteral("Absence de TEST_ dans EVALUATION"),
                QStringLiteral("SELECT COUNT(*) FROM EVALUATION WHERE commentaire LIKE 'TEST\\_%' ESCAPE '\\'"));
    compterTest(QStringLiteral("Absence de TEST_ dans SESSION_EVALUATION"),
                QStringLiteral("SELECT COUNT(*) FROM SESSION_EVALUATION WHERE salle LIKE 'TEST\\_%' ESCAPE '\\'"));
    compterTest(QStringLiteral("Absence de TEST_ dans PARAMETRES_JURY"),
                QStringLiteral("SELECT COUNT(*) FROM PARAMETRES_JURY WHERE cle LIKE 'TEST\\_%' ESCAPE '\\'"));

    out << "\nTEST | RESULTAT | MESSAGE\n";
    bool toutOk = true;
    for (const ResultatTest &r : resultats) {
        out << r.nom << " | " << (r.ok ? QStringLiteral("OK") : QStringLiteral("KO"))
            << " | " << r.message << "\n";
        if (!r.ok)
            toutOk = false;
    }
    out << "\nRESULTAT GLOBAL | " << (toutOk ? QStringLiteral("OK") : QStringLiteral("KO")) << "\n";
    out.flush();

    return toutOk ? 0 : 1;
}
