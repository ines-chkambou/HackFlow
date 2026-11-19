#include "planningevaluations.h"
#include "parametresjury.h"
#include "sqlutils.h"
#include "uihelpers.h"

#include <QAction>
#include <QActionGroup>
#include <QCalendarWidget>
#include <QCheckBox>
#include <QComboBox>
#include <QDateEdit>
#include <QDateTime>
#include <QDebug>
#include <QDialog>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPageLayout>
#include <QPageSize>
#include <QPrinter>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTableWidget>
#include <QTextDocument>
#include <QTextStream>
#include <QTimeEdit>
#include <QVBoxLayout>
#include <algorithm>

using UiHelpers::EnteteTri;

namespace {

const int kColCoche = 0, kColDate = 1, kColHeure = 2, kColProjet = 3, kColEquipe = 4,
          kColJury = 5, kColSalle = 6, kColStatut = 7, kColActions = 8;

const char *kFormatDate = "dd/MM/yyyy";

const QString kCarteCss =
    "QFrame#cartePlanning { background-color: #011846; border: 1px solid #14449A; border-radius: 8px; }";

const QString kComboCss =
    "QComboBox { background-color: #01286C; border: 1px solid #1F57A4; border-radius: 6px; padding: 0px 12px; "
    "color: #FFFFFF; font-size: 12px; min-height: 34px; } "
    "QComboBox:focus { border: 1px solid #3B82F6; } "
    "QComboBox::drop-down { border: none; width: 30px; } "
    "QComboBox::down-arrow { image: url(:/img/chevron_down_white.png); width: 12px; height: 12px; } "
    "QComboBox QAbstractItemView { background-color: #021B4A; color: #E6EEFF; border: 1px solid #1F57A4; "
    "selection-background-color: #0B6BFF; outline: none; }";

const QString kDateCss =
    "QDateEdit { background-color: #01286C; border: 1px solid #1F57A4; border-radius: 6px; padding-left: 4px; "
    "color: #FFFFFF; font-size: 12px; min-height: 34px; } "
    "QDateEdit:focus { border: 1px solid #3B82F6; } "
    "QDateEdit::drop-down { subcontrol-origin: padding; subcontrol-position: center left; width: 30px; border: none; } "
    "QDateEdit::down-arrow { image: url(:/img/calendar_small.png); width: 16px; height: 16px; }";

const QString kCalendrierPopupCss =
    "QCalendarWidget QWidget { background-color: #021B4A; color: #E6EEFF; } "
    "QCalendarWidget QToolButton { color: #E6EEFF; background: transparent; border: none; padding: 4px 8px; } "
    "QCalendarWidget QToolButton:hover { background-color: #173266; } "
    "QCalendarWidget QMenu { background-color: #021B4A; color: #E6EEFF; } "
    "QCalendarWidget QSpinBox { background-color: #01286C; color: #E6EEFF; } "
    "QCalendarWidget QAbstractItemView:enabled { background-color: #021B4A; color: #E6EEFF; "
    "selection-background-color: #0B6BFF; selection-color: #FFFFFF; } "
    "QCalendarWidget QAbstractItemView:disabled { color: #5B6E98; }";

const QString kDialogCss =
    "QDialog { background-color: #01143C; } "
    "QLabel { color: #E6EEFF; font-size: 13px; background: transparent; } "
    "QLineEdit, QComboBox, QDateEdit, QTimeEdit { background-color: #01286C; border: 1px solid #1F57A4; border-radius: 8px; "
    "padding: 0px 12px; min-height: 38px; color: #EAF2FF; font-size: 13px; } "
    "QLineEdit:focus, QComboBox:focus, QDateEdit:focus, QTimeEdit:focus { border: 1px solid #3B82F6; } "
    "QComboBox::drop-down { border: none; width: 28px; } "
    "QComboBox::down-arrow { image: url(:/img/chevron.png); width: 12px; height: 7px; } "
    "QDateEdit::drop-down { border: none; width: 28px; } "
    "QDateEdit::down-arrow { image: url(:/img/chevron.png); width: 12px; height: 7px; } "
    "QComboBox QAbstractItemView { background-color: #021B4A; color: #E6EEFF; border: 1px solid #1F57A4; selection-background-color: #0B6BFF; outline: none; } "
    "QPushButton { background-color: #11254F; color: #EAF2FF; border: 1px solid #1D4A8B; border-radius: 8px; padding: 0px 18px; "
    "min-height: 40px; font-size: 13px; font-weight: 600; } "
    "QPushButton:hover { background-color: #173266; border-color: #2F6FD8; } "
    "QPushButton[principal=\"true\"] { background-color: #0391FC; color: #FFFFFF; border: none; font-weight: bold; } "
    "QPushButton[principal=\"true\"]:hover { background-color: #1AA0FF; }";

const QStringList kStatuts = {"Planifiée", "En cours", "Terminée"};
const QStringList kMois = {"Janvier", "Février", "Mars", "Avril", "Mai", "Juin", "Juillet", "Août",
                           "Septembre", "Octobre", "Novembre", "Décembre"};

QString couleurStatut(const QString &s)
{
    if (s == "En cours") return "#F2A413";
    if (s == "Terminée") return "#7A5CF0";
    return "#0DA582";   // Planifiée
}

QColor couleurPoint(const QString &s)
{
    if (s == "En cours") return QColor("#F5A30A");
    if (s == "Terminée") return QColor("#8B6CFF");
    return QColor("#19C39A");
}

QString pluriel(int n, const QString &mot)
{
    return QString("%1 %2%3").arg(n).arg(mot).arg(n > 1 ? "s" : "");
}

QString csvChamp(const QString &s)
{
    QString r = s;
    r.replace('"', "\"\"");
    return '"' + r + '"';
}

} // namespace

// ====================================================================
// MINI-CALENDRIER
// ====================================================================

MiniCalendrier::MiniCalendrier(QWidget *parent) : QWidget(parent)
{
    m_mois = QDate(QDate::currentDate().year(), QDate::currentDate().month(), 1);
    setMouseTracking(true);
    setFixedHeight(206);
    setCursor(Qt::PointingHandCursor);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void MiniCalendrier::setMois(const QDate &d)
{
    if (!d.isValid()) return;
    m_mois = QDate(d.year(), d.month(), 1);
    update();
}

void MiniCalendrier::setPlage(const QDate &debut, const QDate &fin)
{
    m_debut = debut;
    m_fin = fin;
    update();
}

void MiniCalendrier::setMarques(const QMap<QDate, QStringList> &statutsParJour)
{
    m_marques = statutsParJour;
    update();
}

QDate MiniCalendrier::dateCellule(int ligne, int colonne) const
{
    const int decalage = m_mois.dayOfWeek() - 1;   // lundi = 0
    return m_mois.addDays(-decalage + ligne * 7 + colonne);
}

QRectF MiniCalendrier::rectCellule(int ligne, int colonne) const
{
    const qreal w = width() / 7.0;
    return QRectF(colonne * w, 26 + ligne * 30, w, 30);
}

bool MiniCalendrier::cellulePosition(const QPoint &p, int &ligne, int &colonne) const
{
    if (p.y() < 26) return false;
    ligne = (p.y() - 26) / 30;
    colonne = int(p.x() / (width() / 7.0));
    return ligne >= 0 && ligne < 6 && colonne >= 0 && colonne < 7;
}

void MiniCalendrier::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    static const QStringList jours = {"Lun", "Mar", "Mer", "Jeu", "Ven", "Sam", "Dim"};
    QFont f = font();
    f.setPixelSize(12);
    p.setFont(f);

    p.setPen(QColor("#E6EEFF"));
    for (int c = 0; c < 7; ++c) {
        QFont fb = f;
        fb.setBold(true);
        p.setFont(fb);
        p.drawText(QRectF(c * width() / 7.0, 0, width() / 7.0, 26), Qt::AlignCenter, jours.at(c));
    }
    p.setFont(f);

    const QDate aujourdhui = QDate::currentDate();

    for (int l = 0; l < 6; ++l) {
        // Bandeau de la plage sélectionnée : un segment arrondi par ligne
        int premier = -1, dernier = -1;
        for (int c = 0; c < 7; ++c) {
            const QDate d = dateCellule(l, c);
            if (m_debut.isValid() && m_fin.isValid() && d >= m_debut && d <= m_fin) {
                if (premier < 0) premier = c;
                dernier = c;
            }
        }
        if (premier >= 0) {
            const QRectF r = rectCellule(l, premier).united(rectCellule(l, dernier)).adjusted(2, 2, -2, -2);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor("#0B6BFF"));
            p.drawRoundedRect(r, 8, 8);
        }

        for (int c = 0; c < 7; ++c) {
            const QDate d = dateCellule(l, c);
            const QRectF r = rectCellule(l, c);
            const bool dansPlage = m_debut.isValid() && m_fin.isValid() && d >= m_debut && d <= m_fin;
            const bool dansMois = d.month() == m_mois.month();

            if (!dansPlage && d == m_survol) {
                p.setPen(Qt::NoPen);
                p.setBrush(QColor(255, 255, 255, 28));
                p.drawRoundedRect(r.adjusted(3, 3, -3, -3), 7, 7);
            }
            if (d == aujourdhui && !dansPlage) {
                p.setPen(QPen(QColor("#19C3F0"), 1.2));
                p.setBrush(Qt::NoBrush);
                p.drawRoundedRect(r.adjusted(3, 3, -3, -3), 7, 7);
            }

            QFont fn = f;
            fn.setBold(dansPlage);
            p.setFont(fn);
            p.setPen(dansPlage ? QColor("#FFFFFF") : (dansMois ? QColor("#E6EEFF") : QColor("#2FA8FF")));
            p.drawText(r.adjusted(0, -3, 0, -3), Qt::AlignCenter, QString::number(d.day()));

            // Points : un par statut présent ce jour-là
            const QStringList statuts = m_marques.value(d);
            if (!statuts.isEmpty()) {
                QStringList distincts;
                for (const QString &s : kStatuts) if (statuts.contains(s)) distincts << s;
                const qreal pas = 7, rayon = 2.2;
                qreal x = r.center().x() - (distincts.size() - 1) * pas / 2.0;
                for (const QString &s : distincts) {
                    p.setPen(Qt::NoPen);
                    p.setBrush(couleurPoint(s));
                    p.drawEllipse(QPointF(x, r.bottom() - 6), rayon, rayon);
                    x += pas;
                }
            }
        }
    }
}

void MiniCalendrier::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton) return;
    int l, c;
    if (!cellulePosition(e->pos(), l, c)) return;
    emit jourClique(dateCellule(l, c));
}

void MiniCalendrier::mouseMoveEvent(QMouseEvent *e)
{
    int l, c;
    const QDate d = cellulePosition(e->pos(), l, c) ? dateCellule(l, c) : QDate();
    if (d != m_survol) {
        m_survol = d;
        update();
    }
}

void MiniCalendrier::leaveEvent(QEvent *)
{
    m_survol = QDate();
    update();
}

