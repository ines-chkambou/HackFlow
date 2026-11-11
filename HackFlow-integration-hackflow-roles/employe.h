#ifndef EMPLOYE_H
#define EMPLOYE_H

#include <QString>
#include <QStringList>
#include <QList>
#include <QMap>
#include <QFlags>
#include <QObject>
#include <QSqlQuery>
#include <QSqlQueryModel>
#include <QDateTime>

// ===========================================================================
//  ENTITÉ EMPLOYÉ (Modèle Oracle : table EMPLOYE)
// ===========================================================================
class Employe {
private:
    int id = 0;
    QString nom;
    QString prenom;
    QString email;
    QString role;
    QString departement;
    QString equipe;
    QString disponibilite;
    QString lastError;

public:
    Employe();
    Employe(int id, const QString &nom, const QString &prenom, const QString &email,
            const QString &role, const QString &departement, const QString &equipe,
            const QString &disponibilite);

    // Getters
    int getId() const { return id; }
    QString getNom() const { return nom; }
    QString getPrenom() const { return prenom; }
    QString getEmail() const { return email; }
    QString getRole() const { return role; }
    QString getDepartement() const { return departement; }
    QString getEquipe() const { return equipe; }
    QString getDisponibilite() const { return disponibilite; }
    QString getLastError() const { return lastError; }

    // Setters
    void setId(int id) { this->id = id; }
    void setNom(const QString &nom) { this->nom = nom; }
    void setPrenom(const QString &prenom) { this->prenom = prenom; }
    void setEmail(const QString &email) { this->email = email; }
    void setRole(const QString &role) { this->role = role; }
    void setDepartement(const QString &departement) { this->departement = departement; }
    void setEquipe(const QString &equipe) { this->equipe = equipe; }
    void setDisponibilite(const QString &disponibilite) { this->disponibilite = disponibilite; }

    // CRUD Oracle (requêtes préparées)
    bool ajouter();
    QSqlQueryModel * afficher();
    bool supprimer(int id);
    bool modifier(int id);

    // Recherche, tri, existence
    static bool existe(int id);
    static bool emailExiste(const QString &email, int excludeId = -1);
    static int prochainId();
    static void reordonnerIds();
    static QSqlQueryModel * rechercher(const QString &terme);
    static QSqlQueryModel * trier(const QString &critere, bool asc = true);
};

// ===========================================================================
//  AUTHENTIFICATION, RBAC & GESTION DES PRIVILÈGES
// ===========================================================================
enum Privilege : quint32 {
    PrivNone          = 0,
    ViewAccueil       = 1u << 0,
    ViewParticipants  = 1u << 1,
    ViewEquipes       = 1u << 2,
    ViewProjets       = 1u << 3,
    ViewEmployes      = 1u << 4,
    ViewJury          = 1u << 5,
    ViewParametres    = 1u << 6,
    EmployeAdd        = 1u << 7,
    EmployeEdit       = 1u << 8,
    EmployeDelete     = 1u << 9,
    EmployeExport     = 1u << 10,
    EmployeAssign     = 1u << 11,   // attribution automatique (CV)
    ManageUsers       = 1u << 12    // comptes, rôles, configuration SMTP
};
Q_DECLARE_FLAGS(Privileges, Privilege)
Q_DECLARE_OPERATORS_FOR_FLAGS(Privileges)

struct UserAccount {
    QString email;
    QString name;
    QString role;        // Administrateur | Organisateur | Technicien | Mentor | Visiteur | USER
    QString saltHex;
    QString hashHex;
};

class AuthManager : public QObject
{
    Q_OBJECT
public:
    static AuthManager &instance();

    // Rôles & privilèges
    static QStringList roles();
    static Privileges privilegesForRole(const QString &role);
    static QString privilegeLabel(Privilege p);

