#include "evaluation.h"
#include "sqlutils.h"
#include <QSqlDatabase>
#include <QSqlError>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QPrinter>
#include <QPageSize>
#include <QPageLayout>
#include <QMarginsF>
#include <QTextDocument>
#include <QDebug>
#include <algorithm>
#include <cmath>

Evaluation::Evaluation()
    : id(-1), idJury(-1), idProjet(-1), note(0.0),
      critereInnovation(0.0), critereTechnique(0.0),
      criterePresentation(0.0), criterePitch(0.0),
      critQualite(0.0), critImpact(0.0), critBusiness(0.0), critEquipe(0.0),
      criteresEtendus(false)
{
    dateEvaluation = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm");
}

Evaluation::Evaluation(int id, int idJury, int idProjet, double note,
                       double innovation, double technique, double presentation, double pitch,
                       const QString &commentaire, const QString &dateEvaluation)
    : id(id), idJury(idJury), idProjet(idProjet), note(note),
      critereInnovation(innovation), critereTechnique(technique),
      criterePresentation(presentation), criterePitch(pitch),
      critQualite(0.0), critImpact(0.0), critBusiness(0.0), critEquipe(0.0),
      criteresEtendus(false),
      commentaire(commentaire), dateEvaluation(dateEvaluation)
{
}

Evaluation::Evaluation(int idJury, int idProjet, double note,
                       double innovation, double technique, double presentation, double pitch,
                       const QString &commentaire)
    : id(-1), idJury(idJury), idProjet(idProjet), note(note),
      critereInnovation(innovation), critereTechnique(technique),
      criterePresentation(presentation), criterePitch(pitch),
      critQualite(0.0), critImpact(0.0), critBusiness(0.0), critEquipe(0.0),
      criteresEtendus(false),
      commentaire(commentaire)
{
    dateEvaluation = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm");
}

bool Evaluation::valider(QString &messageErreur) const
{
    if (idJury <= 0) {
        messageErreur = "Veuillez sélectionner un membre du jury.";
        return false;
    }
    if (idProjet <= 0) {
        messageErreur = "Veuillez sélectionner un projet à évaluer.";
        return false;
    }

    // Règle de gestion : note entre 0 et 20
    if (note < 0.0 || note > 20.0) {
        messageErreur = "La note globale doit être impérativement comprise entre 0.0 et 20.0.";
        return false;
    }

    if (critereInnovation < 0.0 || critereInnovation > 20.0 ||
        critereTechnique < 0.0 || critereTechnique > 20.0 ||
        criterePresentation < 0.0 || criterePresentation > 20.0 ||
        (!criteresEtendus && (criterePitch < 0.0 || criterePitch > 20.0))) {
        messageErreur = "Tous les sous-critères doivent être compris entre 0.0 et 20.0.";
        return false;
    }

    if (criteresEtendus &&
        (critQualite < 0.0 || critQualite > 20.0 ||
         critImpact < 0.0 || critImpact > 20.0 ||
         critBusiness < 0.0 || critBusiness > 20.0 ||
         critEquipe < 0.0 || critEquipe > 20.0)) {
        messageErreur = "Les critères qualité, impact, business et équipe doivent être compris entre 0.0 et 20.0.";
        return false;
    }

    if (commentaire.size() > CommentaireMax) {
        messageErreur = QString("Le commentaire ne peut pas dépasser %1 caractères.").arg(CommentaireMax);
        return false;
    }

    if (!dateEvaluation.trimmed().isEmpty() && dateEvaluation.size() > 16) {
        messageErreur = "La date d'évaluation doit respecter le format yyyy-MM-dd hh:mm.";
        return false;
    }

    // Règle de gestion : un juré n'évalue un projet qu'une seule fois
    if (evaluationExiste(idJury, idProjet, id)) {
        messageErreur = "Ce membre du jury a déjà soumis une évaluation pour ce projet.";
        return false;
    }

    return true;
}

