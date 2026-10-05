#include "createaccountdialog.h"
#include "authmanager.h"
#include "dialogstyle.h"

#include <QCheckBox>
#include <QCryptographicHash>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRandomGenerator>
#include <QRegularExpressionValidator>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

namespace {
QByteArray hashCode(const QString &code, const QByteArray &salt)
{
    return QCryptographicHash::hash(salt + code.toUtf8(), QCryptographicHash::Sha256);
}
}

CreateAccountDialog::CreateAccountDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle("Créer un compte");
    setModal(true);
    setMinimumWidth(470);
    setStyleSheet(hackflowDialogStyle());
    buildUi();
    connect(&m_watcher, &QFutureWatcher<MailResult>::finished, this, &CreateAccountDialog::onMailFinished);
}

void CreateAccountDialog::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(26, 24, 26, 20);
    m_stack = new QStackedWidget; root->addWidget(m_stack);

    // ------------------------------------------------ etape 1 : formulaire
    auto *p1 = new QWidget; auto *l1 = new QVBoxLayout(p1);
    l1->setContentsMargins(0, 0, 0, 0); l1->setSpacing(10);
    auto *t1 = new QLabel("Créer un compte"); t1->setObjectName("dlgTitle");
    auto *s1 = new QLabel("Rejoignez HackFlow. Votre compte aura d'abord un accès limité (rôle « "
                          + AuthManager::defaultRegistrationRole()
                          + " ») ; un administrateur pourra ensuite vous donner plus de droits.");
    s1->setObjectName("dlgSub"); s1->setWordWrap(true);
    m_name = new QLineEdit; m_name->setPlaceholderText("Nom complet");
    m_mail = new QLineEdit; m_mail->setPlaceholderText("Adresse e-mail");
    m_pw   = new QLineEdit; m_pw->setPlaceholderText("Mot de passe"); m_pw->setEchoMode(QLineEdit::Password);
    m_pw2  = new QLineEdit; m_pw2->setPlaceholderText("Confirmer le mot de passe"); m_pw2->setEchoMode(QLineEdit::Password);
    m_show = new QCheckBox("Afficher les mots de passe");
    auto *hint = new QLabel("Au moins 8 caractères, avec au moins une lettre et un chiffre.");
    hint->setObjectName("dlgSub"); hint->setWordWrap(true);
    m_err1 = new QLabel; m_err1->setObjectName("dlgError"); m_err1->setWordWrap(true); m_err1->hide();
    m_btnCreate = new QPushButton("Créer mon compte");
    auto *c1 = new QPushButton("Annuler"); c1->setObjectName("secondary");
    auto *b1 = new QHBoxLayout; b1->addStretch(); b1->addWidget(c1); b1->addWidget(m_btnCreate);
    l1->addWidget(t1); l1->addWidget(s1); l1->addSpacing(4);
    l1->addWidget(m_name); l1->addWidget(m_mail); l1->addWidget(m_pw); l1->addWidget(m_pw2);
    l1->addWidget(m_show); l1->addWidget(hint); l1->addWidget(m_err1); l1->addSpacing(4); l1->addLayout(b1);
    m_stack->addWidget(p1);

    // ------------------------------------------------ etape 2 : verification du code
    auto *p2 = new QWidget; auto *l2 = new QVBoxLayout(p2);
    l2->setContentsMargins(0, 0, 0, 0); l2->setSpacing(10);
    auto *t2 = new QLabel("Vérifiez votre e-mail"); t2->setObjectName("dlgTitle");
    m_info2 = new QLabel; m_info2->setObjectName("dlgInfo"); m_info2->setWordWrap(true);
    m_code = new QLineEdit; m_code->setPlaceholderText("Code à 6 chiffres"); m_code->setMaxLength(6);
    m_code->setValidator(new QRegularExpressionValidator(QRegularExpression("\\d{0,6}"), this));
    m_err2 = new QLabel; m_err2->setObjectName("dlgError"); m_err2->setWordWrap(true); m_err2->hide();
    m_btnVerify = new QPushButton("Valider et créer le compte");
    auto *back = new QPushButton("Retour"); back->setObjectName("secondary");
    auto *b2 = new QHBoxLayout; b2->addStretch(); b2->addWidget(back); b2->addWidget(m_btnVerify);
    l2->addWidget(t2); l2->addWidget(m_info2); l2->addWidget(m_code); l2->addWidget(m_err2);
    l2->addSpacing(4); l2->addLayout(b2);
    m_stack->addWidget(p2);

    connect(c1, &QPushButton::clicked, this, &QDialog::reject);
    connect(back, &QPushButton::clicked, this, [this]() { showStep(0); });
    connect(m_btnCreate, &QPushButton::clicked, this, &CreateAccountDialog::onCreate);
    connect(m_pw2, &QLineEdit::returnPressed, this, &CreateAccountDialog::onCreate);
    connect(m_btnVerify, &QPushButton::clicked, this, &CreateAccountDialog::onVerify);
    connect(m_code, &QLineEdit::returnPressed, this, &CreateAccountDialog::onVerify);
    connect(m_show, &QCheckBox::toggled, this, [this](bool on) {
        const auto mode = on ? QLineEdit::Normal : QLineEdit::Password;
        m_pw->setEchoMode(mode); m_pw2->setEchoMode(mode);
    });
    showStep(0);
}

