#include "employe.h"

#include <QSqlError>
#include <QDebug>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QSettings>
#include <QSslSocket>
#include <QRandomGenerator>
#include <QCryptographicHash>
#include <QPasswordDigestor>
#include <QDateTime>
#include <QRegularExpression>
#include <QXmlStreamReader>
#include <QtMath>
#include <algorithm>

#include <private/qzipreader_p.h>
#if defined(HAVE_QTPDF) && QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
#  include <QPdfDocument>
#  include <QPdfSelection>
#  define CV_PDF 1
#endif

// ===========================================================================
//  PARTIE 1 : CLASSE EMPLOYE (CRUD ORACLE)
// ===========================================================================
Employe::Employe() : id(0) {}

Employe::Employe(int id, const QString &nom, const QString &prenom, const QString &email,
                 const QString &role, const QString &departement, const QString &equipe,
                 const QString &disponibilite)
    : id(id), nom(nom), prenom(prenom), email(email), role(role),
      departement(departement), equipe(equipe), disponibilite(disponibilite) {}

int Employe::prochainId() {
    QSqlQuery q("SELECT NVL(MAX(ID_EMPLOYE), 0) + 1 FROM EMPLOYE");
    if (q.next()) {
        return q.value(0).toInt();
    }
    return 1;
}

bool Employe::emailExiste(const QString &email, int excludeId) {
    QSqlQuery query;
    if (excludeId > 0) {
        query.prepare("SELECT ID_EMPLOYE FROM EMPLOYE WHERE LOWER(EMAIL) = LOWER(:email) AND ID_EMPLOYE <> :id");
        query.bindValue(":email", email.trimmed());
        query.bindValue(":id", excludeId);
    } else {
        query.prepare("SELECT ID_EMPLOYE FROM EMPLOYE WHERE LOWER(EMAIL) = LOWER(:email)");
        query.bindValue(":email", email.trimmed());
    }
    if (query.exec() && query.next()) {
        return true;
    }
    return false;
}

bool Employe::ajouter() {
    lastError.clear();
    QSqlQuery query;
    query.prepare("INSERT INTO EMPLOYE (ID_EMPLOYE, NOM, PRENOM, EMAIL, ROLE, DEPARTEMENT, EQUIPE, DISPONIBILITE) "
                  "VALUES (:id, :nom, :prenom, :email, :role, :departement, :equipe, :disponibilite)");

    query.bindValue(":id", id);
    query.bindValue(":nom", nom);
    query.bindValue(":prenom", prenom);
    query.bindValue(":email", email);
    query.bindValue(":role", role);
    query.bindValue(":departement", departement);
    query.bindValue(":equipe", equipe);
    query.bindValue(":disponibilite", disponibilite);

    if (!query.exec()) {
        lastError = query.lastError().text();
        qDebug() << "Erreur ajout Employe :" << lastError;
        return false;
    }
    // Validation immédiate dans la base de données Oracle (COMMIT)
    QSqlQuery commitQuery;
    commitQuery.exec("COMMIT");
    QSqlDatabase::database().commit();
    return true;
}

QSqlQueryModel * Employe::afficher() {
    auto *model = new QSqlQueryModel();
    model->setQuery("SELECT ID_EMPLOYE, NOM, PRENOM, EMAIL, ROLE, DEPARTEMENT, EQUIPE, DISPONIBILITE FROM EMPLOYE ORDER BY ID_EMPLOYE ASC");

    model->setHeaderData(0, Qt::Horizontal, QObject::tr("ID"));
    model->setHeaderData(1, Qt::Horizontal, QObject::tr("Nom"));
    model->setHeaderData(2, Qt::Horizontal, QObject::tr("Prénom"));
    model->setHeaderData(3, Qt::Horizontal, QObject::tr("E-mail"));
    model->setHeaderData(4, Qt::Horizontal, QObject::tr("Rôle"));
    model->setHeaderData(5, Qt::Horizontal, QObject::tr("Département"));
    model->setHeaderData(6, Qt::Horizontal, QObject::tr("Équipe(s)"));
    model->setHeaderData(7, Qt::Horizontal, QObject::tr("Disponibilité"));

    return model;
}

bool Employe::supprimer(int id) {
    lastError.clear();
    QSqlQuery query;
    query.prepare("DELETE FROM EMPLOYE WHERE ID_EMPLOYE = :id");
    query.bindValue(":id", id);

    if (!query.exec()) {
        lastError = query.lastError().text();
        qDebug() << "Erreur suppression Employe :" << lastError;
        return false;
    }
    // Validation immédiate dans la base de données Oracle (COMMIT)
    QSqlQuery commitQuery;
    commitQuery.exec("COMMIT");
    QSqlDatabase::database().commit();

    // Réorganisation automatique et ordonnée des IDs (1, 2, 3, 4...) sans aucun trou
    reordonnerIds();

    return true;
}

