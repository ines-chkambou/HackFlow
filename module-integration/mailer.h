#ifndef MAILER_H
#define MAILER_H

#include <QString>

// ---------------------------------------------------------------------------
//  Envoi d'e-mails (mot de passe oublie) - client SMTP minimal base sur QSslSocket.
//  Configuration : <AppData>/HackFlow/mail.ini  (modifiable via menu Admin > Configurer l'e-mail)
//  Si le SMTP n'est pas configure, le message est enregistre en .eml dans <AppData>/HackFlow/outbox
//  (mode hors-ligne : rien n'est envoye sur le reseau).
// ---------------------------------------------------------------------------
struct MailConfig {
    QString host;
    int     port = 587;
    QString security = "starttls";   // "ssl" (465) | "starttls" (587) | "none"
    QString user;
    QString password;
    QString from;                    // adresse expediteur (par defaut = user)

    bool isConfigured() const { return !host.trimmed().isEmpty() && !effectiveFrom().isEmpty(); }
    QString effectiveFrom() const { return from.trimmed().isEmpty() ? user.trimmed() : from.trimmed(); }

    static QString filePath();
    static MailConfig load();
    bool save() const;
};

struct MailResult {
    bool    ok = false;
    bool    savedToOutbox = false;   // true = mode hors-ligne (SMTP non configure)
    QString message;                 // message lisible par l'utilisateur
    QString path;                    // fichier .eml (mode hors-ligne)
};

class Mailer
{
public:
    // Bloquant (a appeler depuis un thread de travail : QtConcurrent::run)
    static MailResult send(const MailConfig &cfg, const QString &to,
                           const QString &subject, const QString &body);
    // Construit le message RFC 5322 complet (en-tetes + corps UTF-8 en base64)
    static QByteArray buildMessage(const QString &from, const QString &to,
                                   const QString &subject, const QString &body);
};

#endif // MAILER_H
