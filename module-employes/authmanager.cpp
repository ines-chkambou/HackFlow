#include "authmanager.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPasswordDigestor>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QStandardPaths>

namespace {
const int kPbkdf2Iterations = 100000;
const int kMaxFailures      = 5;
const int kLockSeconds      = 60;

bool constantTimeEqual(const QByteArray &a, const QByteArray &b)
{
    if (a.size() != b.size()) return false;
    quint8 diff = 0;
    for (int i = 0; i < a.size(); ++i) diff |= quint8(a[i]) ^ quint8(b[i]);
    return diff == 0;
}
} // namespace

// ---------------------------------------------------------------------------
AuthManager &AuthManager::instance()
{
    static AuthManager inst;
    return inst;
}

AuthManager::AuthManager() { load(); }

QString AuthManager::dataDir()
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (base.isEmpty()) base = QDir::homePath() + "/.hackflow";
    QDir().mkpath(base);
    return base;
}

// ---------------------------------------------------------------------------
//  ROLES -> PRIVILEGES   (modifier ici pour changer la matrice des droits)
// ---------------------------------------------------------------------------
QStringList AuthManager::roles()
{
    return { "Administrateur", "Organisateur", "Technicien", "Mentor", "Visiteur" };
}

Privileges AuthManager::privilegesForRole(const QString &role)
{
    const Privileges views = ViewAccueil | ViewParticipants | ViewEquipes | ViewProjets
                           | ViewEmployes | ViewJury;
    if (role == "Administrateur")
        return views | ViewParametres | EmployeAdd | EmployeEdit | EmployeDelete
                     | EmployeExport | EmployeAssign | ManageUsers;
    if (role == "Organisateur")
        return views | EmployeAdd | EmployeEdit | EmployeExport | EmployeAssign;
    if (role == "Technicien")
        return ViewAccueil | ViewEquipes | ViewProjets | ViewEmployes | EmployeExport;
    if (role == "Mentor")
        return ViewAccueil | ViewParticipants | ViewEquipes | ViewProjets | ViewJury;
    if (role == "Visiteur")                       // compte cree par inscription : acces minimal
        return ViewAccueil;
    return PrivNone;
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

// ---------------------------------------------------------------------------
//  MOTS DE PASSE
// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
//  STOCKAGE
// ---------------------------------------------------------------------------
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
    if (m_users.isEmpty()) seedDefaults();
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

// comptes de demonstration (premier lancement uniquement)
void AuthManager::seedDefaults()
{
    struct S { const char *mail, *name, *role, *pw; };
    const S defs[] = {
        { "admin@hackflow.com",        "Admin",        "Administrateur", "Admin@123" },
        { "organisateur@hackflow.com", "Organisateur", "Organisateur",   "Orga@1234" },
        { "technicien@hackflow.com",   "Technicien",   "Technicien",     "Tech@1234" },
        { "mentor@hackflow.com",       "Mentor",       "Mentor",         "Mentor@123" },
    };
    for (const S &d : defs) {
        UserAccount u{ d.mail, d.name, d.role, makeSalt(), QString() };
        u.hashHex = hashPassword(d.pw, u.saltHex);
        m_users << u;
    }
    save();
}

// ---------------------------------------------------------------------------
//  SESSION
// ---------------------------------------------------------------------------
int AuthManager::indexOf(const QString &email) const
{
    const QString e = email.trimmed().toLower();
    for (int i = 0; i < m_users.size(); ++i)
        if (m_users[i].email == e) return i;
    return -1;
}

bool AuthManager::login(const QString &email, const QString &password, QString *error)
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now < m_lockUntilMs) {
        if (error) *error = QString("Trop de tentatives. Réessayez dans %1 s.")
                                .arg((m_lockUntilMs - now + 999) / 1000);
        return false;
    }
    if (email.trimmed().isEmpty() || password.isEmpty()) {
        if (error) *error = "Veuillez saisir votre e-mail et votre mot de passe.";
        return false;
    }
    int i = indexOf(email);
    if (i < 0 && !email.contains('@')) {          // "admin" -> admin@hackflow.com (si unique)
        int found = -1, n = 0;
        for (int k = 0; k < m_users.size(); ++k)
            if (m_users[k].email.section('@', 0, 0) == email.trimmed().toLower()) { found = k; ++n; }
        if (n == 1) i = found;
    }
    // on calcule toujours un hash (meme si le compte n'existe pas) : meme duree de reponse
    const QString salt = (i >= 0) ? m_users[i].saltHex : makeSalt();
    const QByteArray got = hashPassword(password, salt).toLatin1();
    const QByteArray want = (i >= 0) ? m_users[i].hashHex.toLatin1() : QByteArray("x");
    if (i < 0 || !constantTimeEqual(got, want)) {
        if (++m_failures >= kMaxFailures) {
            m_failures = 0;
            m_lockUntilMs = now + kLockSeconds * 1000;
            if (error) *error = QString("Trop de tentatives. Compte verrouillé %1 s.").arg(kLockSeconds);
        } else if (error) {
            *error = "E-mail ou mot de passe incorrect.";
        }
        return false;
    }
    m_failures = 0;
    m_current = i;
    return true;
}

void AuthManager::logout() { m_current = -1; }

const UserAccount *AuthManager::current() const
{
    return (m_current >= 0 && m_current < m_users.size()) ? &m_users[m_current] : nullptr;
}

Privileges AuthManager::currentPrivileges() const
{
    const UserAccount *u = current();
    return u ? privilegesForRole(u->role) : PrivNone;
}

bool AuthManager::has(Privilege p) const { return currentPrivileges().testFlag(p); }

// ---------------------------------------------------------------------------
//  GESTION DES COMPTES (reserve a ManageUsers par l'interface)
// ---------------------------------------------------------------------------
bool AuthManager::userExists(const QString &email) const { return indexOf(email) >= 0; }

QString AuthManager::userName(const QString &email) const
{
    const int i = indexOf(email);
    return i >= 0 ? m_users[i].name : QString();
}

int AuthManager::adminCount() const
{
    int n = 0;
    for (const UserAccount &u : m_users) n += (u.role == "Administrateur");
    return n;
}

bool AuthManager::addUser(const QString &email, const QString &name, const QString &role,
                          const QString &password, QString *error)
{
    const QString e = email.trimmed().toLower();
    auto fail = [&](const QString &m) { if (error) *error = m; return false; };
    if (!isValidEmail(e))            return fail("Adresse e-mail invalide.");
    if (indexOf(e) >= 0)             return fail("Un compte existe déjà avec cet e-mail.");
    if (name.trimmed().isEmpty())    return fail("Le nom est obligatoire.");
    if (!roles().contains(role))     return fail("Rôle inconnu.");
    const QString pe = passwordPolicyError(password);
    if (!pe.isEmpty())               return fail(pe);
    UserAccount u{ e, name.trimmed(), role, makeSalt(), QString() };
    u.hashHex = hashPassword(password, u.saltHex);
    m_users << u;
    return save() ? true : fail("Impossible d'enregistrer les comptes.");
}

bool AuthManager::registerUser(const QString &email, const QString &name, const QString &password, QString *error)
{
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