// ====================================================================
// ACCÈS AUX DONNÉES
// ====================================================================

static SessionEval sessionDepuisRequete(const QSqlQuery &q)
{
    SessionEval s;
    s.id = q.value(0).toInt();
    s.date = QDate::fromString(q.value(1).toString(), "yyyy-MM-dd");
    s.heure = QTime::fromString(q.value(2).toString(), "HH:mm");
    s.idProjet = q.value(3).toInt();
    s.titreProjet = q.value(4).toString();
    s.descriptionProjet = q.value(5).toString();
    s.equipe = q.value(6).toString();
    s.idJury = q.value(7).toInt();
    s.nomJury = q.value(8).toString();
    s.prenomJury = q.value(9).toString();
    s.salle = q.value(10).toString();
    s.statut = q.value(11).toString();
    return s;
}

static const char *kSelectSession =
    "SELECT s.id_session, s.date_session, s.heure, s.id_projet, p.titre, COALESCE(p.description, ''), p.nom_equipe, "
    "s.id_jury, j.nom, j.prenom, s.salle, s.statut "
    "FROM SESSION_EVALUATION s "
    "JOIN PROJET p ON p.id_projet = s.id_projet "
    "JOIN JURY j ON j.id_jury = s.id_jury ";

QVector<SessionEval> PlanningEvaluationsPage::lireSessions()
{
    QVector<SessionEval> res;
    QSqlQuery q;
    if (!q.exec(QString(kSelectSession) + "ORDER BY s.date_session, s.heure, s.id_session")) {
        signalerErreurSql(QStringLiteral("Impossible de charger le planning des évaluations"), q.lastError());
        return res;
    }
    while (q.next()) res.append(sessionDepuisRequete(q));
    return res;
}

SessionEval PlanningEvaluationsPage::lireSession(int id)
{
    QSqlQuery q;
    q.prepare(QString(kSelectSession) + "WHERE s.id_session = ?");
    q.addBindValue(id);
    if (!q.exec()) {
        signalerErreurSql(QStringLiteral("Impossible de lire la session #%1").arg(id), q.lastError());
        return SessionEval();
    }
    if (q.next()) return sessionDepuisRequete(q);
    return SessionEval();
}

ResultatVerificationConflit PlanningEvaluationsPage::verifierConflit(const QDate &date, const QTime &heure,
                                                                     int idJury, const QString &salle,
                                                                     int exclureId)
{
    ResultatVerificationConflit resultat;

    if (!date.isValid() || !heure.isValid()) {
        resultat.etat = ResultatVerificationConflit::ConflitDetecte;
        resultat.message = QStringLiteral("La date ou l'heure de la session est invalide.");
        return resultat;
    }

    const ParametresJury parametres = ParametresJury::charger();
    const int dureeMin = qMax(1, parametres.dureeEvaluationMin);
    const QDateTime debutCandidat(date, heure);
    const QDateTime finCandidat = debutCandidat.addSecs(dureeMin * 60);
    const QString salleNettoyee = salle.trimmed();

    // On lit toutes les sessions du même jour : deux créneaux peuvent se chevaucher
    // même si leur heure de début diffère (durée réelle ParametresJury::dureeEvaluationMin).
    QSqlQuery q;
    q.prepare("SELECT id_jury, salle, heure FROM SESSION_EVALUATION "
              "WHERE date_session = ? AND id_session <> ?");
    q.addBindValue(date.toString("yyyy-MM-dd"));
    q.addBindValue(exclureId);

    if (!q.exec()) {
        const QString message = QStringLiteral("Impossible de vérifier les conflits de planning : %1")
                                    .arg(q.lastError().text());
        qWarning() << "Erreur vérification conflit planning:" << q.lastError().text();
        resultat.etat = ResultatVerificationConflit::ErreurSql;
        resultat.message = message;
        return resultat;
    }

    while (q.next()) {
        const QTime heureExistante = QTime::fromString(q.value(2).toString(), "HH:mm");
        if (!heureExistante.isValid()) continue;

        const QDateTime debutExistant(date, heureExistante);
        const QDateTime finExistante = debutExistant.addSecs(dureeMin * 60);
        const bool chevauche = debutCandidat < finExistante && debutExistant < finCandidat;
        if (!chevauche) continue;

        if (q.value(0).toInt() == idJury) {
            resultat.etat = ResultatVerificationConflit::ConflitDetecte;
            resultat.message = QString("Ce juré a déjà une session le %1 de %2 à %3 (durée : %4 min).")
                                   .arg(date.toString(kFormatDate),
                                        heureExistante.toString("HH:mm"),
                                        finExistante.time().toString("HH:mm"))
                                   .arg(dureeMin);
            return resultat;
        }
        if (!salleNettoyee.isEmpty() &&
            q.value(1).toString().trimmed().compare(salleNettoyee, Qt::CaseInsensitive) == 0) {
            resultat.etat = ResultatVerificationConflit::ConflitDetecte;
            resultat.message = QString("La salle « %1 » est déjà occupée le %2 de %3 à %4 (durée : %5 min).")
                                   .arg(salleNettoyee,
                                        date.toString(kFormatDate),
                                        heureExistante.toString("HH:mm"),
                                        finExistante.time().toString("HH:mm"))
                                   .arg(dureeMin);
            return resultat;
        }
    }
    if (q.lastError().isValid()) {
        resultat.etat = ResultatVerificationConflit::ErreurSql;
        resultat.message = QStringLiteral("Impossible de lire les conflits de planning : %1")
                               .arg(q.lastError().text());
        qWarning() << "Erreur lecture conflits planning:" << q.lastError().text();
    }
    return resultat;
}

bool PlanningEvaluationsPage::validerSession(const SessionEval &s, int exclureId, QString *messageErreur)
{
    if (messageErreur) messageErreur->clear();

    if (!s.date.isValid()) {
        if (messageErreur) *messageErreur = QStringLiteral("La date de la session est invalide.");
        return false;
    }
    if (!s.heure.isValid()) {
        if (messageErreur) *messageErreur = QStringLiteral("L'heure de la session est invalide.");
        return false;
    }
    if (s.idProjet <= 0) {
        if (messageErreur) *messageErreur = QStringLiteral("Veuillez sélectionner un projet.");
        return false;
    }
    if (s.idJury <= 0) {
        if (messageErreur) *messageErreur = QStringLiteral("Veuillez sélectionner un juré.");
        return false;
    }

    const QString salle = s.salle.trimmed();
    if (salle.isEmpty()) {
        if (messageErreur) *messageErreur = QStringLiteral("Veuillez indiquer la salle.");
        return false;
    }
    if (salle.size() > 100) {
        if (messageErreur) *messageErreur = QStringLiteral("La salle ne peut pas dépasser 100 caractères.");
        return false;
    }
    if (!kStatuts.contains(s.statut)) {
        if (messageErreur) *messageErreur = QStringLiteral("Le statut de la session est invalide.");
        return false;
    }

    const ResultatVerificationConflit verification =
        verifierConflit(s.date, s.heure, s.idJury, salle, exclureId);
    if (verification.etat == ResultatVerificationConflit::ErreurSql) {
        if (messageErreur) *messageErreur = verification.message;
        return false;
    }
    if (verification.etat == ResultatVerificationConflit::ConflitDetecte) {
        if (messageErreur) *messageErreur = verification.message;
        return false;
    }
    return true;
}

bool PlanningEvaluationsPage::ajouterSession(const SessionEval &s, int *nouvelId, QString *messageErreur)
{
    if (nouvelId) *nouvelId = -1;
    if (messageErreur) messageErreur->clear();

    QString validation;
    if (!validerSession(s, -1, &validation)) {
        if (messageErreur) *messageErreur = validation;
        return false;
    }

    const bool oracle = QSqlDatabase::database().driverName() != QLatin1String("QSQLITE");
    QVariant idOracle;
    if (oracle) {
        // NEXTVAL explicite avant l'INSERT : l'identifiant est connu sans dépendre
        // de CURRVAL ni du trigger SESSION_BIR sur la même connexion.
        QSqlQuery seq;
        if (!seq.exec(QStringLiteral("SELECT SESSION_SEQ.NEXTVAL FROM DUAL")) || !seq.next()) {
            const QString message = QStringLiteral("Impossible d'obtenir un identifiant de session Oracle : %1")
                                        .arg(seq.lastError().text());
            signalerErreurSql(QStringLiteral("Impossible d'obtenir SESSION_SEQ.NEXTVAL"), seq.lastError());
            if (messageErreur) *messageErreur = message;
            return false;
        }
        idOracle = seq.value(0);
        if (idOracle.toInt() <= 0) {
            const QString message = QStringLiteral("SESSION_SEQ.NEXTVAL a renvoyé un identifiant invalide.");
            qWarning() << message << idOracle;
            if (messageErreur) *messageErreur = message;
            return false;
        }
    }

    QSqlDatabase db = QSqlDatabase::database();
    if (!db.transaction()) {
        const QString message = QStringLiteral("Impossible de démarrer la transaction : %1")
                                    .arg(db.lastError().text());
        signalerErreurSql(QStringLiteral("Impossible de démarrer la transaction d'ajout de session"), db.lastError());
        if (messageErreur) *messageErreur = message;
        return false;
    }

    QSqlQuery q;
    if (oracle) {
        q.prepare("INSERT INTO SESSION_EVALUATION (id_session, date_session, heure, id_projet, id_jury, salle, statut) "
                  "VALUES (?, ?, ?, ?, ?, ?, ?)");
        q.addBindValue(idOracle);
    } else {
        q.prepare("INSERT INTO SESSION_EVALUATION (date_session, heure, id_projet, id_jury, salle, statut) "
                  "VALUES (?, ?, ?, ?, ?, ?)");
    }
    q.addBindValue(s.date.toString("yyyy-MM-dd"));
    q.addBindValue(s.heure.toString("HH:mm"));
    q.addBindValue(s.idProjet);
    q.addBindValue(s.idJury);
    q.addBindValue(s.salle.trimmed());
    q.addBindValue(s.statut);
    if (!q.exec()) {
        const QString message = QStringLiteral("Impossible d'ajouter la session : %1").arg(q.lastError().text());
        qWarning() << "Erreur ajout session:" << q.lastError().text();
        if (!db.rollback())
            qWarning() << "Erreur rollback ajout session:" << db.lastError().text();
        if (messageErreur) *messageErreur = message;
        return false;
    }

    if (oracle) {
        if (nouvelId) *nouvelId = idOracle.toInt();
    } else if (nouvelId) {
        const QVariant id = q.lastInsertId();
        if (!id.isValid() || id.toInt() <= 0) {
            const QString message = QStringLiteral(
                "La session a été ajoutée, mais son identifiant n'a pas pu être récupéré. "
                "Vérifiez le planning avant de réessayer pour éviter un doublon.");
            qWarning() << "Erreur récupération identifiant session:" << id;
            if (!db.rollback())
                qWarning() << "Erreur rollback ajout session:" << db.lastError().text();
            if (messageErreur) *messageErreur = message;
            return false;
        }
        *nouvelId = id.toInt();
    }

    if (!db.commit()) {
        const QString message = QStringLiteral("Impossible de valider l'ajout de la session : %1")
                                    .arg(db.lastError().text());
        qWarning() << "Erreur validation ajout session:" << db.lastError().text();
        if (!db.rollback())
            qWarning() << "Erreur rollback ajout session:" << db.lastError().text();
        if (messageErreur) *messageErreur = message;
        return false;
    }
    return true;
}

