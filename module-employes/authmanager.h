#ifndef AUTHMANAGER_H
#define AUTHMANAGER_H

#include <QObject>
#include <QString>
#include <QList>
#include <QFlags>

// ---------------------------------------------------------------------------
//  Authentification + gestion des privileges (RBAC : un role = un ensemble de privileges)
//  - comptes stockes dans  <AppData>/HackFlow/users.json
//  - mots de passe : PBKDF2-HMAC-SHA256 (100 000 iterations) + sel aleatoire, jamais en clair
//  - verrouillage temporaire apres 5 echecs de connexion
// ---------------------------------------------------------------------------
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
    ManageUsers       = 1u << 12    // comptes, roles, configuration SMTP
};
Q_DECLARE_FLAGS(Privileges, Privilege)
Q_DECLARE_OPERATORS_FOR_FLAGS(Privileges)

struct UserAccount {
    QString email;
    QString name;
    QString role;        // Administrateur | Organisateur | Technicien | Mentor
    QString saltHex;
    QString hashHex;
};

class AuthManager : public QObject
{
    Q_OBJECT
public:
    static AuthManager &instance();

    // --- roles / privileges
    static QStringList roles();
    static Privileges privilegesForRole(const QString &role);
    static QString privilegeLabel(Privilege p);

    // --- session
    bool login(const QString &email, const QString &password, QString *error);
    void logout();
    bool isLoggedIn() const { return m_current >= 0; }
    const UserAccount *current() const;
    bool has(Privilege p) const;
    Privileges currentPrivileges() const;

    // --- comptes
    QList<UserAccount> users() const { return m_users; }
    bool userExists(const QString &email) const;
    QString userName(const QString &email) const;
    bool addUser(const QString &email, const QString &name, const QString &role,
                 const QString &password, QString *error);
    // inscription libre ("Creer un compte") : le compte recoit le role le plus restreint (Visiteur),
    // un administrateur peut ensuite lui donner un autre role.
    bool registerUser(const QString &email, const QString &name, const QString &password, QString *error);
    static QString defaultRegistrationRole() { return "Visiteur"; }
    bool removeUser(const QString &email, QString *error);
    bool setRole(const QString &email, const QString &role, QString *error);
    bool setPassword(const QString &email, const QString &newPassword, QString *error);

    // --- utilitaires
    static bool isValidEmail(const QString &email);
    static QString passwordPolicyError(const QString &pw);   // vide si le mot de passe est valide
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

#endif // AUTHMANAGER_H