    // Session
    bool login(const QString &email, const QString &password, QString *error);
    void logout();
    bool isLoggedIn() const { return m_current >= 0; }
    const UserAccount *current() const;
    bool has(Privilege p) const;
    Privileges currentPrivileges() const;

    // Comptes
    QList<UserAccount> users() const { return m_users; }
    bool userExists(const QString &email) const;
    QString userName(const QString &email) const;
    bool addUser(const QString &email, const QString &name, const QString &role,
                 const QString &password, QString *error);
    // Inscription libre : attribue par défaut les permissions complètes (Administrateur / Accès complet)
    bool registerUser(const QString &email, const QString &name, const QString &password, QString *error);
    static QString defaultRegistrationRole() { return "Administrateur"; }
    bool removeUser(const QString &email, QString *error);
    bool setRole(const QString &email, const QString &role, QString *error);
    bool setPassword(const QString &email, const QString &newPassword, QString *error);

    // Utilitaires
    static bool isValidEmail(const QString &email);
    static QString passwordPolicyError(const QString &pw);
    static QString dataDir();

private:
    AuthManager();
    int indexOf(const QString &email) const;
    void load();
    bool save() const;
    void seedDefaults();
    static QString makeSalt();
    static QString hashPassword(const QString &pw, const QString &saltHex);
    int adminCount() const;

    QList<UserAccount> m_users;
    int m_current = -1;
    int m_failures = 0;
    qint64 m_lockUntilMs = 0;
};

// ===========================================================================
//  ENVOI D'E-MAILS RÉEL (SMTP GMAIL PRECONFIGURÉ)
// ===========================================================================
struct MailConfig {
    QString host = "smtp.gmail.com";
    int     port = 587;
    QString security = "starttls";   // "ssl" (465) | "starttls" (587) | "none"
    QString user = "dhaouafihanin@gmail.com";
    QString password = "71n1n3@4h";
    QString from = "dhaouafihanin@gmail.com";

    bool isConfigured() const { return !host.trimmed().isEmpty() && !effectiveFrom().isEmpty(); }
    QString effectiveFrom() const { return from.trimmed().isEmpty() ? user.trimmed() : from.trimmed(); }

    static QString filePath();
    static MailConfig load();
    bool save() const;
};

struct MailResult {
    bool    ok = false;
    bool    savedToOutbox = false;
    QString message;
    QString path;
};

class Mailer
{
public:
    static MailResult send(const MailConfig &cfg, const QString &to,
                           const QString &subject, const QString &body);
    static QByteArray buildMessage(const QString &from, const QString &to,
                                   const QString &subject, const QString &body);
};

// ===========================================================================
//  ANALYSE ET ATTRIBUTION AUTOMATIQUE INTELLIGENTE DES CV
// ===========================================================================
struct SkillHit {
    QString     category;
    double      score = 0;
    QStringList keywords;
};

struct TeamScore {
    QString team;
    QString label;
    double  affinity = 0;
    double  finalScore = 0;
    int     load = 0;
    bool    full = false;
    bool    alreadyMember = false;
    QString reason;
    bool eligible() const { return !full && !alreadyMember; }
};

struct CvAnalysis {
    bool    ok = false;
    QString error;
    int     words = 0;
    int     years = -1;
    QString cvEmail;
    QList<SkillHit>  skills;
    QList<TeamScore> teams;
    QString suggestedRole;
    QString suggestedDept;
    bool hasSkills() const { return !skills.isEmpty(); }
    const TeamScore *best() const {
        return (!teams.isEmpty() && teams.first().eligible() && hasSkills()) ? &teams.first() : nullptr;
    }
};

class CvAnalyzer
{
public:
    static QString extractText(const QString &path, QString *error);
    static QStringList teamNames();
    static QString fileFilter();
    static bool pdfSupported();

    static CvAnalysis analyze(const QString &text,
                              const QMap<QString, int> &teamLoad,
                              const QStringList &currentTeams,
                              int maxTeamSize);
};

#endif // EMPLOYE_H