bool PlanningEvaluationsPage::modifierSession(const SessionEval &s, QString *messageErreur)
{
    if (messageErreur) messageErreur->clear();

    if (s.id <= 0) {
        if (messageErreur) *messageErreur = QStringLiteral("Identifiant de session invalide.");
        return false;
    }
    QString validation;
    if (!validerSession(s, s.id, &validation)) {
        if (messageErreur) *messageErreur = validation;
        return false;
    }

    QSqlDatabase db = QSqlDatabase::database();
    if (!db.transaction()) {
        const QString message = QStringLiteral("Impossible de démarrer la transaction : %1")
                                    .arg(db.lastError().text());
        signalerErreurSql(QStringLiteral("Impossible de démarrer la transaction de modification de session"), db.lastError());
        if (messageErreur) *messageErreur = message;
        return false;
    }

    QSqlQuery q;
    q.prepare("UPDATE SESSION_EVALUATION SET date_session = ?, heure = ?, id_projet = ?, id_jury = ?, salle = ?, statut = ? "
              "WHERE id_session = ?");
    q.addBindValue(s.date.toString("yyyy-MM-dd"));
    q.addBindValue(s.heure.toString("HH:mm"));
    q.addBindValue(s.idProjet);
    q.addBindValue(s.idJury);
    q.addBindValue(s.salle.trimmed());
    q.addBindValue(s.statut);
    q.addBindValue(s.id);
    if (!q.exec()) {
        const QString message = QStringLiteral("Impossible de modifier la session : %1").arg(q.lastError().text());
        qWarning() << "Erreur modification session:" << q.lastError().text();
        if (!db.rollback())
            qWarning() << "Erreur rollback modification session:" << db.lastError().text();
        if (messageErreur) *messageErreur = message;
        return false;
    }

    if (!db.commit()) {
        const QString message = QStringLiteral("Impossible de valider la modification de la session : %1")
                                    .arg(db.lastError().text());
        qWarning() << "Erreur validation modification session:" << db.lastError().text();
        if (!db.rollback())
            qWarning() << "Erreur rollback modification session:" << db.lastError().text();
        if (messageErreur) *messageErreur = message;
        return false;
    }
    return true;
}

bool PlanningEvaluationsPage::supprimerSession(int id)
{
    QSqlQuery q;
    q.prepare("DELETE FROM SESSION_EVALUATION WHERE id_session = ?");
    q.addBindValue(id);
    if (!q.exec()) {
        qWarning() << "Erreur suppression session:" << q.lastError().text();
        return false;
    }
    return true;
}

bool PlanningEvaluationsPage::exporterCSV(const QString &chemin, const QVector<SessionEval> &sessions)
{
    if (chemin.trimmed().isEmpty()) {
        qWarning() << "Export CSV du planning : chemin vide.";
        return false;
    }
    QFile f(chemin);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Export CSV du planning : ouverture impossible :" << f.errorString();
        return false;
    }
    QTextStream out(&f);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    out.setCodec("UTF-8");
#endif
    out << QChar(0xFEFF);   // BOM : ouverture correcte sous Excel
    out << "ID;Date;Heure;Projet;Équipe;Juré;Salle;Statut\n";
    for (const SessionEval &s : sessions) {
        out << s.id << ';' << s.date.toString(kFormatDate) << ';' << s.heure.toString("HH:mm") << ';'
            << csvChamp(s.titreProjet) << ';' << csvChamp(s.equipe) << ';' << csvChamp(s.nomCompletJury()) << ';'
            << csvChamp(s.salle) << ';' << csvChamp(s.statut) << '\n';
    }
    out.flush();
    const bool ok = out.status() == QTextStream::Ok && f.error() == QFile::NoError;
    if (!ok) qWarning() << "Export CSV du planning : écriture incomplète :" << f.errorString();
    f.close();
    return ok;
}

bool PlanningEvaluationsPage::exporterPDF(const QString &chemin, const QVector<SessionEval> &sessions, const QString &periode)
{
    if (chemin.trimmed().isEmpty()) {
        qWarning() << "Export PDF du planning : chemin vide.";
        return false;
    }
    QString html =
        "<html><head><style>"
        "body { font-family: 'Segoe UI', Arial, sans-serif; color: #0F172A; }"
        "h1 { color: #0369A1; font-size: 20pt; margin-bottom: 2px; }"
        "p.sub { color: #475569; font-size: 10pt; margin-top: 0; }"
        "table { border-collapse: collapse; width: 100%; }"
        "th { background-color: #0F2A63; color: #FFFFFF; font-size: 9pt; padding: 6px; text-align: left; }"
        "td { border-bottom: 1px solid #CBD5E1; font-size: 9pt; padding: 5px; }"
        "</style></head><body>"
        "<h1>HackFlow — Planning des évaluations</h1>"
        "<p class='sub'>Période : " + periode.toHtmlEscaped() + " — " + pluriel(sessions.size(), "session") +
        " — généré le " + QDateTime::currentDateTime().toString("dd/MM/yyyy HH:mm") + "</p>"
        "<table><tr><th>Date</th><th>Heure</th><th>Projet</th><th>Équipe</th><th>Juré</th><th>Salle</th><th>Statut</th></tr>";
    for (const SessionEval &s : sessions) {
        html += "<tr><td>" + s.date.toString(kFormatDate) + "</td><td>" + s.heure.toString("HH:mm") + "</td>"
                "<td><b>" + s.titreProjet.toHtmlEscaped() + "</b></td>"
                "<td>" + s.equipe.toHtmlEscaped() + "</td>"
                "<td>" + s.nomCompletJury().toHtmlEscaped() + "</td>"
                "<td>" + s.salle.toHtmlEscaped() + "</td>"
                "<td>" + s.statut.toHtmlEscaped() + "</td></tr>";
    }
    html += "</table></body></html>";

    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(chemin);
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
    if (!QFile::exists(chemin) || QFileInfo(chemin).size() <= 0) {
        qWarning() << "Export PDF du planning : le fichier n'a pas été créé ou est vide :" << chemin;
        return false;
    }
    return true;
}

// ====================================================================
// PAGE
// ====================================================================

PlanningEvaluationsPage::PlanningEvaluationsPage(QWidget *parent)
    : QWidget(parent),
      m_cbProjet(nullptr), m_cbJury(nullptr), m_cbEquipe(nullptr), m_cbStatut(nullptr),
      m_dateDu(nullptr), m_dateAu(nullptr), m_calendrier(nullptr), m_lblMois(nullptr),
      m_btnAjouter(nullptr), m_btnExporter(nullptr), m_btnVue(nullptr), m_menuVue(nullptr),
      m_lblNombre(nullptr), m_inputRecherche(nullptr), m_table(nullptr), m_lblAffichage(nullptr),
      m_btnPrecedent(nullptr), m_btnSuivant(nullptr), m_pagesLayout(nullptr),
      m_vue(VueSemaine), m_triColonne(kColDate), m_triCroissant(true), m_page(1), m_taillePage(10),
      m_modificationAutorisee(true), m_chargement(false)
{
    setObjectName("pagePlanningEvaluations");
    construireInterface();

    // Période initiale : une semaine à partir de la première session à venir (à défaut, la dernière, sinon aujourd'hui)
    QDate debut = QDate::currentDate();
    const QVector<SessionEval> toutes = lireSessions();
    if (!toutes.isEmpty()) {
        const QDate seuil = QDate::currentDate().addDays(-1);
        debut = toutes.last().date;
        for (const SessionEval &s : toutes) {
            if (s.date >= seuil) { debut = s.date; break; }
        }
    }
    {
        QSignalBlocker b1(m_dateDu), b2(m_dateAu);
        m_dateDu->setDate(debut);
        m_dateAu->setDate(debut.addDays(6));
    }
    m_calendrier->setMois(debut);
    rafraichir();
}

void PlanningEvaluationsPage::construireInterface()
{
    QVBoxLayout *externe = new QVBoxLayout(this);
    externe->setContentsMargins(0, 0, 0, 0);

    QScrollArea *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("QScrollArea { background: transparent; border: none; } "
                          "QScrollArea > QWidget > QWidget { background: transparent; } "
                          "QScrollBar:vertical { background: transparent; width: 10px; } "
                          "QScrollBar::handle:vertical { background: #1F57A4; border-radius: 5px; min-height: 30px; } "
                          "QScrollBar:horizontal { background: transparent; height: 10px; } "
                          "QScrollBar::handle:horizontal { background: #1F57A4; border-radius: 5px; min-width: 30px; } "
                          "QScrollBar::add-line, QScrollBar::sub-line { height: 0px; width: 0px; }");
    externe->addWidget(scroll);

    QWidget *contenu = new QWidget();
    contenu->setObjectName("contenuPlanningEvaluations");
    contenu->setMinimumWidth(1120);
    scroll->setWidget(contenu);

    QVBoxLayout *principal = new QVBoxLayout(contenu);
    principal->setContentsMargins(22, 22, 22, 22);
    principal->setSpacing(18);
    principal->addWidget(construireEnTete());

    QHBoxLayout *corps = new QHBoxLayout();
    corps->setSpacing(18);

    QVBoxLayout *colonneGauche = new QVBoxLayout();
    colonneGauche->setSpacing(16);
    colonneGauche->addWidget(construireCarteFiltres());
    colonneGauche->addWidget(construireCarteCalendrier());
    colonneGauche->addStretch();
    corps->addLayout(colonneGauche, 0);
    corps->addWidget(construireCarteListe(), 1);
    principal->addLayout(corps, 1);
}