void Employe::reordonnerIds() {
    // 1. Lire tous les identifiants existants dans l'ordre croissant
    QSqlQuery sel("SELECT ID_EMPLOYE FROM EMPLOYE ORDER BY ID_EMPLOYE ASC");
    QList<int> oldIds;
    while (sel.next()) {
        oldIds.append(sel.value(0).toInt());
    }

    // 2. Si déjà séquentiels (1, 2, 3...), rien à faire
    bool need = false;
    for (int i = 0; i < oldIds.size(); ++i) {
        if (oldIds[i] != (i + 1)) {
            need = true;
            break;
        }
    }
    if (!need) return;

    // 3. Passer temporairement en négatif pour éviter tout conflit de clé primaire
    for (int oldId : oldIds) {
        QSqlQuery q;
        q.prepare("UPDATE EMPLOYE SET ID_EMPLOYE = :neg WHERE ID_EMPLOYE = :pos");
        q.bindValue(":neg", -oldId);
        q.bindValue(":pos", oldId);
        q.exec();
    }

    // 4. Réattribuer les identifiants ordonnés consécutifs : 1, 2, 3, 4, ...
    for (int i = 0; i < oldIds.size(); ++i) {
        int oldId = oldIds[i];
        int newId = i + 1;

        QSqlQuery q;
        q.prepare("UPDATE EMPLOYE SET ID_EMPLOYE = :newId WHERE ID_EMPLOYE = :neg");
        q.bindValue(":newId", newId);
        q.bindValue(":neg", -oldId);
        q.exec();

        if (oldId != newId) {
            QSqlQuery qPart;
            qPart.prepare("UPDATE PARTICIPANT SET ID_EMPLOYE = :newId WHERE ID_EMPLOYE = :oldId");
            qPart.bindValue(":newId", newId);
            qPart.bindValue(":oldId", oldId);
            qPart.exec();
        }
    }

    // 5. Validation finale dans Oracle (COMMIT)
    QSqlQuery commitQ;
    commitQ.exec("COMMIT");
    QSqlDatabase::database().commit();
}

bool Employe::modifier(int id) {
    lastError.clear();
    QSqlQuery query;
    query.prepare("UPDATE EMPLOYE SET NOM = :nom, PRENOM = :prenom, EMAIL = :email, ROLE = :role, "
                  "DEPARTEMENT = :departement, EQUIPE = :equipe, DISPONIBILITE = :disponibilite "
                  "WHERE ID_EMPLOYE = :id");

    query.bindValue(":id", id);
    query.bindValue(":nom", nom);
    query.bindValue(":prenom", prenom);
    query.bindValue(":email", email);
    query.bindValue(":role", role);
    query.bindValue(":departement", departement);
    query.bindValue(":equipe", equipe);
    query.bindValue(":disponibilite", disponibilite);

    if (!query.exec()) {
        lastError = query.lastError().text();
        qDebug() << "Erreur modification Employe :" << lastError;
        return false;
    }
    // Validation immédiate dans la base de données Oracle (COMMIT)
    QSqlQuery commitQuery;
    commitQuery.exec("COMMIT");
    QSqlDatabase::database().commit();
    return true;
}

bool Employe::existe(int id) {
    QSqlQuery query;
    query.prepare("SELECT ID_EMPLOYE FROM EMPLOYE WHERE ID_EMPLOYE = :id");
    query.bindValue(":id", id);
    if (query.exec() && query.next()) {
        return true;
    }
    return false;
}

QSqlQueryModel * Employe::rechercher(const QString &terme) {
    auto *model = new QSqlQueryModel();
    QSqlQuery query;
    query.prepare("SELECT ID_EMPLOYE, NOM, PRENOM, EMAIL, ROLE, DEPARTEMENT, EQUIPE, DISPONIBILITE FROM EMPLOYE "
                  "WHERE LOWER(NOM) LIKE :pattern OR LOWER(PRENOM) LIKE :pattern "
                  "OR LOWER(EMAIL) LIKE :pattern OR LOWER(ROLE) LIKE :pattern "
                  "OR LOWER(DEPARTEMENT) LIKE :pattern OR TO_CHAR(ID_EMPLOYE) LIKE :pattern "
                  "ORDER BY ID_EMPLOYE ASC");
    query.bindValue(":pattern", "%" + terme.trimmed().toLower() + "%");
    query.exec();
    model->setQuery(std::move(query));

    model->setHeaderData(0, Qt::Horizontal, QObject::tr("ID"));
    model->setHeaderData(1, Qt::Horizontal, QObject::tr("Nom"));
    model->setHeaderData(2, Qt::Horizontal, QObject::tr("Prénom"));
    model->setHeaderData(3, Qt::Horizontal, QObject::tr("E-mail"));
    model->setHeaderData(4, Qt::Horizontal, QObject::tr("Rôle"));
    model->setHeaderData(5, Qt::Horizontal, QObject::tr("Département"));
    model->setHeaderData(6, Qt::Horizontal, QObject::tr("Équipe(s)"));
    model->setHeaderData(7, Qt::Horizontal, QObject::tr("Disponibilité"));

    return model;
}

QSqlQueryModel * Employe::trier(const QString &critere, bool asc) {
    auto *model = new QSqlQueryModel();
    QString col = "ID_EMPLOYE";
    if (critere == "Nom") col = "NOM";
    else if (critere == "Prénom") col = "PRENOM";
    else if (critere == "Rôle") col = "ROLE";
    else if (critere == "Département") col = "DEPARTEMENT";

    QString sql = QString("SELECT ID_EMPLOYE, NOM, PRENOM, EMAIL, ROLE, DEPARTEMENT, EQUIPE, DISPONIBILITE FROM EMPLOYE ORDER BY %1 %2")
                      .arg(col, asc ? "ASC" : "DESC");
    model->setQuery(sql);

    model->setHeaderData(0, Qt::Horizontal, QObject::tr("ID"));
    model->setHeaderData(1, Qt::Horizontal, QObject::tr("Nom"));
    model->setHeaderData(2, Qt::Horizontal, QObject::tr("Prénom"));
    model->setHeaderData(3, Qt::Horizontal, QObject::tr("E-mail"));
    model->setHeaderData(4, Qt::Horizontal, QObject::tr("Rôle"));
    model->setHeaderData(5, Qt::Horizontal, QObject::tr("Département"));
    model->setHeaderData(6, Qt::Horizontal, QObject::tr("Équipe(s)"));
    model->setHeaderData(7, Qt::Horizontal, QObject::tr("Disponibilité"));

    return model;
}

