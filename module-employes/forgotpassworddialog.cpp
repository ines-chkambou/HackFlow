#include "forgotpassworddialog.h"
#include "authmanager.h"
#include "dialogstyle.h"

#include <QCheckBox>
#include <QCryptographicHash>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRandomGenerator>
#include <QRegularExpressionValidator>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

namespace {
const int kCodeValidMinutes = 10;
const int kMaxAttempts      = 5;
const int kResendSeconds    = 30;

QByteArray hashCode(const QString &code, const QByteArray &salt)
{
    return QCryptographicHash::hash(salt + code.toUtf8(), QCryptographicHash::Sha256);
}
} // namespace

// ---------------------------------------------------------------------------
ForgotPasswordDialog::ForgotPasswordDialog(const QString &prefillEmail, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Mot de passe oublié");
    setModal(true);
    setMinimumWidth(460);
    setStyleSheet(hackflowDialogStyle());
    buildUi();
    m_lineEmail->setText(prefillEmail);
    connect(&m_watcher, &QFutureWatcher<MailResult>::finished, this, &ForgotPasswordDialog::onMailFinished);
    m_cooldownTimer = new QTimer(this);
    m_cooldownTimer->setInterval(500);
    connect(m_cooldownTimer, &QTimer::timeout, this, &ForgotPasswordDialog::updateResendButton);
}

void ForgotPasswordDialog::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(26, 24, 26, 20);
    m_stack = new QStackedWidget;
    root->addWidget(m_stack);

    // ------------------------------------------------ etape 1 : e-mail
    auto *p1 = new QWidget; auto *l1 = new QVBoxLayout(p1);
    l1->setContentsMargins(0, 0, 0, 0); l1->setSpacing(10);
    auto *t1 = new QLabel("Mot de passe oublié ?"); t1->setObjectName("dlgTitle");
    auto *s1 = new QLabel("Saisissez l'adresse e-mail de votre compte. Nous vous enverrons un code de "
                          "vérification à 6 chiffres.");
    s1->setObjectName("dlgSub"); s1->setWordWrap(true);
    m_lineEmail = new QLineEdit; m_lineEmail->setPlaceholderText("exemple@email.com");
    m_err1 = new QLabel; m_err1->setObjectName("dlgError"); m_err1->setWordWrap(true); m_err1->hide();
    m_btnSend = new QPushButton("Envoyer le code");
    auto *cancel1 = new QPushButton("Annuler"); cancel1->setObjectName("secondary");
    auto *b1 = new QHBoxLayout; b1->addStretch(); b1->addWidget(cancel1); b1->addWidget(m_btnSend);
    l1->addWidget(t1); l1->addWidget(s1); l1->addSpacing(6);
    l1->addWidget(m_lineEmail); l1->addWidget(m_err1); l1->addSpacing(6); l1->addLayout(b1);
    m_stack->addWidget(p1);

    // ------------------------------------------------ etape 2 : code + nouveau mot de passe
    auto *p2 = new QWidget; auto *l2 = new QVBoxLayout(p2);
    l2->setContentsMargins(0, 0, 0, 0); l2->setSpacing(10);
    auto *t2 = new QLabel("Réinitialiser le mot de passe"); t2->setObjectName("dlgTitle");
    m_info2 = new QLabel; m_info2->setObjectName("dlgInfo"); m_info2->setWordWrap(true);
    m_lineCode = new QLineEdit; m_lineCode->setPlaceholderText("Code à 6 chiffres");
    m_lineCode->setMaxLength(6);
    m_lineCode->setValidator(new QRegularExpressionValidator(QRegularExpression("\\d{0,6}"), this));
    m_linePw  = new QLineEdit; m_linePw->setEchoMode(QLineEdit::Password);
    m_linePw->setPlaceholderText("Nouveau mot de passe");
    m_linePw2 = new QLineEdit; m_linePw2->setEchoMode(QLineEdit::Password);
    m_linePw2->setPlaceholderText("Confirmer le mot de passe");
    m_showPw = new QCheckBox("Afficher les mots de passe");
    m_pwHint = new QLabel("Au moins 8 caractères, avec au moins une lettre et un chiffre.");
    m_pwHint->setObjectName("dlgSub"); m_pwHint->setWordWrap(true);
    m_err2 = new QLabel; m_err2->setObjectName("dlgError"); m_err2->setWordWrap(true); m_err2->hide();
    m_btnReset = new QPushButton("Modifier le mot de passe");
    m_btnResend = new QPushButton("Renvoyer le code"); m_btnResend->setObjectName("linkBtn");
    auto *cancel2 = new QPushButton("Annuler"); cancel2->setObjectName("secondary");
    auto *b2 = new QHBoxLayout; b2->addWidget(m_btnResend); b2->addStretch();
    b2->addWidget(cancel2); b2->addWidget(m_btnReset);
    l2->addWidget(t2); l2->addWidget(m_info2); l2->addWidget(m_lineCode);
    l2->addWidget(m_linePw); l2->addWidget(m_linePw2); l2->addWidget(m_showPw); l2->addWidget(m_pwHint);
    l2->addWidget(m_err2); l2->addSpacing(6); l2->addLayout(b2);
    m_stack->addWidget(p2);

    connect(cancel1, &QPushButton::clicked, this, &QDialog::reject);
    connect(cancel2, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_btnSend, &QPushButton::clicked, this, &ForgotPasswordDialog::onSendCode);
    connect(m_lineEmail, &QLineEdit::returnPressed, this, &ForgotPasswordDialog::onSendCode);
    connect(m_btnReset, &QPushButton::clicked, this, &ForgotPasswordDialog::onReset);
    connect(m_linePw2, &QLineEdit::returnPressed, this, &ForgotPasswordDialog::onReset);
    connect(m_btnResend, &QPushButton::clicked, this, [this]() { showStep(0); });
    connect(m_showPw, &QCheckBox::toggled, this, [this](bool on) {
        const auto mode = on ? QLineEdit::Normal : QLineEdit::Password;
        m_linePw->setEchoMode(mode); m_linePw2->setEchoMode(mode);
    });
    showStep(0);
}