QWidget *PlanningEvaluationsPage::construireEnTete()
{
    QWidget *w = new QWidget();
    w->setStyleSheet("background: transparent;");
    QHBoxLayout *h = new QHBoxLayout(w);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(16);

    QPushButton *btnRetour = new QPushButton(" Retour");
    btnRetour->setObjectName("btnRetourPlanning");
    btnRetour->setIcon(QIcon(":/img/arrow_left.png"));
    btnRetour->setIconSize(QSize(18, 18));
    btnRetour->setFixedSize(120, 44);
    btnRetour->setCursor(Qt::PointingHandCursor);
    btnRetour->setStyleSheet("QPushButton { background-color: #011846; color: #FFFFFF; border: 1px solid #1E5BC0; "
                             "border-radius: 6px; font-size: 13px; font-weight: 600; } "
                             "QPushButton:hover { background-color: #06306F; border-color: #3B82F6; } "
                             "QPushButton:pressed { background-color: #031A48; }");
    connect(btnRetour, &QPushButton::clicked, this, &PlanningEvaluationsPage::retourDemande);

    QLabel *icone = new QLabel();
    QPixmap pm(":/img/calendar_cyan.png");
    pm.setDevicePixelRatio(pm.width() / 46.0);
    icone->setPixmap(pm);
    icone->setFixedSize(46, 46);
    icone->setStyleSheet("background: transparent;");

    QVBoxLayout *textes = new QVBoxLayout();
    textes->setSpacing(2);
    QLabel *titre = new QLabel("Planning des évaluations");
    titre->setStyleSheet("color: #FFFFFF; font-size: 21px; font-weight: bold; background: transparent;");
    QLabel *sousTitre = new QLabel("Consultez et gérez le planning des évaluations des projets par jury.");
    sousTitre->setStyleSheet("color: #19C3F0; font-size: 12px; background: transparent;");
    textes->addWidget(titre);
    textes->addWidget(sousTitre);

    h->addWidget(btnRetour, 0, Qt::AlignVCenter);
    h->addWidget(icone, 0, Qt::AlignVCenter);
    h->addLayout(textes);
    h->addStretch();

    // Boutons de droite : Ajouter une session / Exporter / Vue
    m_btnAjouter = new QPushButton(" Ajouter une session");
    m_btnAjouter->setObjectName("btnAjouterSession");
    m_btnAjouter->setIcon(QIcon(":/img/calendar_plus_white.png"));
    m_btnAjouter->setIconSize(QSize(20, 20));
    m_btnAjouter->setFixedSize(175, 44);
    m_btnAjouter->setCursor(Qt::PointingHandCursor);
    m_btnAjouter->setStyleSheet("QPushButton { background-color: #0AA9F5; color: #FFFFFF; border: none; border-radius: 6px; "
                                "font-size: 13px; font-weight: 600; } "
                                "QPushButton:hover { background-color: #2BBBFF; } "
                                "QPushButton:pressed { background-color: #0890D0; } "
                                "QPushButton:disabled { background-color: #1A3A6B; color: #7F93B8; }");
    connect(m_btnAjouter, &QPushButton::clicked, this, &PlanningEvaluationsPage::ajouterSessionDialogue);

    const QString cssSecondaire =
        "QPushButton { background-color: #011846; color: #FFFFFF; border: 1px solid #1E5BC0; border-radius: 6px; "
        "font-size: 13px; font-weight: 600; } "
        "QPushButton:hover { background-color: #06306F; border-color: #3B82F6; } "
        "QPushButton::menu-indicator { image: none; width: 0px; }";

    m_btnExporter = new QPushButton(" Exporter");
    m_btnExporter->setObjectName("btnExporterPlanning");
    m_btnExporter->setIcon(QIcon(":/img/download.png"));
    m_btnExporter->setIconSize(QSize(20, 20));
    m_btnExporter->setFixedSize(130, 44);
    m_btnExporter->setCursor(Qt::PointingHandCursor);
    m_btnExporter->setStyleSheet(cssSecondaire);
    QMenu *menuExport = new QMenu(m_btnExporter);
    menuExport->setStyleSheet(UiHelpers::cssMenu());
    QAction *aPdf = menuExport->addAction("Exporter en PDF");
    QAction *aCsv = menuExport->addAction("Exporter en CSV (Excel)");
    aPdf->setObjectName("actionExportPdfPlanning");
    aCsv->setObjectName("actionExportCsvPlanning");
    connect(aPdf, &QAction::triggered, this, [this]() { exporterDialogue(true); });
    connect(aCsv, &QAction::triggered, this, [this]() { exporterDialogue(false); });
    m_btnExporter->setMenu(menuExport);

    m_btnVue = new QPushButton(" Vue semaine");
    m_btnVue->setObjectName("btnVuePlanning");
    m_btnVue->setIcon(QIcon(":/img/calendar_white.png"));
    m_btnVue->setIconSize(QSize(20, 20));
    m_btnVue->setFixedSize(172, 44);
    m_btnVue->setCursor(Qt::PointingHandCursor);
    m_btnVue->setStyleSheet(cssSecondaire + " QPushButton { padding-right: 20px; }");
    m_menuVue = new QMenu(m_btnVue);
    m_menuVue->setStyleSheet(UiHelpers::cssMenu());
    QActionGroup *groupe = new QActionGroup(m_menuVue);
    groupe->setExclusive(true);
    const QList<QPair<QString, Vue>> vues = {{"Vue jour", VueJour}, {"Vue semaine", VueSemaine}, {"Vue mois", VueMois}};
    for (const auto &v : vues) {
        QAction *a = m_menuVue->addAction(v.first);
        a->setCheckable(true);
        a->setActionGroup(groupe);
        a->setChecked(v.second == m_vue);
        a->setData(int(v.second));
        connect(a, &QAction::triggered, this, [this, v]() { definirVue(v.second); });
    }
    m_btnVue->setMenu(m_menuVue);
    // Petite flèche à droite du bouton « Vue »
    QLabel *fleche = new QLabel(m_btnVue);
    QPixmap pf(":/img/chevron_down_white.png");
    pf.setDevicePixelRatio(pf.width() / 14.0);
    fleche->setPixmap(pf);
    fleche->setGeometry(172 - 28, 15, 14, 14);
    fleche->setStyleSheet("background: transparent;");
    fleche->setAttribute(Qt::WA_TransparentForMouseEvents);

    h->addWidget(m_btnAjouter);
    h->addWidget(m_btnExporter);
    h->addWidget(m_btnVue);
    return w;
}

QWidget *PlanningEvaluationsPage::construireCarteFiltres()
{
    QFrame *carte = new QFrame();
    carte->setObjectName("cartePlanning");
    carte->setStyleSheet(kCarteCss);
    carte->setFixedWidth(318);

    QVBoxLayout *v = new QVBoxLayout(carte);
    v->setContentsMargins(14, 14, 14, 16);
    v->setSpacing(6);

    QHBoxLayout *entete = new QHBoxLayout();
    entete->setSpacing(10);
    QLabel *ic = new QLabel();
    QPixmap pm(":/img/filter_cyan.png");
    pm.setDevicePixelRatio(pm.width() / 22.0);
    ic->setPixmap(pm);
    ic->setFixedSize(22, 22);
    ic->setStyleSheet("background: transparent; border: none;");
    QLabel *titre = new QLabel("Filtres");
    titre->setStyleSheet("color: #FFFFFF; font-size: 16px; font-weight: bold; background: transparent; border: none;");
    entete->addWidget(ic);
    entete->addWidget(titre);
    entete->addStretch();
    v->addLayout(entete);
    v->addSpacing(4);

    auto ajouterChamp = [&](const QString &libelle, QWidget *champ) {
        QLabel *l = new QLabel(libelle);
        l->setStyleSheet("color: #FFFFFF; font-size: 12px; font-weight: 600; background: transparent; border: none;");
        v->addWidget(l);
        v->addWidget(champ);
        v->addSpacing(4);
    };

    m_cbProjet = new QComboBox();
    m_cbJury = new QComboBox();
    m_cbEquipe = new QComboBox();
    m_cbStatut = new QComboBox();
    m_cbProjet->setObjectName("cbFiltreProjet");
    m_cbJury->setObjectName("cbFiltreJury");
    m_cbEquipe->setObjectName("cbFiltreEquipe");
    m_cbStatut->setObjectName("cbFiltreStatut");
    for (QComboBox *c : {m_cbProjet, m_cbJury, m_cbEquipe, m_cbStatut}) {
        c->setStyleSheet(kComboCss);
        c->setFixedHeight(36);
        c->setCursor(Qt::PointingHandCursor);
        connect(c, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) { surFiltreChange(); });
    }
    ajouterChamp("Projet", m_cbProjet);
    ajouterChamp("Jury", m_cbJury);
    ajouterChamp("Équipe", m_cbEquipe);
    ajouterChamp("Statut", m_cbStatut);

    QLabel *lblPeriode = new QLabel("Période");
    lblPeriode->setStyleSheet("color: #FFFFFF; font-size: 12px; font-weight: 600; background: transparent; border: none;");
    v->addWidget(lblPeriode);

    QHBoxLayout *periode = new QHBoxLayout();
    periode->setSpacing(8);
    m_dateDu = new QDateEdit(QDate::currentDate());
    m_dateAu = new QDateEdit(QDate::currentDate().addDays(6));
    m_dateDu->setObjectName("dateDu");
    m_dateAu->setObjectName("dateAu");
    for (QDateEdit *d : {m_dateDu, m_dateAu}) {
        d->setDisplayFormat(kFormatDate);
        d->setCalendarPopup(true);
        d->setStyleSheet(kDateCss);
        d->setFixedHeight(36);
        d->setCursor(Qt::PointingHandCursor);
        if (QCalendarWidget *cw = d->calendarWidget()) {
            cw->setStyleSheet(kCalendrierPopupCss);
            cw->setVerticalHeaderFormat(QCalendarWidget::NoVerticalHeader);
            cw->setFirstDayOfWeek(Qt::Monday);
        }
        connect(d, &QDateEdit::dateChanged, this, [this](const QDate &) { surDateModifiee(); });
    }
    QLabel *fleche = new QLabel();
    QPixmap pf(":/img/arrow_right_cyan.png");
    pf.setDevicePixelRatio(pf.width() / 16.0);
    fleche->setPixmap(pf);
    fleche->setFixedSize(16, 16);
    fleche->setStyleSheet("background: transparent; border: none;");
    periode->addWidget(m_dateDu, 1);
    periode->addWidget(fleche);
    periode->addWidget(m_dateAu, 1);
    v->addLayout(periode);
    return carte;
}