// ===========================================================================
//  PARTIE 2 : AUTHENTIFICATION & PRIVILÈGES (AUTHMANAGER)
// ===========================================================================
namespace {
const int kPbkdf2Iterations = 100000;
const int kMaxFailures = 5;
const int kLockDurationSecs = 60;
}

AuthManager &AuthManager::instance()
{
    static AuthManager inst;
    return inst;
}

AuthManager::AuthManager()
{
    load();
    if (m_users.isEmpty()) seedDefaults();
}

QString AuthManager::dataDir()
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (base.isEmpty()) base = QDir::homePath() + "/.hackflow";
    QDir().mkpath(base);
    return base;
}

QStringList AuthManager::roles()
{
    return { "Administrateur", "Organisateur", "Technicien", "Mentor", "Visiteur", "USER" };
}

Privileges AuthManager::privilegesForRole(const QString &role)
{
    const Privileges views = ViewAccueil | ViewParticipants | ViewEquipes | ViewProjets
                           | ViewEmployes | ViewJury;
    // Tous les roles ont acces complet aux operations CRUD du module Employe
    const Privileges empCrud = EmployeAdd | EmployeEdit | EmployeDelete | EmployeExport | EmployeAssign;

    if (role == "Administrateur" || role == "USER")
        return views | ViewParametres | empCrud | ManageUsers;
    if (role == "Organisateur")
        return views | empCrud;
    if (role == "Technicien")
        return ViewAccueil | ViewEquipes | ViewProjets | ViewEmployes | empCrud;
    if (role == "Mentor")
        return ViewAccueil | ViewParticipants | ViewEquipes | ViewProjets | ViewJury | empCrud;
    if (role == "Visiteur")
        return views | empCrud;
    return views | empCrud;
}

QString AuthManager::privilegeLabel(Privilege p)
{
    switch (p) {
    case ViewAccueil:      return "Consulter l'accueil";
    case ViewParticipants: return "Consulter les participants";
    case ViewEquipes:      return "Consulter les équipes";
    case ViewProjets:      return "Consulter les projets";
    case ViewEmployes:     return "Consulter les employés";
    case ViewJury:         return "Consulter le jury";
    case ViewParametres:   return "Accéder aux paramètres";
    case EmployeAdd:       return "Ajouter un employé";
    case EmployeEdit:      return "Modifier un employé";
    case EmployeDelete:    return "Supprimer un employé";
    case EmployeExport:    return "Exporter les employés";
    case EmployeAssign:    return "Affecter à une équipe (CV)";
    case ManageUsers:      return "Gérer les utilisateurs et l'e-mail";
    default:               return QString();
    }
}

QString AuthManager::makeSalt()
{
    QByteArray s(16, 0);
    for (int i = 0; i < s.size(); i += 4) {
        const quint32 v = QRandomGenerator::system()->generate();
        memcpy(s.data() + i, &v, 4);
    }
    return QString::fromLatin1(s.toHex());
}

QString AuthManager::hashPassword(const QString &pw, const QString &saltHex)
{
    const QByteArray h = QPasswordDigestor::deriveKeyPbkdf2(
        QCryptographicHash::Sha256, pw.toUtf8(), QByteArray::fromHex(saltHex.toLatin1()),
        kPbkdf2Iterations, 32);
    return QString::fromLatin1(h.toHex());
}

QString AuthManager::passwordPolicyError(const QString &pw)
{
    if (pw.size() < 8)
        return "Le mot de passe doit contenir au moins 8 caractères.";
    bool letter = false, digit = false;
    for (const QChar c : pw) { letter |= c.isLetter(); digit |= c.isDigit(); }
    if (!letter || !digit)
        return "Le mot de passe doit contenir au moins une lettre et un chiffre.";
    return QString();
}

bool AuthManager::isValidEmail(const QString &email)
{
    static const QRegularExpression re(
        QStringLiteral("^[A-Za-z0-9._%+\\-]+@[A-Za-z0-9\\-]+(\\.[A-Za-z0-9\\-]+)*\\.[A-Za-z]{2,}$"));
    return email.size() <= 254 && re.match(email).hasMatch();
}

void AuthManager::load()
{
    QFile f(dataDir() + "/users.json");
    if (f.open(QIODevice::ReadOnly)) {
        const QJsonArray arr = QJsonDocument::fromJson(f.readAll()).array();
        for (const QJsonValue &v : arr) {
            const QJsonObject o = v.toObject();
            UserAccount u{ o["email"].toString().toLower(), o["name"].toString(), o["role"].toString(),
                           o["salt"].toString(), o["hash"].toString() };
            if (!u.email.isEmpty() && !u.hashHex.isEmpty()) m_users << u;
        }
    }
}

bool AuthManager::save() const
{
    QJsonArray arr;
    for (const UserAccount &u : m_users)
        arr << QJsonObject{ { "email", u.email }, { "name", u.name }, { "role", u.role },
                            { "salt", u.saltHex }, { "hash", u.hashHex } };
    QFile f(dataDir() + "/users.json");
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    f.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
    f.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    return true;
}

void AuthManager::seedDefaults()
{
    struct S { const char *mail, *name, *role, *pw; };
    const S defs[] = {
        { "admin@hackflow.com",        "Admin",        "Administrateur", "Admin@123" },
        { "organisateur@hackflow.com", "Organisateur", "Organisateur",   "Orga@1234" },
        { "technicien@hackflow.com",   "Technicien",   "Technicien",     "Tech@1234" },
        { "mentor@hackflow.com",       "Mentor",       "Mentor",         "Mentor@123" },
        { "dhaouafihanin@gmail.com",   "Hanin",        "Administrateur", "71n1n3@4h" },
        { "hanindh03@gmail.com",       "Hanin DH",     "Administrateur", "Admin@123" },
        { "tassnimeyounsi@gmail.com",  "Tassnime",     "Administrateur", "Admin@123" },
        { "nannou.12.slimi@gmail.com", "Nour",         "Administrateur", "Admin@123" },
        { "ineschkambou@gmail.com",    "Ines",         "Administrateur", "Admin@123" }
    };
    for (const S &d : defs) {
        if (indexOf(d.mail) < 0) {
            UserAccount u{ d.mail, d.name, d.role, makeSalt(), QString() };
            u.hashHex = hashPassword(d.pw, u.saltHex);
            m_users << u;
        }
    }
    save();
}

