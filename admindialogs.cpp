#include "admindialogs.h"
#include "authmanager.h"
#include "dialogstyle.h"

#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QStyle>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

// ===========================================================================
UserManagementDialog::UserManagementDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle("Gestion des utilisateurs et des privilèges");
    setStyleSheet(hackflowDialogStyle());
    resize(760, 560);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(22, 20, 22, 18); root->setSpacing(10);
    auto *title = new QLabel("Utilisateurs et privilèges"); title->setObjectName("dlgTitle");
    root->addWidget(title);

    m_table = new QTableWidget(0, 3);
    m_table->setHorizontalHeaderLabels({ "E-mail", "Nom", "Rôle" });
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->verticalHeader()->hide();
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setAlternatingRowColors(true);
    m_table->setShowGrid(false);
    m_table->setMinimumHeight(170);
    root->addWidget(m_table, 1);

    auto *form = new QFormLayout; form->setSpacing(8);
    m_email = new QLineEdit; m_email->setPlaceholderText("nouveau.compte@email.com");
    m_name  = new QLineEdit; m_name->setPlaceholderText("Nom affiché");
    m_pw    = new QLineEdit; m_pw->setPlaceholderText("Mot de passe initial (8+ car., lettre + chiffre)");
    m_pw->setEchoMode(QLineEdit::Password);
    m_role  = new QComboBox; m_role->addItems(AuthManager::roles());
    form->addRow("E-mail", m_email); form->addRow("Nom", m_name);
    form->addRow("Mot de passe", m_pw); form->addRow("Rôle", m_role);
    root->addLayout(form);

    m_privs = new QLabel; m_privs->setObjectName("dlgInfo"); m_privs->setWordWrap(true);
    root->addWidget(m_privs);
    m_msg = new QLabel; m_msg->setObjectName("dlgError"); m_msg->setWordWrap(true); m_msg->hide();
    root->addWidget(m_msg);

    auto *bAdd  = new QPushButton("Ajouter le compte");
    auto *bRole = new QPushButton("Appliquer le rôle au compte sélectionné"); bRole->setObjectName("secondary");
    auto *bPw   = new QPushButton("Réinitialiser le mot de passe"); bPw->setObjectName("secondary");
    auto *bDel  = new QPushButton("Supprimer"); bDel->setObjectName("danger");
    auto *bClose = new QPushButton("Fermer"); bClose->setObjectName("secondary");
    auto *row1 = new QHBoxLayout; row1->addWidget(bAdd); row1->addWidget(bRole); row1->addStretch();
    auto *row2 = new QHBoxLayout; row2->addWidget(bPw); row2->addWidget(bDel); row2->addStretch(); row2->addWidget(bClose);
    root->addLayout(row1); root->addLayout(row2);

    auto report = [this](bool ok, const QString &err) {
        if (ok) { m_msg->hide(); refresh(); }
        else { m_msg->setText(err); m_msg->show(); }
    };
    connect(bAdd, &QPushButton::clicked, this, [=]() {
        QString err;
        const bool ok = AuthManager::instance().addUser(m_email->text(), m_name->text(), m_role->currentText(), m_pw->text(), &err);
        if (ok) { m_email->clear(); m_name->clear(); m_pw->clear(); }
        report(ok, err);
    });
    connect(bRole, &QPushButton::clicked, this, [=]() {
        QString err; const QString e = selectedEmail();
        if (e.isEmpty()) return report(false, "Sélectionnez d'abord un compte dans la liste.");
        report(AuthManager::instance().setRole(e, m_role->currentText(), &err), err);
    });
    connect(bPw, &QPushButton::clicked, this, [=]() {
        const QString e = selectedEmail();
        if (e.isEmpty()) return report(false, "Sélectionnez d'abord un compte dans la liste.");
        bool okIn = false;
        const QString pw = QInputDialog::getText(this, "Nouveau mot de passe",
            "Nouveau mot de passe pour " + e + " :", QLineEdit::Password, QString(), &okIn);
        if (!okIn) return;
        QString err; report(AuthManager::instance().setPassword(e, pw, &err), err);
    });
    connect(bDel, &QPushButton::clicked, this, [=]() {
        const QString e = selectedEmail();
        if (e.isEmpty()) return report(false, "Sélectionnez d'abord un compte dans la liste.");
        if (QMessageBox::question(this, "Supprimer", "Supprimer le compte " + e + " ?") != QMessageBox::Yes) return;
        QString err; report(AuthManager::instance().removeUser(e, &err), err);
    });
    connect(bClose, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_role, &QComboBox::currentTextChanged, this, [this]() { updatePrivList(); });
    connect(m_table, &QTableWidget::itemSelectionChanged, this, [this]() {
        const int r = m_table->currentRow();
        if (r >= 0) { m_role->setCurrentText(m_table->item(r, 2)->text()); }
    });
    refresh();
    updatePrivList();
}

QString UserManagementDialog::selectedEmail() const
{
    const int r = m_table->currentRow();
    return r >= 0 ? m_table->item(r, 0)->text() : QString();
}

void UserManagementDialog::refresh()
{
    const QString keep = selectedEmail();
    const auto users = AuthManager::instance().users();
    m_table->setRowCount(users.size());
    for (int i = 0; i < users.size(); ++i) {
        m_table->setItem(i, 0, new QTableWidgetItem(users[i].email));
        m_table->setItem(i, 1, new QTableWidgetItem(users[i].name));
        m_table->setItem(i, 2, new QTableWidgetItem(users[i].role));
        if (users[i].email == keep) m_table->selectRow(i);
    }
}