QWidget *PlanningEvaluationsPage::construireCarteCalendrier()
{
    QFrame *carte = new QFrame();
    carte->setObjectName("cartePlanning");
    carte->setStyleSheet(kCarteCss);
    carte->setFixedWidth(318);

    QVBoxLayout *v = new QVBoxLayout(carte);
    v->setContentsMargins(14, 14, 14, 14);
    v->setSpacing(10);

    QHBoxLayout *entete = new QHBoxLayout();
    entete->setSpacing(6);
    QLabel *ic = new QLabel();
    QPixmap pm(":/img/calendar_cyan.png");
    pm.setDevicePixelRatio(pm.width() / 26.0);
    ic->setPixmap(pm);
    ic->setFixedSize(26, 26);
    ic->setStyleSheet("background: transparent; border: none;");

    m_lblMois = new QLabel();
    m_lblMois->setAlignment(Qt::AlignCenter);
    m_lblMois->setStyleSheet("color: #FFFFFF; font-size: 14px; font-weight: 600; background: transparent; border: none;");

    const QString cssNav = "QPushButton { background-color: transparent; border: 1px solid #1F57A4; border-radius: 6px; } "
                           "QPushButton:hover { background-color: #173266; border-color: #3B82F6; }";
    QPushButton *prec = new QPushButton();
    prec->setObjectName("btnMoisPrecedent");
    prec->setIcon(QIcon(":/img/page_prev.png"));
    prec->setIconSize(QSize(13, 13));
    prec->setFixedSize(30, 30);
    prec->setCursor(Qt::PointingHandCursor);
    prec->setStyleSheet(cssNav);
    prec->setToolTip("Mois précédent");
    QPushButton *suiv = new QPushButton();
    suiv->setObjectName("btnMoisSuivant");
    suiv->setIcon(QIcon(":/img/page_next.png"));
    suiv->setIconSize(QSize(13, 13));
    suiv->setFixedSize(30, 30);
    suiv->setCursor(Qt::PointingHandCursor);
    suiv->setStyleSheet(cssNav);
    suiv->setToolTip("Mois suivant");
    connect(prec, &QPushButton::clicked, this, [this]() { changerMois(-1); });
    connect(suiv, &QPushButton::clicked, this, [this]() { changerMois(+1); });

    entete->addWidget(ic);
    entete->addWidget(prec);
    entete->addWidget(m_lblMois, 1);
    entete->addWidget(suiv);
    v->addLayout(entete);

    m_calendrier = new MiniCalendrier();
    m_calendrier->setObjectName("miniCalendrier");
    connect(m_calendrier, &MiniCalendrier::jourClique, this, &PlanningEvaluationsPage::surJourCalendrier);
    v->addWidget(m_calendrier);

    // Légende
    QHBoxLayout *legende = new QHBoxLayout();
    legende->setSpacing(6);
    const QList<QPair<QString, QString>> items = {{"Planifiée", "Évaluations"}, {"En cours", "En cours"}, {"Terminée", "Terminées"}};
    for (const auto &it : items) {
        QLabel *point = new QLabel();
        point->setFixedSize(10, 10);
        point->setStyleSheet(QString("background-color: %1; border-radius: 5px; border: none;").arg(couleurPoint(it.first).name()));
        QLabel *txt = new QLabel(it.second);
        txt->setStyleSheet("color: #E6EEFF; font-size: 11px; background: transparent; border: none;");
        legende->addWidget(point);
        legende->addWidget(txt);
        legende->addSpacing(8);
    }
    legende->addStretch();
    v->addLayout(legende);
    return carte;
}

QWidget *PlanningEvaluationsPage::construireCarteListe()
{
    QFrame *carte = new QFrame();
    carte->setObjectName("cartePlanning");
    carte->setStyleSheet(kCarteCss);

    QVBoxLayout *v = new QVBoxLayout(carte);
    v->setContentsMargins(14, 14, 14, 14);
    v->setSpacing(14);

    QHBoxLayout *entete = new QHBoxLayout();
    entete->setSpacing(12);
    QLabel *ic = new QLabel();
    QPixmap pm(":/img/calendar_cyan.png");
    pm.setDevicePixelRatio(pm.width() / 28.0);
    ic->setPixmap(pm);
    ic->setFixedSize(28, 28);
    ic->setStyleSheet("background: transparent; border: none;");
    QLabel *titre = new QLabel("Planning des sessions d'évaluation");
    titre->setStyleSheet("color: #FFFFFF; font-size: 16px; font-weight: bold; background: transparent; border: none;");
    entete->addWidget(ic);
    entete->addWidget(titre);
    entete->addStretch();

    m_lblNombre = new QLabel();
    m_lblNombre->setObjectName("lblNombreSessions");
    m_lblNombre->setStyleSheet("QLabel#lblNombreSessions { color: #E6EEFF; font-size: 12px; background-color: #011846; "
                               "border: 1px solid #1F57A4; border-radius: 8px; padding: 9px 14px 9px 38px; }");
    QLabel *icChip = new QLabel(m_lblNombre);
    QPixmap pc(":/img/calendar_cyan.png");
    pc.setDevicePixelRatio(pc.width() / 18.0);
    icChip->setPixmap(pc);
    icChip->setGeometry(13, 9, 18, 18);
    icChip->setStyleSheet("background: transparent; border: none;");
    entete->addWidget(m_lblNombre);
    v->addLayout(entete);

    // Recherche
    m_inputRecherche = new QLineEdit();
    m_inputRecherche->setObjectName("inputRecherchePlanning");
    m_inputRecherche->setPlaceholderText("Rechercher une session...");
    m_inputRecherche->setClearButtonEnabled(true);
    m_inputRecherche->setFixedHeight(40);
    m_inputRecherche->setStyleSheet("QLineEdit { background-color: #01286C; border: 1px solid #1F57A4; border-radius: 8px; "
                                    "padding-left: 40px; padding-right: 8px; color: #EAF2FF; font-size: 12px; } "
                                    "QLineEdit:focus { border: 1px solid #3B82F6; }");
    QPalette pal = m_inputRecherche->palette();
    pal.setColor(QPalette::PlaceholderText, QColor("#BDD6FF"));
    m_inputRecherche->setPalette(pal);
    QLabel *loupe = new QLabel(m_inputRecherche);
    QPixmap pl(":/img/search.png");
    pl.setDevicePixelRatio(pl.width() / 18.0);
    loupe->setPixmap(pl);
    loupe->setGeometry(14, 11, 18, 18);
    loupe->setStyleSheet("background: transparent; border: none;");
    loupe->setAttribute(Qt::WA_TransparentForMouseEvents);
    connect(m_inputRecherche, &QLineEdit::textChanged, this, &PlanningEvaluationsPage::surRecherche);
    v->addWidget(m_inputRecherche);

    // Tableau
    m_table = new QTableWidget();
    m_table->setObjectName("tablePlanning");
    EnteteTri *entetes = new EnteteTri(m_table);
    entetes->triables = {kColHeure};
    entetes->margesGauche[kColDate] = 0;
    m_table->setHorizontalHeader(entetes);
    m_table->setColumnCount(9);
    m_table->setHorizontalHeaderLabels({"", "Date", "Heure", "Projet", "Équipe", "Jury", "Salle", "Statut", "Actions"});
    m_table->verticalHeader()->setVisible(false);
    m_table->verticalHeader()->setDefaultSectionSize(46);
    m_table->setShowGrid(false);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setFocusPolicy(Qt::NoFocus);
    m_table->setWordWrap(false);
    m_table->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_table->setFrameShape(QFrame::NoFrame);
    m_table->setStyleSheet("QTableWidget { background: transparent; border: none; color: #E6EEFF; font-size: 12px; } "
                           "QTableWidget::item { border-bottom: 1px solid #0F3070; padding-left: 4px; }");
    entetes->setSectionResizeMode(QHeaderView::Fixed);
    entetes->setStretchLastSection(false);
    entetes->setFixedHeight(44);
    entetes->setSectionsClickable(true);
    entetes->setHighlightSections(false);
    entetes->setStyleSheet("QHeaderView { background: transparent; border: none; }");
    entetes->viewport()->setAutoFillBackground(false);
    connect(entetes, &QHeaderView::sectionClicked, this, &PlanningEvaluationsPage::surTriDemande);
    m_table->viewport()->installEventFilter(this);
    v->addWidget(m_table, 1);

    // Pied
    QHBoxLayout *pied = new QHBoxLayout();
    pied->setSpacing(8);
    m_lblAffichage = new QLabel();
    m_lblAffichage->setStyleSheet("color: #B8C7E8; font-size: 11px; background: transparent; border: none;");
    pied->addWidget(m_lblAffichage);
    pied->addStretch();

    const QString cssNav = "QPushButton { background-color: transparent; border: 1px solid #1F57A4; border-radius: 6px; } "
                           "QPushButton:hover { background-color: #173266; border-color: #3B82F6; } "
                           "QPushButton:disabled { border-color: #12326B; }";
    m_btnPrecedent = new QPushButton();
    m_btnPrecedent->setObjectName("btnPagePrecedentePlanning");
    m_btnPrecedent->setIcon(QIcon(":/img/page_prev.png"));
    m_btnPrecedent->setIconSize(QSize(14, 14));
    m_btnPrecedent->setFixedSize(34, 34);
    m_btnPrecedent->setCursor(Qt::PointingHandCursor);
    m_btnPrecedent->setStyleSheet(cssNav);
    m_btnPrecedent->setToolTip("Page précédente");
    connect(m_btnPrecedent, &QPushButton::clicked, this, [this]() { allerPage(m_page - 1); });
    m_btnSuivant = new QPushButton();
    m_btnSuivant->setObjectName("btnPageSuivantePlanning");
    m_btnSuivant->setIcon(QIcon(":/img/page_next.png"));
    m_btnSuivant->setIconSize(QSize(14, 14));
    m_btnSuivant->setFixedSize(34, 34);
    m_btnSuivant->setCursor(Qt::PointingHandCursor);
    m_btnSuivant->setStyleSheet(cssNav);
    m_btnSuivant->setToolTip("Page suivante");
    connect(m_btnSuivant, &QPushButton::clicked, this, [this]() { allerPage(m_page + 1); });

    m_pagesLayout = new QHBoxLayout();
    m_pagesLayout->setSpacing(8);
    m_pagesLayout->setContentsMargins(0, 0, 0, 0);
    pied->addWidget(m_btnPrecedent);
    pied->addLayout(m_pagesLayout);
    pied->addWidget(m_btnSuivant);
    v->addLayout(pied);
    return carte;
}

// ====================================================================
// CHARGEMENT
// ====================================================================

void PlanningEvaluationsPage::rafraichir()
{
    rechargerFiltres();
    chargerListe();
}

void PlanningEvaluationsPage::definirModificationAutorisee(bool autorisee)
{
    m_modificationAutorisee = autorisee;
    m_btnAjouter->setEnabled(autorisee);
    m_btnAjouter->setToolTip(autorisee ? QString() : "Réservé aux administrateurs / organisateurs");
    if (m_table) chargerListe();
}

void PlanningEvaluationsPage::rechargerFiltres()
{
    m_chargement = true;
    auto recharger = [](QComboBox *cb, const QString &tous, const QList<QPair<QString, QVariant>> &valeurs) {
        const QVariant ancien = cb->currentData();
        QSignalBlocker b(cb);
        cb->clear();
        cb->addItem(tous, QVariant());
        for (const auto &v : valeurs) cb->addItem(v.first, v.second);
        const int i = ancien.isValid() ? cb->findData(ancien) : -1;
        cb->setCurrentIndex(i >= 0 ? i : 0);
    };

    QList<QPair<QString, QVariant>> projets, jurys, equipes, statuts;
    QSqlQuery q;
    if (!q.exec(QStringLiteral("SELECT id_projet, titre FROM PROJET ORDER BY titre"))) {
        signalerErreurSql(QStringLiteral("Impossible de charger le filtre des projets du planning"), q.lastError());
    } else {
        while (q.next()) projets.append({q.value(1).toString(), q.value(0).toInt()});
    }
    QSqlQuery qj;
    if (!qj.exec(QStringLiteral("SELECT id_jury, nom, prenom FROM JURY ORDER BY nom, prenom"))) {
        signalerErreurSql(QStringLiteral("Impossible de charger le filtre des jurés du planning"), qj.lastError());
    } else {
        while (qj.next()) jurys.append({qj.value(1).toString() + " " + qj.value(2).toString(), qj.value(0).toInt()});
    }
    QSqlQuery qe;
    if (!qe.exec(QStringLiteral("SELECT DISTINCT nom_equipe FROM PROJET ORDER BY nom_equipe"))) {
        signalerErreurSql(QStringLiteral("Impossible de charger le filtre des équipes du planning"), qe.lastError());
    } else {
        while (qe.next()) equipes.append({qe.value(0).toString(), qe.value(0).toString()});
    }
    for (const QString &s : kStatuts) statuts.append({s, s});

    recharger(m_cbProjet, "Tous les projets", projets);
    recharger(m_cbJury, "Tous les jurés", jurys);
    recharger(m_cbEquipe, "Toutes les équipes", equipes);
    recharger(m_cbStatut, "Tous les statuts", statuts);
    m_chargement = false;
}