int AuthManager::indexOf(const QString &email) const
{
    const QString e = email.trimmed().toLower();
    for (int i = 0; i < m_users.size(); ++i)
        if (m_users[i].email.compare(e, Qt::CaseInsensitive) == 0) return i;
    return -1;
}

bool AuthManager::userExists(const QString &email) const
{
    return indexOf(email) >= 0;
}

QString AuthManager::userName(const QString &email) const
{
    const int i = indexOf(email);
    return i >= 0 ? m_users[i].name : QString();
}

bool AuthManager::login(const QString &email, const QString &password, QString *error)
{
    auto fail = [&](const QString &m) { if (error) *error = m; return false; };

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (m_lockUntilMs > now) {
        const int left = int((m_lockUntilMs - now + 999) / 1000);
        return fail(QString("Trop de tentatives infructueuses. Réessayez dans %1 s.").arg(left));
    }

    int i = indexOf(email);
    if (i < 0 && !email.contains('@')) {
        int match = -1;
        for (int k = 0; k < m_users.size(); ++k) {
            const QString uPart = m_users[k].email.section('@', 0, 0);
            if (uPart.compare(email.trimmed(), Qt::CaseInsensitive) == 0) {
                if (match >= 0) { match = -1; break; }
                match = k;
            }
        }
        i = match;
    }

    if (i < 0) {
        ++m_failures;
        if (m_failures >= kMaxFailures) m_lockUntilMs = now + kLockDurationSecs * 1000;
        return fail("Identifiant ou mot de passe incorrect.");
    }

    const UserAccount &u = m_users[i];
    const QString hashed = hashPassword(password, u.saltHex);
    if (hashed != u.hashHex) {
        ++m_failures;
        if (m_failures >= kMaxFailures) m_lockUntilMs = now + kLockDurationSecs * 1000;
        return fail("Identifiant ou mot de passe incorrect.");
    }

    m_failures = 0;
    m_lockUntilMs = 0;
    m_current = i;
    return true;
}

void AuthManager::logout()
{
    m_current = -1;
}

const UserAccount *AuthManager::current() const
{
    return (m_current >= 0 && m_current < m_users.size()) ? &m_users[m_current] : nullptr;
}

Privileges AuthManager::currentPrivileges() const
{
    const auto *u = current();
    return u ? privilegesForRole(u->role) : PrivNone;
}

bool AuthManager::has(Privilege p) const
{
    return currentPrivileges().testFlag(p);
}

int AuthManager::adminCount() const
{
    int n = 0;
    for (const UserAccount &u : m_users) if (u.role == "Administrateur") ++n;
    return n;
}

bool AuthManager::addUser(const QString &email, const QString &name, const QString &role,
                          const QString &password, QString *error)
{
    auto fail = [&](const QString &m) { if (error) *error = m; return false; };
    const QString e = email.trimmed().toLower();
    if (!isValidEmail(e))             return fail("Adresse e-mail invalide.");
    if (indexOf(e) >= 0)              return fail("Un compte existe déjà avec cette adresse e-mail.");
    if (name.trimmed().isEmpty())     return fail("Le nom est obligatoire.");
    if (!roles().contains(role))      return fail("Rôle inconnu.");
    const QString pe = passwordPolicyError(password);
    if (!pe.isEmpty())                return fail(pe);

    UserAccount u{ e, name.trimmed(), role, makeSalt(), QString() };
    u.hashHex = hashPassword(password, u.saltHex);
    m_users << u;
    return save() ? true : fail("Impossible d'enregistrer les comptes.");
}

bool AuthManager::registerUser(const QString &email, const QString &name, const QString &password, QString *error)
{
    // Accès complet garanti pour les nouveaux comptes (Administrateur / rôle complet)
    return addUser(email, name, defaultRegistrationRole(), password, error);
}

bool AuthManager::removeUser(const QString &email, QString *error)
{
    auto fail = [&](const QString &m) { if (error) *error = m; return false; };
    const int i = indexOf(email);
    if (i < 0)                        return fail("Compte introuvable.");
    if (i == m_current)               return fail("Vous ne pouvez pas supprimer votre propre compte.");
    if (m_users[i].role == "Administrateur" && adminCount() <= 1)
                                      return fail("Il doit rester au moins un administrateur.");
    m_users.removeAt(i);
    if (m_current > i) --m_current;
    return save();
}

bool AuthManager::setRole(const QString &email, const QString &role, QString *error)
{
    auto fail = [&](const QString &m) { if (error) *error = m; return false; };
    const int i = indexOf(email);
    if (i < 0)                        return fail("Compte introuvable.");
    if (!roles().contains(role))      return fail("Rôle inconnu.");
    if (m_users[i].role == "Administrateur" && role != "Administrateur" && adminCount() <= 1)
                                      return fail("Il doit rester au moins un administrateur.");
    m_users[i].role = role;
    return save();
}

bool AuthManager::setPassword(const QString &email, const QString &newPassword, QString *error)
{
    auto fail = [&](const QString &m) { if (error) *error = m; return false; };
    const int i = indexOf(email);
    if (i < 0)                        return fail("Compte introuvable.");
    const QString pe = passwordPolicyError(newPassword);
    if (!pe.isEmpty())                return fail(pe);
    m_users[i].saltHex = makeSalt();
    m_users[i].hashHex = hashPassword(newPassword, m_users[i].saltHex);
    return save() ? true : fail("Impossible d'enregistrer le mot de passe.");
}

