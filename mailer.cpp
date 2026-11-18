#include "mailer.h"
#include "authmanager.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QRandomGenerator>
#include <QSettings>
#include <QSslSocket>

// ---------------------------------------------------------------------------
QString MailConfig::filePath() { return AuthManager::dataDir() + "/mail.ini"; }

MailConfig MailConfig::load()
{
    QSettings s(filePath(), QSettings::IniFormat);
    MailConfig c;
    c.host     = s.value("smtp/host").toString();
    c.port     = s.value("smtp/port", 587).toInt();
    c.security = s.value("smtp/security", "starttls").toString().toLower();
    c.user     = s.value("smtp/user").toString();
    c.password = s.value("smtp/password").toString();
    c.from     = s.value("smtp/from").toString();
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

// ---------------------------------------------------------------------------
//  Message RFC 5322
// ---------------------------------------------------------------------------
namespace {
QByteArray wrap76(const QByteArray &b64)
{
    QByteArray out;
    for (int i = 0; i < b64.size(); i += 76) out += b64.mid(i, 76) + "\r\n";
    return out;
}
QByteArray encodedWord(const QString &s)   // =?UTF-8?B?...?=
{
    return "=?UTF-8?B?" + s.toUtf8().toBase64() + "?=";
}
QString cleanHeader(QString s)             // anti injection d'en-tetes
{
    s.remove('\r'); s.remove('\n');
    return s.trimmed();
}
} // namespace

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

// ---------------------------------------------------------------------------
//  Client SMTP minimal
// ---------------------------------------------------------------------------
namespace {
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
            if (line.size() == 3 || line.at(3) != '-') return true;     // derniere ligne
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
} // namespace

MailResult Mailer::send(const MailConfig &cfg, const QString &to,
                        const QString &subject, const QString &body)
{
    MailResult r;
    const QString from = cfg.effectiveFrom();
    const QByteArray msg = buildMessage(from.isEmpty() ? "noreply@hackflow.local" : from, to, subject, body);

    // ---- SMTP non configure : mode hors-ligne
    if (!cfg.isConfigured()) {
        const QString dir = AuthManager::dataDir() + "/outbox";
        QDir().mkpath(dir);
        const QString path = dir + "/mail_" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz") + ".eml";
        QFile f(path);
        if (f.open(QIODevice::WriteOnly)) { f.write(msg); f.close(); }
        r.ok = f.error() == QFileDevice::NoError;
        r.savedToOutbox = true;
        r.path = path;
        r.message = r.ok
            ? QString("Le serveur SMTP n'est pas configuré : le message n'a PAS été envoyé.\n"
                      "Il a été enregistré dans :\n%1").arg(QDir::toNativeSeparators(path))
            : "Impossible d'enregistrer le message.";
        return r;
    }

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
        if (!s.command("AUTH LOGIN", 334)) return fail({});
        if (!s.command(cfg.user.toUtf8().toBase64(), 334)) return fail({});
        if (!s.command(cfg.password.toUtf8().toBase64(), 235))
            return fail("Authentification SMTP refusée (vérifiez l'utilisateur / le mot de passe d'application).");
    }

    if (!s.command("MAIL FROM:<" + from.toUtf8() + ">", 250)) return fail({});
    if (!s.command("RCPT TO:<" + to.toUtf8() + ">", 250)) return fail({});
    if (!s.command("DATA", 354)) return fail({});
    // le corps est en base64 : aucune ligne ne peut commencer par "." (pas de dot-stuffing necessaire)
    s.sock.write(msg + ".\r\n");
    if (!s.sock.waitForBytesWritten(15000)) return fail("Échec d'envoi du message.");
    if (!s.expect(250)) return fail({});
    s.sock.write("QUIT\r\n");
    s.sock.waitForBytesWritten(2000);
    s.sock.disconnectFromHost();

    r.ok = true;
    r.message = "E-mail envoyé.";
    return r;
}