QVector<SessionEval> PlanningEvaluationsPage::sessionsFiltrees() const
{
    QVector<SessionEval> res;
    const QVariant fProjet = m_cbProjet->currentData();
    const QVariant fJury = m_cbJury->currentData();
    const QVariant fEquipe = m_cbEquipe->currentData();
    const QVariant fStatut = m_cbStatut->currentData();
    const QDate du = m_dateDu->date(), au = m_dateAu->date();
    const QString rech = m_inputRecherche->text().trimmed();

    for (const SessionEval &s : lireSessions()) {
        if (s.date < du || s.date > au) continue;
        if (fProjet.isValid() && s.idProjet != fProjet.toInt()) continue;
        if (fJury.isValid() && s.idJury != fJury.toInt()) continue;
        if (fEquipe.isValid() && s.equipe != fEquipe.toString()) continue;
        if (fStatut.isValid() && s.statut != fStatut.toString()) continue;
        if (!rech.isEmpty()) {
            const QString bloc = s.titreProjet + ' ' + s.descriptionProjet + ' ' + s.equipe + ' ' + s.nomJury + ' ' +
                                 s.prenomJury + ' ' + s.prenomJury + ' ' + s.nomJury + ' ' + s.salle + ' ' + s.statut + ' ' +
                                 s.date.toString(kFormatDate) + ' ' + s.heure.toString("HH:mm");
            if (!bloc.contains(rech, Qt::CaseInsensitive)) continue;
        }
        res.append(s);
    }

    std::stable_sort(res.begin(), res.end(), [this](const SessionEval &a, const SessionEval &b) {
        int c;
        if (m_triColonne == kColHeure) {
            c = a.heure < b.heure ? -1 : (a.heure > b.heure ? 1 : 0);
            if (c == 0) c = a.date < b.date ? -1 : (a.date > b.date ? 1 : 0);
        } else {
            c = a.date < b.date ? -1 : (a.date > b.date ? 1 : 0);
            if (c == 0) c = a.heure < b.heure ? -1 : (a.heure > b.heure ? 1 : 0);
        }
        return m_triCroissant ? c < 0 : c > 0;
    });
    return res;
}

QString PlanningEvaluationsPage::libellePeriode() const
{
    return m_dateDu->date().toString(kFormatDate) + " → " + m_dateAu->date().toString(kFormatDate);
}

void PlanningEvaluationsPage::mettreAJourCalendrier()
{
    m_calendrier->setPlage(m_dateDu->date(), m_dateAu->date());
    QMap<QDate, QStringList> marques;
    for (const SessionEval &s : lireSessions()) marques[s.date].append(s.statut);
    m_calendrier->setMarques(marques);
    const QDate m = m_calendrier->mois();
    m_lblMois->setText(kMois.at(m.month() - 1) + " " + QString::number(m.year()));
}

void PlanningEvaluationsPage::chargerListe()
{
    if (!m_table || m_chargement) return;

    // Le nettoyage des sessions orphelines est effectué lors de la suppression
    // d'un juré (Jury::supprimer), jamais pendant le simple chargement de la page.

    mettreAJourCalendrier();
    const QVector<SessionEval> sessions = sessionsFiltrees();

    const int total = sessions.size();
    const int nbPages = qMax(1, (total + m_taillePage - 1) / m_taillePage);
    m_page = qBound(1, m_page, nbPages);
    const int debut = (m_page - 1) * m_taillePage;
    const int fin = qMin(total, debut + m_taillePage);

    m_lblNombre->setText(pluriel(total, "session"));
    m_lblNombre->adjustSize();

    if (EnteteTri *e = dynamic_cast<EnteteTri *>(m_table->horizontalHeader())) {
        e->colonneActive = -1;   // maquette : titres en blanc, seule « Heure » porte l'icône de tri
        e->viewport()->update();
    }

    m_table->setRowCount(0);
    m_table->setRowCount(fin - debut);
    for (int i = debut; i < fin; ++i) {
        const int r = i - debut;
        const SessionEval &s = sessions.at(i);
        const int id = s.id;

        auto cellule = [&](int col, QWidget *w) {
            w->setStyleSheet("background: transparent;");
            m_table->setCellWidget(r, col, w);
        };
        auto item = [&](int col, const QString &texte) {
            QTableWidgetItem *it = new QTableWidgetItem(texte);
            it->setData(Qt::UserRole, id);
            m_table->setItem(r, col, it);
        };
        auto avatar = [](const QString &texte, const QString &couleur) {
            QLabel *a = new QLabel(texte);
            a->setFixedSize(24, 24);
            a->setAlignment(Qt::AlignCenter);
            a->setStyleSheet(QString("background-color: %1; color: #FFFFFF; border-radius: 12px; font-size: 9px; font-weight: bold;").arg(couleur));
            return a;
        };

        // Case à cocher
        QWidget *cCoche = new QWidget();
        QHBoxLayout *lc = new QHBoxLayout(cCoche);
        lc->setContentsMargins(0, 0, 0, 0);
        QCheckBox *chk = new QCheckBox();
        chk->setStyleSheet(UiHelpers::cssCheckbox());
        chk->setCursor(Qt::PointingHandCursor);
        connect(chk, &QCheckBox::toggled, this, &PlanningEvaluationsPage::surCaseLigne);
        lc->addWidget(chk, 0, Qt::AlignCenter);
        cellule(kColCoche, cCoche);
        item(kColCoche, QString());

        item(kColDate, s.date.toString(kFormatDate));
        item(kColHeure, s.heure.toString("HH:mm"));
        item(kColSalle, s.salle);

        // Projet : avatar + titre + sous-titre
        QWidget *cProjet = new QWidget();
        QHBoxLayout *lp = new QHBoxLayout(cProjet);
        lp->setContentsMargins(0, 0, 0, 0);
        lp->setSpacing(8);
        lp->addWidget(avatar(UiHelpers::initiales(s.titreProjet),
                             UiHelpers::couleurAvatar(UiHelpers::sommeCaracteres(s.titreProjet))));
        QVBoxLayout *tp = new QVBoxLayout();
        tp->setSpacing(0);
        tp->setContentsMargins(0, 0, 0, 0);
        QLabel *lblTitre = new QLabel(s.titreProjet);
        lblTitre->setStyleSheet("color: #F5FAFF; font-size: 12px; font-weight: 600; background: transparent;");
        QLabel *lblDesc = new QLabel(s.descriptionProjet);
        lblDesc->setStyleSheet("color: #B8C7E8; font-size: 10px; background: transparent;");
        tp->addStretch();
        tp->addWidget(lblTitre);
        if (!s.descriptionProjet.isEmpty()) tp->addWidget(lblDesc);
        tp->addStretch();
        lp->addLayout(tp, 1);
        cProjet->setToolTip(s.titreProjet + (s.descriptionProjet.isEmpty() ? QString() : " — " + s.descriptionProjet));
        cellule(kColProjet, cProjet);
        item(kColProjet, QString());

        // Équipe : avatar + nom
        QWidget *cEquipe = new QWidget();
        QHBoxLayout *le = new QHBoxLayout(cEquipe);
        le->setContentsMargins(0, 0, 0, 0);
        le->setSpacing(8);
        le->addWidget(avatar(UiHelpers::initiales(s.equipe), UiHelpers::couleurAvatar(UiHelpers::sommeCaracteres(s.equipe) + 3)));
        QLabel *lblEquipe = new QLabel(s.equipe);
        lblEquipe->setStyleSheet("color: #E6EEFF; font-size: 12px; background: transparent;");
        le->addWidget(lblEquipe, 1);
        cellule(kColEquipe, cEquipe);
        item(kColEquipe, QString());

        // Jury : avatar + « Nom Prénom »
        QWidget *cJury = new QWidget();
        QHBoxLayout *lj = new QHBoxLayout(cJury);
        lj->setContentsMargins(0, 0, 0, 0);
        lj->setSpacing(8);
        lj->addWidget(avatar(UiHelpers::initiales(s.nomJury), UiHelpers::couleurAvatar(s.idJury - 1)));
        QLabel *lblJury = new QLabel(s.nomCompletJury());
        lblJury->setStyleSheet("color: #E6EEFF; font-size: 12px; background: transparent;");
        lj->addWidget(lblJury, 1);
        cellule(kColJury, cJury);
        item(kColJury, QString());

        // Statut
        QWidget *cStatut = new QWidget();
        QHBoxLayout *ls = new QHBoxLayout(cStatut);
        ls->setContentsMargins(0, 0, 0, 0);
        QLabel *badge = new QLabel(s.statut);
        badge->setAlignment(Qt::AlignCenter);
        badge->setFixedSize(66, 24);
        badge->setStyleSheet(QString("background-color: %1; color: #FFFFFF; border-radius: 5px; font-size: 11px; font-weight: bold;")
                                 .arg(couleurStatut(s.statut)));
        ls->addWidget(badge);
        ls->addStretch();
        cellule(kColStatut, cStatut);
        item(kColStatut, QString());

        // Actions : consulter / modifier / supprimer
        QWidget *cActions = new QWidget();
        QHBoxLayout *la = new QHBoxLayout(cActions);
        la->setContentsMargins(0, 0, 0, 0);
        la->setSpacing(10);
        QPushButton *bVoir = UiHelpers::boutonAction("act_eye.png", "Consulter la session");
        QPushButton *bEdit = UiHelpers::boutonAction("act_edit.png", "Modifier la session");
        QPushButton *bSupp = UiHelpers::boutonAction("act_trash.png", "Supprimer la session");
        bVoir->setObjectName("btnVoirSession");
        bEdit->setObjectName("btnModifierSession");
        bSupp->setObjectName("btnSupprimerSession");
        bEdit->setEnabled(m_modificationAutorisee);
        bSupp->setEnabled(m_modificationAutorisee);
        la->addWidget(bVoir);
        la->addWidget(bEdit);
        la->addWidget(bSupp);
        la->addStretch();
        connect(bVoir, &QPushButton::clicked, this, [this, id]() { detailSession(id); });
        connect(bEdit, &QPushButton::clicked, this, [this, id]() { dialogueSession(id); });
        connect(bSupp, &QPushButton::clicked, this, [this, id]() { confirmerSuppression(id); });
        cellule(kColActions, cActions);
        item(kColActions, QString());
    }

    definirToutCoche(false);
    mettreAJourPagination(total, debut, fin, nbPages);
}