// ===========================================================================
//  PARTIE 3 : CLIENT MAILER SMTP (AVEC GMAIL PRÉCONFIGURÉ)
// ===========================================================================
QString MailConfig::filePath() { return AuthManager::dataDir() + "/mail.ini"; }

MailConfig MailConfig::load()
{
    MailConfig c; // valeurs par défaut Gmail préconfigurées
    c.host     = "smtp.gmail.com";
    c.port     = 587;
    c.security = "starttls";
    c.user     = "dhaouafihanin@gmail.com";
    c.password = "bpsesnnuccdlkist";
    c.from     = "dhaouafihanin@gmail.com";

    if (QFile::exists(filePath())) {
        QSettings s(filePath(), QSettings::IniFormat);
        QString h = s.value("smtp/host").toString();
        if (!h.isEmpty()) {
            c.host     = h;
            c.port     = s.value("smtp/port", 587).toInt();
            c.security = s.value("smtp/security", "starttls").toString().toLower();
            c.user     = s.value("smtp/user").toString();
            c.password = s.value("smtp/password").toString();
            c.from     = s.value("smtp/from").toString();
        }
    }
    return c;
}

bool MailConfig::save() const
{
    QSettings s(filePath(), QSettings::IniFormat);
    s.setValue("smtp/host", host.trimmed());
    s.setValue("smtp/port", port);
    s.setValue("smtp/security", security);
    s.setValue("smtp/user", user.trimmed());
    s.setValue("smtp/password", password);
    s.setValue("smtp/from", from.trimmed());
    s.sync();
    QFile::setPermissions(filePath(), QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    return s.status() == QSettings::NoError;
}

namespace {
QByteArray wrap76(const QByteArray &b64)
{
    QByteArray out;
    for (int i = 0; i < b64.size(); i += 76) out += b64.mid(i, 76) + "\r\n";
    return out;
}
QByteArray encodedWord(const QString &s)
{
    return "=?UTF-8?B?" + s.toUtf8().toBase64() + "?=";
}
QString cleanHeader(QString s)
{
    s.remove('\r'); s.remove('\n');
    return s.trimmed();
}

class SmtpSession
{
public:
    explicit SmtpSession(int timeoutMs) : m_timeout(timeoutMs) {}
    QSslSocket sock;
    QString error;

    bool readReply(int &code, QString &text)
    {
        code = 0; text.clear();
        for (;;) {
            while (!sock.canReadLine()) {
                if (!sock.waitForReadyRead(m_timeout)) {
                    error = "Le serveur SMTP ne répond pas (délai dépassé).";
                    return false;
                }
            }
            const QString line = QString::fromUtf8(sock.readLine()).trimmed();
            text += line + "\n";
            if (line.size() < 3) { error = "Réponse SMTP invalide."; return false; }
            code = line.left(3).toInt();
            if (line.size() == 3 || line.at(3) != '-') return true;
        }
    }
    bool expect(int want, QString *textOut = nullptr)
    {
        int code; QString text;
        if (!readReply(code, text)) return false;
        if (textOut) *textOut = text;
        if (code != want) {
            error = QString("Réponse inattendue du serveur (%1) : %2").arg(code).arg(text.trimmed());
            return false;
        }
        return true;
    }
    bool command(const QByteArray &line, int want, QString *textOut = nullptr)
    {
        sock.write(line + "\r\n");
        if (!sock.waitForBytesWritten(m_timeout)) { error = "Échec d'envoi vers le serveur SMTP."; return false; }
        return expect(want, textOut);
    }
private:
    int m_timeout;
};
}

QByteArray Mailer::buildMessage(const QString &from, const QString &to,
                                const QString &subject, const QString &body)
{
    const QString f = cleanHeader(from), t = cleanHeader(to);
    QByteArray m;
    m += "From: HackFlow <" + f.toUtf8() + ">\r\n";
    m += "To: <" + t.toUtf8() + ">\r\n";
    m += "Subject: " + encodedWord(cleanHeader(subject)) + "\r\n";
    m += "Date: " + QLocale::c().toString(QDateTime::currentDateTimeUtc(),
                                          "ddd, dd MMM yyyy HH:mm:ss").toLatin1() + " +0000\r\n";
    m += "Message-ID: <" + QByteArray::number(QDateTime::currentMSecsSinceEpoch()) + "."
         + QByteArray::number(QRandomGenerator::global()->generate()) + "@hackflow.local>\r\n";
    m += "MIME-Version: 1.0\r\n";
    m += "Content-Type: text/plain; charset=UTF-8\r\n";
    m += "Content-Transfer-Encoding: base64\r\n\r\n";
    m += wrap76(body.toUtf8().toBase64());
    return m;
}

MailResult Mailer::send(const MailConfig &cfg, const QString &to,
                        const QString &subject, const QString &body)
{
    MailResult r;
    const QString from = cfg.effectiveFrom();
    const QByteArray msg = buildMessage(from.isEmpty() ? "noreply@hackflow.local" : from, to, subject, body);

    SmtpSession s(15000);
    auto fail = [&](const QString &why) {
        r.ok = false; r.message = why.isEmpty() ? s.error : why;
        s.sock.abort();
        return r;
    };

    const bool implicitSsl = (cfg.security == "ssl");
    if (implicitSsl) {
        s.sock.connectToHostEncrypted(cfg.host, quint16(cfg.port));
        if (!s.sock.waitForEncrypted(15000))
            return fail("Connexion SSL impossible : " + s.sock.errorString());
    } else {
        s.sock.connectToHost(cfg.host, quint16(cfg.port));
        if (!s.sock.waitForConnected(15000))
            return fail("Connexion au serveur SMTP impossible : " + s.sock.errorString());
    }

    if (!s.expect(220)) return fail({});
    if (!s.command("EHLO hackflow.local", 250)) return fail({});

    if (cfg.security == "starttls") {
        if (!s.command("STARTTLS", 220)) return fail({});
        s.sock.startClientEncryption();
        if (!s.sock.waitForEncrypted(15000))
            return fail("Négociation TLS impossible : " + s.sock.errorString());
        if (!s.command("EHLO hackflow.local", 250)) return fail({});
    }

    if (!cfg.user.isEmpty()) {
        QString pass = cfg.password.trimmed();
        pass.remove(' ');
        if (!s.command("AUTH LOGIN", 334)) return fail({});
        if (!s.command(cfg.user.trimmed().toUtf8().toBase64(), 334)) return fail({});
        if (!s.command(pass.toUtf8().toBase64(), 235))
            return fail("Authentification SMTP refusée (vérifiez l'utilisateur / le mot de passe d'application).");
    }

    if (!s.command("MAIL FROM:<" + from.toUtf8() + ">", 250)) return fail({});
    if (!s.command("RCPT TO:<" + to.toUtf8() + ">", 250)) return fail({});
    if (!s.command("DATA", 354)) return fail({});
    s.sock.write(msg + ".\r\n");
    if (!s.sock.waitForBytesWritten(15000)) return fail("Échec d'envoi du message.");
    if (!s.expect(250)) return fail({});
    s.sock.write("QUIT\r\n");
    s.sock.waitForBytesWritten(2000);
    s.sock.disconnectFromHost();

    r.ok = true;
    r.message = "E-mail envoyé avec succès.";
    return r;
}

// ===========================================================================
//  PARTIE 4 : ANALYSE DES CV (CVANALYZER)
// ===========================================================================
namespace {
struct Kw { const char *word; double w; };
struct Cat { const char *name; QList<Kw> kws; };

const QList<Cat> &categories()
{
    static const QList<Cat> c = {
      { "Web", {
        {"html",1},{"css",1},{"javascript",1},{"typescript",1},{"react",1},{"angular",1},{"vue",1},{"vue.js",1},
        {"node.js",1},{"nodejs",1},{"express",.5},{"php",1},{"laravel",1},{"symfony",1},{"django",1},{"flask",1},
        {"spring boot",1},{"wordpress",1},{"bootstrap",1},{"tailwind",1},{"next.js",1},{"rest api",1},{"api rest",1},
        {"graphql",1},{"frontend",1},{"front-end",1},{"front end",1},{"backend",1},{"back-end",1},{"back end",1},
        {"full stack",1},{"fullstack",1},{"full-stack",1},{"developpement web",1},{"web",.5},{"jquery",1},{"ui/ux",.5},{"figma",.5} } },
      { "Mobile", {
        {"android",1},{"ios",1},{"flutter",1},{"dart",1},{"kotlin",1},{"swift",1},{"swiftui",1},{"react native",1},
        {"xamarin",1},{"ionic",1},{"jetpack compose",1},{"application mobile",1},{"applications mobiles",1},
        {"developpement mobile",1},{"mobile",.5},{"firebase",.5},{"android studio",1},{"xcode",1} } },
      { "IA & Data", {
        {"python",.7},{"machine learning",1},{"deep learning",1},{"apprentissage automatique",1},{"tensorflow",1},
        {"pytorch",1},{"keras",1},{"scikit-learn",1},{"sklearn",1},{"nlp",1},{"traitement du langage",1},
        {"computer vision",1},{"vision par ordinateur",1},{"opencv",1},{"data science",1},{"data scientist",1},
        {"data analyst",1},{"data engineer",1},{"pandas",1},{"numpy",1},{"big data",1},{"spark",.7},{"hadoop",1},
        {"power bi",1},{"tableau",.5},{"statistiques",.5},{"intelligence artificielle",1},{"ia",.7},{"ai",.7},
        {"llm",1},{"sql",.5},{"mongodb",.5},{"etl",.7},{"data mining",1},{"reseaux de neurones",1},{"jupyter",1} } },
      { "IoT & Embarque", {
        {"arduino",1},{"raspberry pi",1},{"raspberry",1},{"esp32",1},{"esp8266",1},{"iot",1},{"internet des objets",1},
        {"embarque",1},{"systemes embarques",1},{"microcontroleur",1},{"microcontrolleur",1},{"stm32",1},{"mqtt",1},
        {"capteur",1},{"capteurs",1},{"electronique",1},{"fpga",1},{"vhdl",1},{"verilog",1},{"rtos",1},{"freertos",1},
        {"robotique",1},{"ros",.7},{"lora",1},{"zigbee",1},{"automate",.5},{"plc",1},{"pcb",1},{"embedded",1} } },
      { "Cybersecurite & Reseaux", {
        {"cybersecurite",1},{"securite informatique",1},{"pentest",1},{"penetration testing",1},{"ethical hacking",1},
        {"kali linux",1},{"owasp",1},{"cryptographie",1},{"firewall",1},{"pare-feu",1},{"siem",1},{"soc",.7},
        {"reseau",.7},{"reseaux",.7},{"cisco",1},{"ccna",1},{"ccnp",1},{"vpn",1},{"tcp/ip",1},{"vlan",1},{"routage",1},
        {"wireshark",1},{"nmap",1},{"metasploit",1},{"iso 27001",1},{"forensic",1},{"ctf",1},{"securite",.5} } },
      { "DevOps & Cloud", {
        {"docker",1},{"kubernetes",1},{"k8s",1},{"jenkins",1},{"gitlab ci",1},{"github actions",1},{"ci/cd",1},
        {"terraform",1},{"ansible",1},{"aws",1},{"azure",1},{"gcp",1},{"google cloud",1},{"cloud",.7},{"linux",.7},
        {"devops",1},{"nginx",.7},{"git",.5},{"prometheus",1},{"grafana",1},{"administration systeme",1},{"bash",.5} } },
      { "Logiciel & Desktop", {
        {"c++",1},{"c#",1},{".net",1},{"qt",1},{"qt creator",1},{"qml",1},{"java",1},{"javafx",1},{"swing",.7},{"wpf",1},
        {"winforms",1},{"langage c",1},{"uml",.7},{"poo",.7},{"oop",.7},{"programmation orientee objet",1},
        {"design patterns",1},{"rust",1},{"golang",1},{"qmake",1},{"cmake",1},{"sqlite",.5},{"oracle",.5} } },
      { "Gestion & Mentorat", {
        {"gestion de projet",1},{"project management",1},{"scrum",1},{"agile",1},{"kanban",1},{"jira",.7},
        {"product owner",1},{"scrum master",1},{"mentorat",1},{"mentor",1},{"coaching",1},{"coach",.7},{"formation",.5},
        {"formateur",1},{"enseignement",1},{"enseignant",1},{"pedagogie",1},{"pedagogique",1},{"leadership",1},
        {"encadrement",1},{"animation",.7},{"communication",.5},{"pitch",.7},{"soft skills",.5},{"management",.7} } },
      { "Logistique & Organisation", {
        {"logistique",1},{"organisation d'evenements",1},{"evenementiel",1},{"evenement",.5},{"evenements",.5},
        {"planification",.7},{"planning",.5},{"coordination",.7},{"gestion de stock",1},{"approvisionnement",1},
        {"achats",.7},{"budget",.5},{"hospitalite",1},{"accueil",.5},{"protocole",.5},{"benevole",.7},{"benevolat",.7},
        {"gestion des equipements",1},{"materiel",.5},{"sponsoring",.7} } },
    };
    return c;
}

struct TeamProfile { const char *team, *label; QMap<QString, double> weights; };
const QList<TeamProfile> &teamProfiles()
{
    static const QList<TeamProfile> t = {
        { "Team Alpha", "Développement Web",         { {"Web",1.0}, {"DevOps & Cloud",0.35}, {"Logiciel & Desktop",0.2} } },
        { "Team Beta",  "IA & Data",                 { {"IA & Data",1.0}, {"Web",0.15}, {"Logiciel & Desktop",0.15} } },
        { "Team Gamma", "Applications mobiles",      { {"Mobile",1.0}, {"Web",0.3}, {"Logiciel & Desktop",0.2} } },
        { "Team Delta", "IoT & Systèmes embarqués",  { {"IoT & Embarque",1.0}, {"Logiciel & Desktop",0.3}, {"Cybersecurite & Reseaux",0.15} } },
        { "Team Omega", "Cybersécurité & Cloud",     { {"Cybersecurite & Reseaux",1.0}, {"DevOps & Cloud",0.6} } },
    };
    return t;
}

QString normalize(QString s)
{
    s.replace(QChar(0x2019), '\'').replace(QChar(0x00A0), ' ').replace(QChar(0x2011), '-');
    s = s.normalized(QString::NormalizationForm_D);
    QString out; out.reserve(s.size());
    for (const QChar c : s)
        if (c.category() != QChar::Mark_NonSpacing) out.append(c);
    return out.toLower();
}

QString extractDocx(const QString &path, QString *error)
{
    QZipReader zip(path);
    if (zip.status() != QZipReader::NoError) {
        if (error) *error = "Impossible d'ouvrir le fichier .docx (archive zip invalide).";
        return QString();
    }
    const QByteArray xml = zip.fileData("word/document.xml");
    if (xml.isEmpty()) {
        if (error) *error = "Fichier document.xml introuvable dans le .docx.";
        return QString();
    }
    QXmlStreamReader r(xml);
    QString text;
    while (!r.atEnd()) {
        r.readNext();
        if (r.isStartElement()) {
            if (r.name() == QLatin1String("t")) text += r.readElementText() + " ";
            else if (r.name() == QLatin1String("p") || r.name() == QLatin1String("br")) text += "\n";
        }
    }
    if (r.hasError() && error) *error = "Erreur de lecture du XML docx : " + r.errorString();
    return text;
}

#ifdef CV_PDF
QString extractPdf(const QString &path, QString *error)
{
    QPdfDocument doc;
    if (doc.load(path) != QPdfDocument::Error::None) {
        if (error) *error = "Impossible de charger le document PDF.";
        return QString();
    }
    QString text;
    for (int p = 0; p < doc.pageCount(); ++p) {
        const QPdfSelection sel = doc.getAllText(p);
        text += sel.text() + "\n";
    }
    return text;
}
#endif
}

bool CvAnalyzer::pdfSupported()
{
#ifdef CV_PDF
    return true;
#else
    return false;
#endif
}

QString CvAnalyzer::fileFilter()
{
    QString f = "Documents (*.docx *.txt *.md";
    if (pdfSupported()) f += " *.pdf";
    f += ");;Tous les fichiers (*.*)";
    return f;
}

QStringList CvAnalyzer::teamNames()
{
    QStringList l;
    for (const TeamProfile &tp : teamProfiles()) l << tp.team;
    return l;
}

QString CvAnalyzer::extractText(const QString &path, QString *error)
{
    const QFileInfo fi(path);
    const QString ext = fi.suffix().toLower();
    if (ext == "docx") return extractDocx(path, error);
#ifdef CV_PDF
    if (ext == "pdf")  return extractPdf(path, error);
#endif
    if (ext == "txt" || ext == "md" || ext == "csv") {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            if (error) *error = "Impossible d'ouvrir le fichier : " + f.errorString();
            return QString();
        }
        return QString::fromUtf8(f.readAll());
    }
    if (ext == "pdf") {
        if (error) *error = "La lecture directe des PDF n'est pas activée. Utilisez un format .docx ou .txt.";
        return QString();
    }
    if (error) *error = "Format de fichier non pris en charge (« ." + ext + " »).";
    return QString();
}