void CreateAccountDialog::showStep(int i)
{
    m_stack->setCurrentIndex(i);
    for (int j = 0; j < m_stack->count(); ++j)
        m_stack->widget(j)->setSizePolicy(QSizePolicy::Preferred, j == i ? QSizePolicy::Preferred : QSizePolicy::Ignored);
    if (i == 0) { m_err1->hide(); m_name->setFocus(); } else { m_err2->hide(); m_code->clear(); m_code->setFocus(); }
    adjustSize();
}

// creation effective du compte
bool CreateAccountDialog::finishCreation()
{
    QString err;
    if (!AuthManager::instance().registerUser(m_email, m_nameVal, m_pwVal, &err)) {
        if (m_stack->currentIndex() == 0) { m_err1->setText(err); m_err1->show(); }
        else { m_err2->setText(err); m_err2->show(); }
        return false;
    }
    m_pwVal.clear();
    accept();
    return true;
}

void CreateAccountDialog::onCreate()
{
    if (m_watcher.isRunning()) return;
    auto fail = [this](const QString &m) { m_err1->setText(m); m_err1->show(); };
    m_err1->hide();

    const QString name = m_name->text().trimmed(), mail = m_mail->text().trimmed().toLower();
    if (name.size() < 2)                          return fail("Veuillez saisir votre nom.");
    if (!AuthManager::isValidEmail(mail))         return fail("Adresse e-mail invalide.");
    if (AuthManager::instance().userExists(mail)) return fail("Un compte existe déjà avec cet e-mail. Utilisez « Mot de passe oublié » si besoin.");
    if (m_pw->text() != m_pw2->text())            return fail("Les deux mots de passe ne sont pas identiques.");
    const QString pe = AuthManager::passwordPolicyError(m_pw->text());
    if (!pe.isEmpty())                            return fail(pe);

    m_email = mail; m_nameVal = name; m_pwVal = m_pw->text();

    const MailConfig cfg = MailConfig::load();
    if (!cfg.isConfigured()) {          // pas de SMTP : creation directe (verification impossible)
        m_verified = false;
        finishCreation();
        return;
    }

    // verification de l'adresse e-mail par code
    const quint32 code = QRandomGenerator::system()->bounded(100000u, 1000000u);
    const QString codeStr = QString::number(code);
    m_codeSalt = QByteArray::number(QRandomGenerator::system()->generate64(), 16);
    m_codeHash = hashCode(codeStr, m_codeSalt);
    m_expiry = QDateTime::currentDateTime().addSecs(600);
    m_attempts = 0;
    const QString body = QString(
        "Bonjour %1,\n\nBienvenue sur HackFlow !\n\n"
        "Votre code de vérification : %2\n\n"
        "Ce code est valable 10 minutes. Si vous n'avez pas demandé la création de ce compte, "
        "ignorez simplement ce message.\n\n— L'équipe HackFlow\n").arg(name, codeStr);
    m_btnCreate->setEnabled(false); m_btnCreate->setText("Envoi du code…");
    setCursor(Qt::BusyCursor);
    m_watcher.setFuture(QtConcurrent::run([cfg, mail, body]() {
        return Mailer::send(cfg, mail, "HackFlow - Vérifiez votre adresse e-mail", body);
    }));
}

void CreateAccountDialog::onMailFinished()
{
    m_btnCreate->setEnabled(true); m_btnCreate->setText("Créer mon compte");
    setCursor(Qt::ArrowCursor);
    const MailResult r = m_watcher.result();
    if (!r.ok) {
        m_codeHash.clear();
        m_err1->setText("Le code de vérification n'a pas pu être envoyé.\n" + r.message);
        m_err1->show();
        return;
    }
    m_info2->setText(QString("Un code de vérification vient d'être envoyé à %1. Saisissez-le pour "
                             "confirmer votre adresse (valable 10 minutes).").arg(m_email));
    showStep(1);
}

void CreateAccountDialog::onVerify()
{
    auto fail = [this](const QString &m) { m_err2->setText(m); m_err2->show(); };
    m_err2->hide();
    const QString code = m_code->text().trimmed();
    if (code.size() != 6)                           return fail("Saisissez le code à 6 chiffres reçu par e-mail.");
    if (m_codeHash.isEmpty() || QDateTime::currentDateTime() > m_expiry)
                                                    return fail("Code expiré. Cliquez sur « Retour » puis recommencez.");
    if (m_attempts >= 5) { m_codeHash.clear();      return fail("Trop de tentatives. Cliquez sur « Retour » puis recommencez."); }
    ++m_attempts;
    if (hashCode(code, m_codeSalt) != m_codeHash)
        return fail(QString("Code incorrect (%1 tentative(s) restante(s)).").arg(5 - m_attempts));
    m_verified = true;
    m_codeHash.clear();
    finishCreation();
}