void ForgotPasswordDialog::showStep(int i)
{
    m_stack->setCurrentIndex(i);
    for (int j = 0; j < m_stack->count(); ++j)      // la hauteur suit la page affichee
        m_stack->widget(j)->setSizePolicy(QSizePolicy::Preferred, j == i ? QSizePolicy::Preferred : QSizePolicy::Ignored);
    if (i == 0) { m_err1->hide(); m_lineEmail->setFocus(); }
    else        { m_err2->hide(); m_lineCode->setFocus(); }
    adjustSize();
}

void ForgotPasswordDialog::setBusy(bool b)
{
    m_btnSend->setEnabled(!b);
    m_lineEmail->setEnabled(!b);
    m_btnSend->setText(b ? "Envoi en cours…" : "Envoyer le code");
    setCursor(b ? Qt::BusyCursor : Qt::ArrowCursor);
}

// ---------------------------------------------------------------------------
//  Etape 1 : generation + envoi du code
// ---------------------------------------------------------------------------
void ForgotPasswordDialog::onSendCode()
{
    if (m_watcher.isRunning()) return;
    const QString email = m_lineEmail->text().trimmed().toLower();
    if (!AuthManager::isValidEmail(email)) {
        m_err1->setText("Veuillez saisir une adresse e-mail valide.");
        m_err1->show(); return;
    }
    if (QDateTime::currentDateTime() < m_nextSendAllowed && email == m_email) {
        m_err1->setText(QString("Veuillez patienter %1 s avant de redemander un code.")
                        .arg(QDateTime::currentDateTime().secsTo(m_nextSendAllowed) + 1));
        m_err1->show(); return;
    }
    m_err1->hide();
    m_email = email;
    m_attempts = 0;
    m_nextSendAllowed = QDateTime::currentDateTime().addSecs(kResendSeconds);
    m_pendingKnownUser = AuthManager::instance().userExists(email);

    // reponse identique que le compte existe ou non (ne revele pas quels comptes existent)
    if (!m_pendingKnownUser) {
        m_codeHash.clear();
        m_info2->setText(QString("Si un compte existe pour %1, un code de vérification vient d'être envoyé.").arg(email));
        showStep(1);
        return;
    }

    const quint32 code = QRandomGenerator::system()->bounded(100000u, 1000000u);
    const QString codeStr = QString::number(code);
    m_codeSalt = QByteArray::number(QRandomGenerator::system()->generate64(), 16);
    m_codeHash = hashCode(codeStr, m_codeSalt);
    m_codeExpiry = QDateTime::currentDateTime().addSecs(kCodeValidMinutes * 60);

    const QString name = AuthManager::instance().userName(email);
    const QString body = QString(
        "Bonjour %1,\n\n"
        "Vous avez demandé la réinitialisation de votre mot de passe HackFlow.\n\n"
        "Votre code de vérification : %2\n\n"
        "Ce code est valable %3 minutes. Si vous n'êtes pas à l'origine de cette demande, "
        "ignorez simplement ce message : votre mot de passe ne sera pas modifié.\n\n"
        "— L'équipe HackFlow\n").arg(name, codeStr).arg(kCodeValidMinutes);

    setBusy(true);
    const MailConfig cfg = MailConfig::load();
    m_watcher.setFuture(QtConcurrent::run([cfg, email, body]() {
        return Mailer::send(cfg, email, "HackFlow - Code de réinitialisation du mot de passe", body);
    }));
}