bool Evaluation::evaluationExiste(int idJury, int idProjet, int exclureId)
{
    QSqlQuery q;
    if (exclureId > 0) {
        q.prepare("SELECT COUNT(*) FROM EVALUATION WHERE id_jury = ? AND id_projet = ? AND id_evaluation != ?");
        q.addBindValue(idJury);
        q.addBindValue(idProjet);
        q.addBindValue(exclureId);
    } else {
        q.prepare("SELECT COUNT(*) FROM EVALUATION WHERE id_jury = ? AND id_projet = ?");
        q.addBindValue(idJury);
        q.addBindValue(idProjet);
    }

    if (!q.exec()) {
        signalerErreurSql(QStringLiteral("Impossible de vérifier l'existence de l'évaluation"), q.lastError());
        return false;
    }
    if (q.next()) {
        return q.value(0).toInt() > 0;
    }
    return false;
}

bool Evaluation::enregistrer(const Evaluation &e, bool modification,
                             int *idCree, QString *messageErreur)
{
    if (idCree) *idCree = modification ? e.id : -1;
    if (messageErreur) messageErreur->clear();

    if (modification && e.id <= 0) {
        if (messageErreur) *messageErreur = QStringLiteral("Identifiant d'évaluation invalide pour la modification.");
        return false;
    }

    Evaluation copie = e;
    if (!modification) copie.id = -1;
    copie.commentaire = copie.commentaire.trimmed();

    QSqlDatabase db = QSqlDatabase::database();
    const QString pilote = db.driverName().toUpper();
    const bool oracle = pilote.contains(QLatin1String("QODBC"))
                        || pilote.contains(QLatin1String("QORA"))
                        || pilote.contains(QLatin1String("ORACLE"));

    QVariant pitch;
    QVariant qualite;
    QVariant impact;
    QVariant business;
    QVariant equipe;
    QString date = copie.dateEvaluation.trimmed();

    if (modification) {
        QSqlQuery existante;
        existante.prepare("SELECT critere_pitch, crit_qualite, crit_impact, crit_business, "
                          "crit_equipe, date_evaluation FROM EVALUATION WHERE id_evaluation = ?");
        existante.addBindValue(copie.id);
        if (!existante.exec()) {
            const QString message = QStringLiteral("Impossible de relire l'évaluation à modifier : %1")
                                        .arg(existante.lastError().text());
            qWarning() << "Erreur relecture évaluation:" << existante.lastError().text();
            if (messageErreur) *messageErreur = message;
            return false;
        }
        if (!existante.next()) {
            if (messageErreur) *messageErreur = QStringLiteral("L'évaluation à modifier n'existe plus.");
            return false;
        }

        if (date.isEmpty()) date = existante.value(5).toString();
        pitch = copie.criteresEtendus ? existante.value(0) : QVariant(copie.criterePitch);
        qualite = copie.criteresEtendus ? QVariant(copie.critQualite) : existante.value(1);
        impact = copie.criteresEtendus ? QVariant(copie.critImpact) : existante.value(2);
        business = copie.criteresEtendus ? QVariant(copie.critBusiness) : existante.value(3);
        equipe = copie.criteresEtendus ? QVariant(copie.critEquipe) : existante.value(4);
    } else {
        if (date.isEmpty()) date = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm");
        pitch = copie.criteresEtendus ? QVariant() : QVariant(copie.criterePitch);
        qualite = copie.criteresEtendus ? QVariant(copie.critQualite) : QVariant();
        impact = copie.criteresEtendus ? QVariant(copie.critImpact) : QVariant();
        business = copie.criteresEtendus ? QVariant(copie.critBusiness) : QVariant();
        equipe = copie.criteresEtendus ? QVariant(copie.critEquipe) : QVariant();
    }
    copie.dateEvaluation = date;

    QString validation;
    if (!copie.valider(validation)) {
        if (messageErreur) *messageErreur = validation;
        return false;
    }

    int id = copie.id;
    if (!modification && oracle) {
        QSqlQuery sequence;
        if (!sequence.exec(QStringLiteral("SELECT EVAL_SEQ.NEXTVAL FROM DUAL")) || !sequence.next()) {
            const QString message = QStringLiteral("Impossible de lire EVAL_SEQ.NEXTVAL : %1")
                                        .arg(sequence.lastError().text());
            qWarning() << "Erreur lecture EVAL_SEQ.NEXTVAL:" << sequence.lastError().text();
            if (messageErreur) *messageErreur = message;
            return false;
        }
        id = sequence.value(0).toInt();
        if (id <= 0) {
            if (messageErreur) *messageErreur = QStringLiteral("EVAL_SEQ.NEXTVAL a renvoyé un identifiant invalide.");
            return false;
        }
    }

    QSqlQuery q;
    if (!db.transaction()) {
        const QString message = QStringLiteral("Impossible de démarrer la transaction d'enregistrement de l'évaluation : %1")
                                    .arg(db.lastError().text());
        qWarning() << "Erreur ouverture transaction évaluation:" << db.lastError().text();
        if (messageErreur) *messageErreur = message;
        return false;
    }
    if (!modification) {
        if (oracle) {
            q.prepare("INSERT INTO EVALUATION (id_evaluation, id_jury, id_projet, note, "
                      "critere_innovation, critere_technique, critere_presentation, critere_pitch, "
                      "commentaire, date_evaluation, crit_qualite, crit_impact, crit_business, crit_equipe) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
            q.addBindValue(id);
        } else {
            q.prepare("INSERT INTO EVALUATION (id_jury, id_projet, note, "
                      "critere_innovation, critere_technique, critere_presentation, critere_pitch, "
                      "commentaire, date_evaluation, crit_qualite, crit_impact, crit_business, crit_equipe) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
        }
    } else {
        q.prepare("UPDATE EVALUATION SET id_jury = ?, id_projet = ?, note = ?, "
                  "critere_innovation = ?, critere_technique = ?, critere_presentation = ?, "
                  "critere_pitch = ?, commentaire = ?, date_evaluation = ?, "
                  "crit_qualite = ?, crit_impact = ?, crit_business = ?, crit_equipe = ? "
                  "WHERE id_evaluation = ?");
    }

    if (q.lastError().isValid()) {
        const QString message = QStringLiteral("Impossible de préparer l'enregistrement de l'évaluation : %1")
                                    .arg(q.lastError().text());
        qWarning() << "Erreur préparation enregistrement évaluation:" << q.lastError().text();
        if (!db.rollback()) qWarning() << "Erreur rollback évaluation:" << db.lastError().text();
        if (messageErreur) *messageErreur = message;
        return false;
    }

    q.addBindValue(copie.idJury);
    q.addBindValue(copie.idProjet);
    q.addBindValue(copie.note);
    q.addBindValue(copie.critereInnovation);
    q.addBindValue(copie.critereTechnique);
    q.addBindValue(copie.criterePresentation);
    q.addBindValue(pitch);
    q.addBindValue(copie.commentaire);
    q.addBindValue(copie.dateEvaluation);
    q.addBindValue(qualite);
    q.addBindValue(impact);
    q.addBindValue(business);
    q.addBindValue(equipe);
    if (modification) q.addBindValue(id);

    if (!q.exec()) {
        const QString message = QStringLiteral("Impossible d'enregistrer l'évaluation : %1")
                                    .arg(q.lastError().text());
        qWarning() << "Erreur enregistrement évaluation:" << q.lastError().text();
        if (!db.rollback()) qWarning() << "Erreur rollback évaluation:" << db.lastError().text();
        if (messageErreur) *messageErreur = message;
        return false;
    }

    if (!modification && !oracle) {
        const QVariant dernierId = q.lastInsertId();
        if (!dernierId.isValid() || dernierId.toInt() <= 0) {
            const QString message = QStringLiteral(
                "L'évaluation a été enregistrée, mais son identifiant n'a pas pu être récupéré. "
                "Vérifiez la liste avant de réessayer pour éviter un doublon.");
            qWarning() << "Erreur récupération identifiant évaluation:" << dernierId;
            if (!db.rollback()) qWarning() << "Erreur rollback évaluation:" << db.lastError().text();
            if (messageErreur) *messageErreur = message;
            return false;
        }
        id = dernierId.toInt();
    }

    if (!db.commit()) {
        const QString message = QStringLiteral("Impossible de valider l'enregistrement de l'évaluation : %1")
                                    .arg(db.lastError().text());
        qWarning() << "Erreur validation transaction évaluation:" << db.lastError().text();
        if (!db.rollback()) qWarning() << "Erreur rollback évaluation:" << db.lastError().text();
        if (messageErreur) *messageErreur = message;
        return false;
    }

    if (idCree) *idCree = id;
    return true;
}

bool Evaluation::ajouter(QString *messageErreur)
{
    id = -1;
    int idCree = -1;
    if (!enregistrer(*this, false, &idCree, messageErreur)) return false;
    id = idCree;
    return true;
}

bool Evaluation::modifier(int id, QString *messageErreur)
{
    if (id <= 0) {
        if (messageErreur) *messageErreur = QStringLiteral("Identifiant d'évaluation invalide.");
        return false;
    }

    Evaluation e = *this;
    e.id = id;

    // Conserver impérativement la date d'origine : une modification ne doit pas
    // réhorodater l'évaluation.
    QSqlQuery dateExistante;
    dateExistante.prepare("SELECT date_evaluation FROM EVALUATION WHERE id_evaluation = ?");
    dateExistante.addBindValue(id);
    if (!dateExistante.exec()) {
        const QString message = QStringLiteral("Impossible de relire la date de l'évaluation : %1")
                                    .arg(dateExistante.lastError().text());
        qWarning() << "Erreur relecture date évaluation:" << dateExistante.lastError().text();
        if (messageErreur) *messageErreur = message;
        return false;
    }
    if (!dateExistante.next()) {
        if (messageErreur) *messageErreur = QStringLiteral("L'évaluation à modifier n'existe plus.");
        return false;
    }
    e.dateEvaluation = dateExistante.value(0).toString();

    if (!enregistrer(e, true, nullptr, messageErreur)) return false;
    this->id = id;
    this->dateEvaluation = e.dateEvaluation;
    return true;
}

bool Evaluation::supprimer(int id)
{
    QSqlQuery q;
    q.prepare("DELETE FROM EVALUATION WHERE id_evaluation = ?");
    q.addBindValue(id);
    if (!q.exec()) {
        qWarning() << "Erreur suppression Evaluation:" << q.lastError().text();
        return false;
    }
    return true;
}

Evaluation Evaluation::obtenirParId(int id)
{
    QSqlQuery q;
    q.prepare("SELECT id_evaluation, id_jury, id_projet, note, critere_innovation, "
              "critere_technique, critere_presentation, critere_pitch, commentaire, date_evaluation, "
              "crit_qualite, crit_impact, crit_business, crit_equipe "
              "FROM EVALUATION WHERE id_evaluation = ?");
    q.addBindValue(id);
    if (!q.exec()) {
        signalerErreurSql(QStringLiteral("Impossible de lire l'évaluation #%1").arg(id), q.lastError());
        return Evaluation();
    }
    if (!q.next()) return Evaluation();

    Evaluation e;
    e.id = q.value(0).toInt();
    e.idJury = q.value(1).toInt();
    e.idProjet = q.value(2).toInt();
    e.note = q.value(3).toDouble();
    e.critereInnovation = q.value(4).toDouble();
    e.critereTechnique = q.value(5).toDouble();
    e.criterePresentation = q.value(6).toDouble();
    e.criterePitch = q.value(7).isNull() ? 0.0 : q.value(7).toDouble();
    e.commentaire = q.value(8).toString();
    e.dateEvaluation = q.value(9).toString();
    e.critQualite = q.value(10).isNull() ? 0.0 : q.value(10).toDouble();
    e.critImpact = q.value(11).isNull() ? 0.0 : q.value(11).toDouble();
    e.critBusiness = q.value(12).isNull() ? 0.0 : q.value(12).toDouble();
    e.critEquipe = q.value(13).isNull() ? 0.0 : q.value(13).toDouble();
    e.criteresEtendus = true;
    return e;
}

QSqlQueryModel* Evaluation::afficher()
{
    QSqlQueryModel *model = new QSqlQueryModel();
    QString req = "SELECT e.id_evaluation, "
                  "p.titre AS Projet, "
                  "p.nom_equipe AS Equipe, "
                  "(j.prenom || ' ' || j.nom) AS Jure, "
                  "j.role AS RoleJury, "
                  "e.note AS NoteGlobale, "
                  "e.critere_innovation AS Innovation, "
                  "e.critere_technique AS Technique, "
                  "e.critere_presentation AS Presentation, "
                  "e.commentaire AS Commentaire, "
                  "e.date_evaluation AS DateEvaluation, "
                  "e.critere_pitch AS Pitch, "
                  "e.crit_qualite AS Qualite, "
                  "e.crit_impact AS Impact, "
                  "e.crit_business AS Business, "
                  "e.crit_equipe AS CritEquipe "
                  "FROM EVALUATION e "
                  "JOIN JURY j ON e.id_jury = j.id_jury "
                  "JOIN PROJET p ON e.id_projet = p.id_projet "
                  "ORDER BY e.date_evaluation DESC";

    model->setQuery(req);
    if (model->lastError().isValid()) {
        signalerErreurSql(QStringLiteral("Impossible de charger la liste des évaluations"),
                          model->lastError());
    }
    return model;
}

QVector<ClassementProjet> Evaluation::calculerClassementAutomatique()
{
    QVector<ClassementProjet> classement;

    // Récupérer les moyennes pour chaque projet
    QSqlQuery q;
    q.prepare("SELECT p.id_projet, p.titre, p.nom_equipe, "
              "AVG(e.note) AS moy_note, "
              "AVG(e.critere_innovation) AS moy_inno, "
              "AVG(e.critere_technique) AS moy_tech, "
              "AVG(e.critere_presentation) AS moy_pres, "
              "COUNT(e.id_evaluation) AS nb_evals "
              "FROM PROJET p "
              "LEFT JOIN EVALUATION e ON p.id_projet = e.id_projet "
              "GROUP BY p.id_projet, p.titre, p.nom_equipe "
              "HAVING COUNT(e.id_evaluation) > 0");
    if (!q.exec()) {
        signalerErreurSql(QStringLiteral("Impossible de calculer le classement automatique"), q.lastError());
        return classement;
    }

    while (q.next()) {
        ClassementProjet cp;
        cp.rang = 0;
        cp.idProjet = q.value(0).toInt();
        cp.titre = q.value(1).toString();
        cp.equipe = q.value(2).toString();
        cp.noteMoyenne = std::round(q.value(3).toDouble() * 100.0) / 100.0;
        cp.scoreInnovation = std::round(q.value(4).toDouble() * 100.0) / 100.0;
        cp.scoreTechnique = std::round(q.value(5).toDouble() * 100.0) / 100.0;
        cp.scorePresentation = std::round(q.value(6).toDouble() * 100.0) / 100.0;
        cp.nbEvaluations = q.value(7).toInt();

        // Mention d'excellence
        if (cp.noteMoyenne >= 18.0) cp.mention = "Excellent (Lauréat)";
        else if (cp.noteMoyenne >= 16.0) cp.mention = "Très Bien";
        else if (cp.noteMoyenne >= 14.0) cp.mention = "Bien";
        else if (cp.noteMoyenne >= 12.0) cp.mention = "Assez Bien";
        else cp.mention = "Encourageant";

        classement.append(cp);
    }

    // Tri avec algorithme innovant de départage des ex æquo :
    // 1. Note moyenne générale décroissante
    // 2. Si égalité : score Innovation décroissant
    // 3. Si toujours égalité : score Technique décroissant
    // 4. Si toujours égalité : score Présentation décroissant
    std::sort(classement.begin(), classement.end(), [](const ClassementProjet &a, const ClassementProjet &b) {
        if (std::abs(a.noteMoyenne - b.noteMoyenne) > 0.001) {
            return a.noteMoyenne > b.noteMoyenne;
        }
        if (std::abs(a.scoreInnovation - b.scoreInnovation) > 0.001) {
            return a.scoreInnovation > b.scoreInnovation;
        }
        if (std::abs(a.scoreTechnique - b.scoreTechnique) > 0.001) {
            return a.scoreTechnique > b.scoreTechnique;
        }
        return a.scorePresentation > b.scorePresentation;
    });

    // Attribution des rangs et détection des ex æquo départagés
    for (int i = 0; i < classement.size(); ++i) {
        classement[i].rang = i + 1;
        if (i > 0 && std::abs(classement[i].noteMoyenne - classement[i - 1].noteMoyenne) <= 0.001) {
            classement[i].departageInfo = QString("Départagé à l'ex æquo par Innovation (%1 vs %2)")
                                             .arg(classement[i-1].scoreInnovation)
                                             .arg(classement[i].scoreInnovation);
            classement[i - 1].departageInfo = classement[i].departageInfo;
        }
    }

    return classement;
}

QVector<AlerteDivergence> Evaluation::detecterEcartsEntreJures(double seuilEcart)
{
    QVector<AlerteDivergence> alertes;

    // Récupérer la liste des projets avec leurs notes
    QSqlQuery qProjets;
    if (!qProjets.exec(QStringLiteral("SELECT DISTINCT id_projet, titre, nom_equipe FROM PROJET"))) {
        signalerErreurSql(QStringLiteral("Impossible de lire les projets pour l'analyse des divergences"),
                          qProjets.lastError());
        return alertes;
    }
    while (qProjets.next()) {
        int idProjet = qProjets.value(0).toInt();
        QString titre = qProjets.value(1).toString();
        QString equipe = qProjets.value(2).toString();

        QSqlQuery qNotes;
        qNotes.prepare("SELECT e.note, (j.prenom || ' ' || j.nom) AS jure_nom "
                       "FROM EVALUATION e "
                       "JOIN JURY j ON e.id_jury = j.id_jury "
                       "WHERE e.id_projet = ? "
                       "ORDER BY e.note ASC");
        qNotes.addBindValue(idProjet);

        QVector<double> notes;
        QVector<QString> jures;

        if (!qNotes.exec()) {
            signalerErreurSql(QStringLiteral("Impossible de lire les notes du projet « %1 »").arg(titre),
                              qNotes.lastError());
            return alertes;
        }
        while (qNotes.next()) {
            notes.append(qNotes.value(0).toDouble());
            jures.append(qNotes.value(1).toString());
        }

        // Si le projet a été évalué par au moins 2 jurés
        if (notes.size() >= 2) {
            double noteMin = notes.first();
            QString jureMin = jures.first();
            double noteMax = notes.last();
            QString jureMax = jures.last();
            double ecart = noteMax - noteMin;

            // Calcul de la moyenne et de l'écart-type
            double somme = 0.0;
            for (double n : notes) somme += n;
            double moyenne = somme / notes.size();

            double sommeCarres = 0.0;
            for (double n : notes) {
                sommeCarres += std::pow(n - moyenne, 2);
            }
            double ecartType = std::sqrt(sommeCarres / notes.size());

            // Si l'écart dépasse le seuil, déclenchement d'une alerte
            if (ecart >= seuilEcart || ecartType >= 2.0) {
                AlerteDivergence a;
                a.idProjet = idProjet;
                a.titreProjet = titre;
                a.equipe = equipe;
                a.noteMin = noteMin;
                a.noteMax = noteMax;
                a.ecart = ecart;
                a.ecartType = std::round(ecartType * 100.0) / 100.0;
                a.jureNoteMin = jureMin;
                a.jureNoteMax = jureMax;

                if (ecart >= 6.0) {
                    a.niveauAlerte = "CRITIQUE";
                    a.recommandation = QString("Divergence majeure (%1 pts). Réunion d'harmonisation urgente obligatoire.")
                                           .arg(QString::number(ecart, 'f', 1));
                } else {
                    a.niveauAlerte = "MODÉRÉE";
                    a.recommandation = QString("Écart notable (%1 pts). Consultation des commentaires recommandée.")
                                           .arg(QString::number(ecart, 'f', 1));
                }

                alertes.append(a);
            }
        }
    }

    return alertes;
}

bool Evaluation::exporterClassementPDF(const QString &cheminFichier)
{
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
    QString dateGeneration = QDateTime::currentDateTime().toString("dd/MM/yyyy à hh:mm");
    QVector<ClassementProjet> classement = calculerClassementAutomatique();

    QString html =
        "<!DOCTYPE html>"
        "<html>"
        "<head>"
        "<meta charset='utf-8'>"
        "<style>"
        "body { font-family: 'Segoe UI', Arial, sans-serif; color: #1E293B; margin: 0; padding: 20px; }"
        ".header { border-bottom: 3px solid #0BF8F6; padding-bottom: 15px; margin-bottom: 25px; }"
        ".title { font-size: 24px; font-weight: bold; color: #101B31; margin: 0; }"
        ".subtitle { font-size: 13px; color: #2F67E0; margin-top: 5px; font-weight: bold; }"
        ".meta { font-size: 11px; color: #64748B; margin-top: 10px; }"
        "table { width: 100%; border-collapse: collapse; margin-top: 15px; font-size: 11px; }"
        "th { background: #1C2740; color: #FFFFFF; padding: 10px 8px; text-align: left; font-weight: bold; }"
        "td { padding: 9px 8px; border-bottom: 1px solid #E2E8F0; }"
        ".podium-1 { background-color: #FEF9C3; font-weight: bold; color: #854D0E; }"
        ".podium-2 { background-color: #F1F5F9; font-weight: bold; color: #475569; }"
        ".podium-3 { background-color: #FFEDD5; font-weight: bold; color: #9A3412; }"
        ".badge-gold { background: #EAB308; color: #FFFFFF; padding: 3px 7px; border-radius: 10px; font-weight: bold; }"
        ".badge-silver { background: #94A3B8; color: #FFFFFF; padding: 3px 7px; border-radius: 10px; font-weight: bold; }"
        ".badge-bronze { background: #F97316; color: #FFFFFF; padding: 3px 7px; border-radius: 10px; font-weight: bold; }"
        ".signature-box { float: right; width: 230px; border-top: 1px dashed #64748B; text-align: center; margin-top: 60px; padding-top: 6px; font-size: 11px; font-weight: bold; }"
        ".footer { margin-top: 50px; padding-top: 15px; border-top: 1px solid #E2E8F0; font-size: 10px; color: #94A3B8; text-align: center; }"
        "</style>"
        "</head>"
        "<body>"
        "<div class='header'>"
        "<div class='title'>PALMARÈS ET CLASSEMENT OFFICIEL DU JURY</div>"
        "<div class='subtitle'>SMART HACKATHON • ALGORITHME INNOVANT DE DÉPARTAGE DES EX ÆQUO</div>"
        "<div class='meta'>Délibération officielle arrêtée le " + dateGeneration + " • ChronoHack / NovaCrew</div>"
        "</div>"
        "<table>"
        "<thead>"
        "<tr>"
        "<th style='width:50px;'>Rang</th>"
        "<th>Projet</th>"
        "<th>Équipe</th>"
        "<th style='text-align:center;'>Note Globale</th>"
        "<th style='text-align:center;'>Innovation</th>"
        "<th style='text-align:center;'>Technique</th>"
        "<th style='text-align:center;'>Présentation</th>"
        "<th>Mention / Départage</th>"
        "</tr>"
        "</thead>"
        "<tbody>";

    for (const ClassementProjet &cp : classement) {
        QString rowClass = "";
        QString rangBadge = QString::number(cp.rang);
        if (cp.rang == 1) {
            rowClass = "podium-1";
            rangBadge = "<span class='badge-gold'>1er 🥇</span>";
        } else if (cp.rang == 2) {
            rowClass = "podium-2";
            rangBadge = "<span class='badge-silver'>2ème 🥈</span>";
        } else if (cp.rang == 3) {
            rowClass = "podium-3";
            rangBadge = "<span class='badge-bronze'>3ème 🥉</span>";
        }

        QString infoExtra = cp.mention;
        if (!cp.departageInfo.isEmpty()) {
            infoExtra += "<br><small style='color:#2563EB;'>(" + cp.departageInfo + ")</small>";
        }

        html += "<tr class='" + rowClass + "'>"
                "<td style='text-align:center;'>" + rangBadge + "</td>"
                "<td><strong>" + cp.titre.toHtmlEscaped() + "</strong></td>"
                "<td>" + cp.equipe.toHtmlEscaped() + "</td>"
                "<td style='text-align:center; font-size:13px;'><strong>" + QString::number(cp.noteMoyenne, 'f', 2) + " / 20</strong></td>"
                "<td style='text-align:center;'>" + QString::number(cp.scoreInnovation, 'f', 1) + "</td>"
                "<td style='text-align:center;'>" + QString::number(cp.scoreTechnique, 'f', 1) + "</td>"
                "<td style='text-align:center;'>" + QString::number(cp.scorePresentation, 'f', 1) + "</td>"
                "<td>" + infoExtra + "</td>"
                "</tr>";
    }

    html +=
        "</tbody>"
        "</table>"
        "<div style='clear:both;'></div>"
        "<div class='signature-box'>Président du Jury & Organisateurs</div>"
        "<div class='footer'>"
        "Certifié conforme par le système HackFlow • Faculté et Comité de Hackathon 2025-2026"
        "</div>"
        "</body>"
        "</html>";

    if (cheminFichier.trimmed().isEmpty()) {
        qWarning() << "Export PDF du classement : chemin vide.";
        return false;
    }

    doc.setHtml(html);
    doc.print(&printer);
    if (!QFile::exists(cheminFichier) || QFileInfo(cheminFichier).size() <= 0) {
        qWarning() << "Export PDF du classement : le fichier n'a pas été créé ou est vide :" << cheminFichier;
        return false;
    }
    return true;
}