void PlanningEvaluationsPage::mettreAJourPagination(int total, int debut, int fin, int nbPages)
{
    if (total == 0) m_lblAffichage->setText("Aucune session ne correspond aux filtres");
    else m_lblAffichage->setText(QString("Affichage de %1 à %2 sur %3").arg(debut + 1).arg(fin).arg(pluriel(total, "session")));

    while (QLayoutItem *it = m_pagesLayout->takeAt(0)) {
        if (QWidget *w = it->widget()) w->deleteLater();
        delete it;
    }
    for (int p = 1; p <= nbPages; ++p) {
        QPushButton *b = new QPushButton(QString::number(p));
        b->setFixedSize(34, 34);
        b->setCursor(Qt::PointingHandCursor);
        b->setObjectName(QString("btnPagePlanning%1").arg(p));
        b->setStyleSheet(p == m_page
            ? "QPushButton { background-color: #0391FC; color: #FFFFFF; border: none; border-radius: 6px; font-size: 13px; font-weight: bold; }"
            : "QPushButton { background-color: transparent; color: #FFFFFF; border: 1px solid #1F57A4; border-radius: 6px; font-size: 13px; font-weight: bold; } "
              "QPushButton:hover { background-color: #173266; border-color: #3B82F6; }");
        connect(b, &QPushButton::clicked, this, [this, p]() { allerPage(p); });
        m_pagesLayout->addWidget(b);
    }
    m_btnPrecedent->setEnabled(m_page > 1);
    m_btnSuivant->setEnabled(m_page < nbPages);
}

// ====================================================================
// FILTRES, PÉRIODE, VUE
// ====================================================================

void PlanningEvaluationsPage::surFiltreChange()
{
    if (m_chargement) return;
    m_page = 1;
    chargerListe();
}

void PlanningEvaluationsPage::surRecherche(const QString &)
{
    m_page = 1;
    chargerListe();
}

void PlanningEvaluationsPage::surDateModifiee()
{
    // Les deux bornes restent cohérentes : début <= fin
    QDateEdit *emetteur = qobject_cast<QDateEdit *>(sender());
    if (m_dateDu->date() > m_dateAu->date()) {
        if (emetteur == m_dateDu) { QSignalBlocker b(m_dateAu); m_dateAu->setDate(m_dateDu->date()); }
        else { QSignalBlocker b(m_dateDu); m_dateDu->setDate(m_dateAu->date()); }
    }
    if (emetteur == m_dateDu) m_calendrier->setMois(m_dateDu->date());
    m_page = 1;
    chargerListe();
}

void PlanningEvaluationsPage::definirPeriode(const QDate &debut, const QDate &fin)
{
    {
        QSignalBlocker b1(m_dateDu), b2(m_dateAu);
        m_dateDu->setDate(debut);
        m_dateAu->setDate(fin < debut ? debut : fin);
    }
    m_page = 1;
    chargerListe();
}

void PlanningEvaluationsPage::appliquerVue()
{
    QDate debut = m_dateDu->date();
    QDate fin;
    switch (m_vue) {
    case VueJour:    fin = debut; break;
    case VueSemaine: fin = debut.addDays(6); break;
    case VueMois:
        debut = QDate(debut.year(), debut.month(), 1);
        fin = debut.addMonths(1).addDays(-1);
        break;
    }
    definirPeriode(debut, fin);
}

void PlanningEvaluationsPage::definirVue(Vue vue)
{
    m_vue = vue;
    static const QStringList noms = {"Vue jour", "Vue semaine", "Vue mois"};
    m_btnVue->setText(" " + noms.at(int(vue)));
    for (QAction *a : m_menuVue->actions()) a->setChecked(a->data().toInt() == int(vue));
    appliquerVue();
}

void PlanningEvaluationsPage::surJourCalendrier(const QDate &date)
{
    if (date.month() != m_calendrier->mois().month()) m_calendrier->setMois(date);
    {
        QSignalBlocker b1(m_dateDu), b2(m_dateAu);
        m_dateDu->setDate(date);
    }
    appliquerVue();
}

void PlanningEvaluationsPage::changerMois(int delta)
{
    m_calendrier->setMois(m_calendrier->mois().addMonths(delta));
    const QDate m = m_calendrier->mois();
    m_lblMois->setText(kMois.at(m.month() - 1) + " " + QString::number(m.year()));
}

void PlanningEvaluationsPage::surTriDemande(int colonne)
{
    if (colonne == kColCoche) {
        definirToutCoche(!toutCoche());
        for (int r = 0; r < m_table->rowCount(); ++r) {
            if (QWidget *w = m_table->cellWidget(r, kColCoche)) {
                if (QCheckBox *c = w->findChild<QCheckBox *>()) {
                    QSignalBlocker b(c);
                    c->setChecked(toutCoche());
                }
            }
        }
        return;
    }
    if (colonne != kColDate && colonne != kColHeure) return;
    if (m_triColonne == colonne) m_triCroissant = !m_triCroissant;
    else { m_triColonne = colonne; m_triCroissant = true; }
    m_page = 1;
    chargerListe();
}

void PlanningEvaluationsPage::surCaseLigne()
{
    bool toutes = m_table->rowCount() > 0;
    for (int r = 0; r < m_table->rowCount() && toutes; ++r) {
        QWidget *w = m_table->cellWidget(r, kColCoche);
        QCheckBox *c = w ? w->findChild<QCheckBox *>() : nullptr;
        if (!c || !c->isChecked()) toutes = false;
    }
    definirToutCoche(toutes);
}

bool PlanningEvaluationsPage::toutCoche() const
{
    const EnteteTri *e = dynamic_cast<const EnteteTri *>(m_table->horizontalHeader());
    return e && e->toutCoche;
}

void PlanningEvaluationsPage::definirToutCoche(bool coche)
{
    if (EnteteTri *e = dynamic_cast<EnteteTri *>(m_table->horizontalHeader())) {
        e->toutCoche = coche;
        e->viewport()->update();
    }
}

void PlanningEvaluationsPage::allerPage(int page)
{
    m_page = page;
    chargerListe();
}

// ====================================================================
// DIALOGUES : ajout / modification / détail / suppression / export
// ====================================================================

void PlanningEvaluationsPage::ajouterSessionDialogue()
{
    dialogueSession(-1);
}

void PlanningEvaluationsPage::dialogueSession(int idSession)
{
    if (!m_modificationAutorisee) {
        QMessageBox::information(this, "Accès restreint",
                                 "Seul un administrateur / organisateur peut ajouter ou modifier une session.");
        return;
    }
    const bool modification = idSession > 0;
    SessionEval existante;
    if (modification) {
        existante = lireSession(idSession);
        if (existante.id <= 0) {
            QMessageBox::warning(this, "Session introuvable", "Cette session n'existe plus.");
            rafraichir();
            return;
        }
    }

    QSqlQuery qp;
    QList<QPair<int, QString>> projets;
    if (!qp.exec(QStringLiteral("SELECT id_projet, titre FROM PROJET ORDER BY titre"))) {
        signalerErreurSql(QStringLiteral("Impossible de charger les projets du dialogue de session"), qp.lastError());
        return;
    }
    while (qp.next()) projets.append({qp.value(0).toInt(), qp.value(1).toString()});
    QSqlQuery qj;
    QList<QPair<int, QString>> jurys;
    if (!qj.exec(QStringLiteral("SELECT id_jury, nom, prenom FROM JURY ORDER BY nom, prenom"))) {
        signalerErreurSql(QStringLiteral("Impossible de charger les jurés du dialogue de session"), qj.lastError());
        return;
    }
    while (qj.next()) jurys.append({qj.value(0).toInt(), qj.value(1).toString() + " " + qj.value(2).toString()});
    if (projets.isEmpty() || jurys.isEmpty()) {
        QMessageBox::warning(this, "Données manquantes",
                             "Il faut au moins un projet et un juré pour planifier une session.");
        return;
    }

    QDialog dlg(this);
    dlg.setObjectName("dialogueSession");
    dlg.setWindowTitle(modification ? QString("Modifier la session #%1").arg(idSession) : "Ajouter une session");
    dlg.setStyleSheet(kDialogCss);
    dlg.setMinimumWidth(480);

    QVBoxLayout *principal = new QVBoxLayout(&dlg);
    principal->setContentsMargins(24, 22, 24, 22);
    principal->setSpacing(14);
    QLabel *titre = new QLabel(modification ? "Modifier la session" : "Ajouter une session");
    titre->setStyleSheet("color: #F5FAFF; font-size: 20px; font-weight: bold;");
    principal->addWidget(titre);

    QComboBox *cbProjet = new QComboBox();
    for (const auto &p : projets) cbProjet->addItem(p.second, p.first);
    QComboBox *cbJury = new QComboBox();
    for (const auto &j : jurys) cbJury->addItem(j.second, j.first);
    QDateEdit *edDate = new QDateEdit(modification ? existante.date : (m_dateDu->date() >= QDate::currentDate() ? m_dateDu->date() : QDate::currentDate()));
    edDate->setDisplayFormat(kFormatDate);
    edDate->setCalendarPopup(true);
    if (QCalendarWidget *cw = edDate->calendarWidget()) {
        cw->setStyleSheet(kCalendrierPopupCss);
        cw->setVerticalHeaderFormat(QCalendarWidget::NoVerticalHeader);
        cw->setFirstDayOfWeek(Qt::Monday);
    }
    QTimeEdit *edHeure = new QTimeEdit(modification ? existante.heure : QTime(9, 0));
    edHeure->setDisplayFormat("HH:mm");
    QComboBox *cbSalle = new QComboBox();
    cbSalle->setEditable(true);
    if (cbSalle->lineEdit()) cbSalle->lineEdit()->setMaxLength(100);
    QStringList salles = {"Salle 1", "Salle 2", "Salle 3", "Salle 4", "Salle 5"};
    QSqlQuery qs;
    if (!qs.exec(QStringLiteral("SELECT DISTINCT salle FROM SESSION_EVALUATION ORDER BY salle"))) {
        signalerErreurSql(QStringLiteral("Impossible de charger la liste des salles"), qs.lastError());
    } else {
        while (qs.next()) if (!salles.contains(qs.value(0).toString())) salles << qs.value(0).toString();
    }
    cbSalle->addItems(salles);
    QComboBox *cbStatut = new QComboBox();
    cbStatut->addItems(kStatuts);

    if (modification) {
        cbProjet->setCurrentIndex(qMax(0, cbProjet->findData(existante.idProjet)));
        cbJury->setCurrentIndex(qMax(0, cbJury->findData(existante.idJury)));
        cbSalle->setCurrentText(existante.salle);
        cbStatut->setCurrentIndex(qMax(0, cbStatut->findText(existante.statut)));
    }

    QGridLayout *grille = new QGridLayout();
    grille->setHorizontalSpacing(14);
    grille->setVerticalSpacing(10);
    int ligne = 0;
    auto champ = [&](const QString &libelle, QWidget *w) {
        grille->addWidget(new QLabel(libelle), ligne, 0);
        grille->addWidget(w, ligne, 1);
        ++ligne;
    };
    champ("Projet", cbProjet);
    champ("Juré", cbJury);
    champ("Date", edDate);
    champ("Heure", edHeure);
    champ("Salle", cbSalle);
    champ("Statut", cbStatut);
    grille->setColumnStretch(1, 1);
    principal->addLayout(grille);

    QHBoxLayout *boutons = new QHBoxLayout();
    boutons->addStretch();
    QPushButton *btnAnnuler = new QPushButton("Annuler");
    QPushButton *btnOk = new QPushButton(modification ? "Enregistrer" : "Ajouter");
    btnAnnuler->setObjectName("btnAnnulerSession");
    btnOk->setObjectName("btnValiderSession");
    btnOk->setProperty("principal", true);
    btnAnnuler->setCursor(Qt::PointingHandCursor);
    btnOk->setCursor(Qt::PointingHandCursor);
    boutons->addWidget(btnAnnuler);
    boutons->addWidget(btnOk);
    principal->addLayout(boutons);
    connect(btnAnnuler, &QPushButton::clicked, &dlg, &QDialog::reject);

    SessionEval saisie;
    connect(btnOk, &QPushButton::clicked, &dlg, [&]() {
        saisie = existante;
        saisie.date = edDate->date();
        saisie.heure = edHeure->time();
        saisie.idProjet = cbProjet->currentData().toInt();
        saisie.idJury = cbJury->currentData().toInt();
        saisie.salle = cbSalle->currentText().trimmed();
        saisie.statut = cbStatut->currentText();
        if (saisie.salle.isEmpty()) {
            QMessageBox::warning(&dlg, "Saisie invalide", "Veuillez indiquer la salle.");
            return;
        }
        if (saisie.salle.size() > 100) {
            QMessageBox::warning(&dlg, "Saisie invalide", "La salle ne peut pas dépasser 100 caractères.");
            return;
        }
        const ResultatVerificationConflit verification = verifierConflit(
            saisie.date, saisie.heure, saisie.idJury, saisie.salle, modification ? saisie.id : -1);
        if (verification.etat == ResultatVerificationConflit::ErreurSql) {
            QMessageBox::critical(&dlg, "Erreur de vérification",
                                  "Impossible de vérifier les conflits de planning.\n"
                                  "L'enregistrement est annulé.\n\n" + verification.message);
            return;
        }
        if (verification.etat == ResultatVerificationConflit::ConflitDetecte) {
            QMessageBox::warning(&dlg, "Conflit de planning", verification.message);
            return;
        }
        QString erreurEnregistrement;
        const bool ok = modification ? modifierSession(saisie, &erreurEnregistrement)
                                     : ajouterSession(saisie, &saisie.id, &erreurEnregistrement);
        if (!ok) {
            QMessageBox::critical(&dlg, "Erreur",
                                  "L'enregistrement dans la base de données a échoué :\n" +
                                  (erreurEnregistrement.isEmpty() ? QStringLiteral("Erreur inconnue.")
                                                                  : erreurEnregistrement));
            return;
        }
        dlg.accept();
    });

    if (dlg.exec() == QDialog::Accepted) {
        // Si la session sort de la période affichée, on déplace la période pour qu'elle reste visible
        if (saisie.date < m_dateDu->date() || saisie.date > m_dateAu->date()) {
            m_dateDu->blockSignals(true);
            m_dateDu->setDate(saisie.date);
            m_dateDu->blockSignals(false);
            m_calendrier->setMois(saisie.date);
            appliquerVue();
        }
        rafraichir();
        QMessageBox::information(this, "Succès", modification ? "La session a été modifiée avec succès."
                                                              : "La session a été ajoutée avec succès.");
    }
}