void UserManagementDialog::updatePrivList()
{
    const Privileges p = AuthManager::privilegesForRole(m_role->currentText());
    QStringList items;
    for (quint32 bit = 1; bit <= ManageUsers; bit <<= 1)
        if (p.testFlag(Privilege(bit))) items << AuthManager::privilegeLabel(Privilege(bit));
    m_privs->setText("<b>Privilèges du rôle « " + m_role->currentText() + " » :</b><br>" + items.join("  ·  "));
}

// ===========================================================================
SmtpSettingsDialog::SmtpSettingsDialog(const QString &testRecipient, QWidget *parent)
    : QDialog(parent), m_testTo(testRecipient)
{
    setWindowTitle("Configuration de l'e-mail (SMTP)");
    setStyleSheet(hackflowDialogStyle());
    setMinimumWidth(480);
    const MailConfig c = MailConfig::load();

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(24, 22, 24, 18); root->setSpacing(10);
    auto *title = new QLabel("Serveur d'envoi (SMTP)"); title->setObjectName("dlgTitle");
    auto *sub = new QLabel("Utilisé pour envoyer les codes « Mot de passe oublié ». Avec Gmail / Outlook, "
                           "utilisez un « mot de passe d'application ». Sans configuration, les messages sont "
                           "seulement enregistrés dans le dossier « outbox ».");
    sub->setObjectName("dlgSub"); sub->setWordWrap(true);
    root->addWidget(title); root->addWidget(sub);

    m_host = new QLineEdit(c.host); m_host->setPlaceholderText("smtp.gmail.com");
    m_port = new QSpinBox; m_port->setRange(1, 65535); m_port->setValue(c.port);
    m_sec = new QComboBox; m_sec->addItems({ "starttls", "ssl", "none" });
    m_sec->setCurrentText(c.security);
    m_user = new QLineEdit(c.user); m_user->setPlaceholderText("compte@exemple.com");
    m_pass = new QLineEdit(c.password); m_pass->setEchoMode(QLineEdit::Password);
    m_from = new QLineEdit(c.from); m_from->setPlaceholderText("(identique à l'utilisateur si vide)");
    auto *form = new QFormLayout; form->setSpacing(8);
    form->addRow("Serveur", m_host); form->addRow("Port", m_port); form->addRow("Sécurité", m_sec);
    form->addRow("Utilisateur", m_user); form->addRow("Mot de passe", m_pass); form->addRow("Expéditeur", m_from);
    root->addLayout(form);

    connect(m_sec, &QComboBox::currentTextChanged, this, [this](const QString &s) {
        m_port->setValue(s == "ssl" ? 465 : s == "starttls" ? 587 : 25);
    });

    m_msg = new QLabel; m_msg->setWordWrap(true); m_msg->hide();
    root->addWidget(m_msg);

    m_btnTest = new QPushButton("Envoyer un e-mail de test"); m_btnTest->setObjectName("secondary");
    auto *bSave = new QPushButton("Enregistrer");
    auto *bClose = new QPushButton("Fermer"); bClose->setObjectName("secondary");
    auto *row = new QHBoxLayout; row->addWidget(m_btnTest); row->addStretch(); row->addWidget(bClose); row->addWidget(bSave);
    root->addLayout(row);

    auto show = [this](const QString &t, const char *obj) {
        m_msg->setObjectName(obj); m_msg->setText(t);
        m_msg->style()->unpolish(m_msg); m_msg->style()->polish(m_msg); m_msg->show();
    };
    connect(bSave, &QPushButton::clicked, this, [=]() {
        if (readForm().save()) show("Configuration enregistrée.", "dlgOk");
        else show("Impossible d'enregistrer la configuration.", "dlgError");
    });
    connect(bClose, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_btnTest, &QPushButton::clicked, this, [=]() {
        if (m_watcher.isRunning()) return;
        const MailConfig cfg = readForm();
        const QString to = m_testTo;
        m_btnTest->setEnabled(false); m_btnTest->setText("Envoi en cours…");
        m_watcher.setFuture(QtConcurrent::run([cfg, to]() {
            return Mailer::send(cfg, to, "HackFlow - E-mail de test",
                                "Ceci est un e-mail de test envoyé par HackFlow.\nLa configuration SMTP fonctionne.\n");
        }));
    });
    connect(&m_watcher, &QFutureWatcher<MailResult>::finished, this, [=]() {
        m_btnTest->setEnabled(true); m_btnTest->setText("Envoyer un e-mail de test");
        const MailResult r = m_watcher.result();
        if (r.ok && !r.savedToOutbox) show("E-mail de test envoyé à " + m_testTo + ".", "dlgOk");
        else if (r.ok)                show(r.message, "dlgInfo");
        else                          show(r.message, "dlgError");
    });
}

MailConfig SmtpSettingsDialog::readForm() const
{
    MailConfig c;
    c.host = m_host->text().trimmed(); c.port = m_port->value(); c.security = m_sec->currentText();
    c.user = m_user->text().trimmed(); c.password = m_pass->text(); c.from = m_from->text().trimmed();
    return c;
}