CvAnalysis CvAnalyzer::analyze(const QString &rawText,
                               const QMap<QString, int> &teamLoad,
                               const QStringList &currentTeams,
                               int maxTeamSize)
{
    CvAnalysis a;
    if (rawText.trimmed().isEmpty()) {
        a.error = "Le document est vide ou son texte n'a pas pu être extrait.";
        return a;
    }
    a.ok = true;

    static const QRegularExpression wordRe(R"(\b[\w'-]+\b)");
    auto wit = wordRe.globalMatch(rawText);
    while (wit.hasNext()) { wit.next(); ++a.words; }

    static const QRegularExpression mailRe(R"([A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,})");
    const auto mm = mailRe.match(rawText);
    if (mm.hasMatch()) a.cvEmail = mm.captured(0);

    const QString norm = " " + normalize(rawText) + " ";

    static const QRegularExpression expRe(R"((\d{1,2})\s*(?:\+|\ban[s]?\b|\bann[eé]e[s]?\b|\byears?\b))",
                                           QRegularExpression::CaseInsensitiveOption);
    auto eit = expRe.globalMatch(rawText);
    int maxExp = -1;
    while (eit.hasNext()) {
        const int val = eit.next().captured(1).toInt();
        if (val >= 1 && val <= 40) maxExp = qMax(maxExp, val);
    }
    a.years = maxExp;

    QMap<QString, double> catScores;
    for (const Cat &c : categories()) {
        double sum = 0.0;
        QStringList hits;
        for (const Kw &k : c.kws) {
            const QString needle = " " + QString::fromLatin1(k.word) + " ";
            int pos = 0, count = 0;
            while ((pos = norm.indexOf(needle, pos)) != -1) {
                ++count;
                pos += needle.size() - 1;
            }
            if (count > 0) {
                sum += k.w * (1.0 + 0.3 * qMin(count - 1, 3));
                hits << QString::fromLatin1(k.word);
            }
        }
        if (sum > 0.0) {
            const double sat = 1.0 - std::exp(-sum / 3.0);
            catScores[c.name] = sat;
            a.skills << SkillHit{ c.name, sat, hits };
        }
    }
    std::sort(a.skills.begin(), a.skills.end(),
              [](const SkillHit &x, const SkillHit &y) { return x.score > y.score; });

    for (const TeamProfile &tp : teamProfiles()) {
        TeamScore ts;
        ts.team  = tp.team;
        ts.label = tp.label;
        ts.load  = teamLoad.value(tp.team, 0);
        ts.full  = (maxTeamSize > 0 && ts.load >= maxTeamSize);
        ts.alreadyMember = currentTeams.contains(tp.team);

        double num = 0.0, den = 0.0;
        QStringList matchedCats;
        for (auto it = tp.weights.begin(); it != tp.weights.end(); ++it) {
            den += it.value();
            const double s = catScores.value(it.key(), 0.0);
            num += s * it.value();
            if (s > 0.15) matchedCats << it.key();
        }
        ts.affinity = den > 0 ? (num / den) : 0.0;

        double loadPenalty = 0.0;
        if (maxTeamSize > 0) {
            const double ratio = double(ts.load) / double(maxTeamSize);
            loadPenalty = 0.25 * ratio;
        }
        ts.finalScore = qBound(0.0, ts.affinity - loadPenalty, 1.0);

        if (ts.alreadyMember) ts.reason = "Déjà membre de cette équipe.";
        else if (ts.full)     ts.reason = QString("Équipe complète (%1/%2 membres).").arg(ts.load).arg(maxTeamSize);
        else if (ts.affinity < 0.05) ts.reason = "Compétences peu adaptées aux besoins de l'équipe.";
        else ts.reason = QString("Compétences alignées (%1) - effectif actuel : %2.")
                         .arg(matchedCats.join(", "), QString::number(ts.load));

        a.teams << ts;
    }

    std::sort(a.teams.begin(), a.teams.end(), [](const TeamScore &x, const TeamScore &y) {
        if (x.eligible() != y.eligible()) return x.eligible();
        return x.finalScore > y.finalScore;
    });

    const double sGestion = catScores.value("Gestion & Mentorat", 0.0);
    const double sLog     = catScores.value("Logistique & Organisation", 0.0);
    const double sDev     = std::max({ catScores.value("Web", 0.0),
                                   catScores.value("Mobile", 0.0),
                                   catScores.value("Logiciel & Desktop", 0.0),
                                   catScores.value("IA & Data", 0.0) });
    const double sTech    = std::max({ catScores.value("DevOps & Cloud", 0.0),
                                   catScores.value("IoT & Embarque", 0.0),
                                   catScores.value("Cybersecurite & Reseaux", 0.0) });

    if (sGestion >= 0.35 && sGestion >= sDev && (a.years >= 3 || sGestion > 0.6)) {
        a.suggestedRole = "Mentor";
        a.suggestedDept = "Pédagogie/Mentorat";
    } else if (sLog >= 0.35 && sLog >= sDev) {
        a.suggestedRole = "Organisateur";
        a.suggestedDept = "Logistique";
    } else if (sTech >= 0.35 && sTech > sDev) {
        a.suggestedRole = "Technicien";
        a.suggestedDept = "Technique";
    } else {
        a.suggestedRole = "Technicien";
        a.suggestedDept = "Technique";
    }

    return a;
}