void ForgotPasswordDialog::onMailFinished()
{
    setBusy(false);
    const MailResult r = m_watcher.result();
    if (!r.ok) {
        m_codeHash.clear();
        m_err1->setText("L'e-mail n'a pas pu être envoyé.\n" + r.message);
        m_err1->show();
        return;
    }
    if (r.savedToOutbox)
        m_info2->setText(QString("Si un compte existe pour %1, un code de vérification vient d'être envoyé.\n\n%2")
                         .arg(m_email, r.message));
    else
        m_info2->setText(QString("Un code de vérification vient d'être envoyé à %1. "
                                 "Il est valable %2 minutes.").arg(m_email).arg(kCodeValidMinutes));
    m_cooldownTimer->start();
    updateResendButton();
    showStep(1);
}

void ForgotPasswordDialog::updateResendButton()
{
    const int left = int(QDateTime::currentDateTime().secsTo(m_nextSendAllowed));
    if (left > 0) {
        m_btnResend->setEnabled(false);
        m_btnResend->setText(QString("Renvoyer le code (%1 s)").arg(left));
    } else {
        m_btnResend->setEnabled(true);
        m_btnResend->setText("Renvoyer le code");
        m_cooldownTimer->stop();
    }
}

// ---------------------------------------------------------------------------
//  Etape 2 : verification du code + changement du mot de passe
// ---------------------------------------------------------------------------
void ForgotPasswordDialog::onReset()
{
    auto fail = [this](const QString &m) { m_err2->setText(m); m_err2->show(); };
    m_err2->hide();

    const QString code = m_lineCode->text().trimmed();
    if (code.size() != 6)                         return fail("Saisissez le code à 6 chiffres reçu par e-mail.");
    if (m_codeHash.isEmpty())                     return fail("Code invalide ou expiré. Demandez un nouveau code.");
    if (QDateTime::currentDateTime() > m_codeExpiry) {
        m_codeHash.clear();
        return fail("Ce code a expiré. Demandez un nouveau code.");
    }
    if (m_attempts >= kMaxAttempts) {
        m_codeHash.clear();
        return fail("Trop de tentatives. Demandez un nouveau code.");
    }
    if (m_linePw->text() != m_linePw2->text())    return fail("Les deux mots de passe ne sont pas identiques.");
    const QString pe = AuthManager::passwordPolicyError(m_linePw->text());
    if (!pe.isEmpty())                            return fail(pe);

    ++m_attempts;
    if (hashCode(code, m_codeSalt) != m_codeHash)
        return fail(QString("Code incorrect (%1 tentative(s) restante(s)).").arg(kMaxAttempts - m_attempts));

    QString err;
    if (!AuthManager::instance().setPassword(m_email, m_linePw->text(), &err))
        return fail(err);
    m_codeHash.clear();
    accept();
}