void PlanningEvaluationsPage::detailSession(int idSession)
{
    const SessionEval s = lireSession(idSession);
    if (s.id <= 0) {
        QMessageBox::warning(this, "Session introuvable", "Cette session n'existe plus.");
        rafraichir();
        return;
    }
    const int duree = ParametresJury::charger().dureeEvaluationMin;
    const QTime fin = s.heure.addSecs(duree * 60);

    QString note = "Pas encore évalué";
    QSqlQuery q;
    q.prepare("SELECT note FROM EVALUATION WHERE id_jury = ? AND id_projet = ? ORDER BY date_evaluation DESC");
    q.addBindValue(s.idJury);
    q.addBindValue(s.idProjet);
    if (!q.exec()) {
        signalerErreurSql(QStringLiteral("Impossible de lire la note de la session #%1").arg(idSession), q.lastError());
    } else if (q.next()) {
        note = QString::number(q.value(0).toDouble(), 'f', 1) + " / " +
               QString::number(ParametresJury::charger().noteTotaleParProjet);
    }

    QDialog dlg(this);
    dlg.setObjectName("dialogueDetailSession");
    dlg.setWindowTitle(QString("Session #%1 — %2").arg(s.id).arg(s.titreProjet));
    dlg.setStyleSheet(kDialogCss);
    dlg.setMinimumWidth(460);
    QVBoxLayout *v = new QVBoxLayout(&dlg);
    v->setContentsMargins(24, 22, 24, 22);
    v->setSpacing(14);

    QLabel *titre = new QLabel("Détail de la session");
    titre->setStyleSheet("color: #F5FAFF; font-size: 20px; font-weight: bold;");
    v->addWidget(titre);

    QLabel *badge = new QLabel(s.statut);
    badge->setAlignment(Qt::AlignCenter);
    badge->setFixedSize(78, 26);
    badge->setStyleSheet(QString("background-color: %1; color: #FFFFFF; border-radius: 5px; font-size: 12px; font-weight: bold;").arg(couleurStatut(s.statut)));
    v->addWidget(badge);

    QGridLayout *g = new QGridLayout();
    g->setHorizontalSpacing(16);
    g->setVerticalSpacing(8);
    int l = 0;
    auto ligne = [&](const QString &k, const QString &val) {
        QLabel *a = new QLabel(k);
        a->setStyleSheet("color: #B8C7E8; font-size: 12px;");
        QLabel *b = new QLabel(val);
        b->setStyleSheet("color: #FFFFFF; font-size: 13px; font-weight: 600;");
        b->setWordWrap(true);
        g->addWidget(a, l, 0, Qt::AlignTop);
        g->addWidget(b, l, 1);
        ++l;
    };
    ligne("Projet", s.titreProjet + (s.descriptionProjet.isEmpty() ? QString() : "\n" + s.descriptionProjet));
    ligne("Équipe", s.equipe);
    ligne("Juré", s.nomCompletJury());
    ligne("Date", s.date.toString("dddd dd/MM/yyyy"));
    ligne("Horaire", QString("%1 → %2 (%3 min)").arg(s.heure.toString("HH:mm"), fin.toString("HH:mm")).arg(duree));
    ligne("Salle", s.salle);
    ligne("Note attribuée", note);
    g->setColumnStretch(1, 1);
    v->addLayout(g);

    QHBoxLayout *boutons = new QHBoxLayout();
    boutons->addStretch();
    QPushButton *fermer = new QPushButton("Fermer");
    fermer->setObjectName("btnFermerDetailSession");
    fermer->setProperty("principal", true);
    fermer->setCursor(Qt::PointingHandCursor);
    boutons->addWidget(fermer);
    v->addLayout(boutons);
    connect(fermer, &QPushButton::clicked, &dlg, &QDialog::accept);
    dlg.exec();
}

void PlanningEvaluationsPage::confirmerSuppression(int idSession)
{
    if (!m_modificationAutorisee) {
        QMessageBox::information(this, "Accès restreint", "Seul un administrateur / organisateur peut supprimer une session.");
        return;
    }
    const SessionEval s = lireSession(idSession);
    if (s.id <= 0) {
        rafraichir();
        return;
    }
    const QString question = QString("Voulez-vous vraiment supprimer la session suivante ?\n\n%1 — %2\n%3 à %4 (%5)")
                                 .arg(s.titreProjet, s.nomCompletJury(), s.date.toString(kFormatDate), s.heure.toString("HH:mm"), s.salle);
    if (QMessageBox::question(this, "Confirmation de suppression", question, QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;
    if (supprimerSession(idSession)) {
        rafraichir();
        QMessageBox::information(this, "Suppression effectuée", "La session a été supprimée du planning.");
    } else {
        QMessageBox::critical(this, "Erreur", "Impossible de supprimer cette session.");
    }
}

void PlanningEvaluationsPage::exporterDialogue(bool pdf)
{
    const QVector<SessionEval> sessions = sessionsFiltrees();
    if (sessions.isEmpty()) {
        QMessageBox::information(this, "Export", "Aucune session à exporter pour les filtres actuels.");
        return;
    }
    const QString base = "planning_evaluations_" + QDate::currentDate().toString("yyyyMMdd");
    const QString chemin = QFileDialog::getSaveFileName(
        this, pdf ? "Exporter le planning en PDF" : "Exporter le planning en CSV", base + (pdf ? ".pdf" : ".csv"),
        pdf ? "Fichier PDF (*.pdf)" : "Fichier CSV (*.csv)");
    if (chemin.isEmpty()) return;

    const bool ok = pdf ? exporterPDF(chemin, sessions, libellePeriode()) : exporterCSV(chemin, sessions);
    if (ok) QMessageBox::information(this, "Export réussi", QString("%1 exportée(s) dans :\n%2").arg(pluriel(sessions.size(), "session"), chemin));
    else QMessageBox::critical(this, "Erreur", "L'export a échoué (fichier inaccessible ?).");
}

// ====================================================================
// MISE EN PAGE DYNAMIQUE
// ====================================================================

bool PlanningEvaluationsPage::eventFilter(QObject *obj, QEvent *event)
{
    if (m_table && obj == m_table->viewport() && (event->type() == QEvent::Resize || event->type() == QEvent::Show)) {
        const int total = m_table->viewport()->width();
        const int coche = 40, date = 84, heure = 56, salle = 56, statut = 82, actions = 104;
        const int reste = qMax(300, total - coche - date - heure - salle - statut - actions);
        QHeaderView *h = m_table->horizontalHeader();
        const int projet = reste * 38 / 100, equipe = reste * 28 / 100;
        h->resizeSection(kColCoche, coche);
        h->resizeSection(kColDate, date);
        h->resizeSection(kColHeure, heure);
        h->resizeSection(kColProjet, projet);
        h->resizeSection(kColEquipe, equipe);
        h->resizeSection(kColJury, reste - projet - equipe);
        h->resizeSection(kColSalle, salle);
        h->resizeSection(kColStatut, statut);
        h->resizeSection(kColActions, actions);
    }
    return QWidget::eventFilter(obj, event);
}
