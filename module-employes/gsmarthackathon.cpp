#include "gsmarthackathon.h"
#include "ui_gsmarthackathon.h"

#include <QLabel>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QPushButton>
#include <QHeaderView>
#include <QTimer>
#include <QList>
#include <QPair>
#include <QGuiApplication>
#include <QScreen>
#include <QAction>
#include <QMenu>
#include <QMessageBox>
#include <QSettings>
#include <QStyle>
#include <QGraphicsOpacityEffect>
#include <QToolButton>
#include <QLineEdit>
#include "authmanager.h"
#include "forgotpassworddialog.h"
#include "createaccountdialog.h"
#include "admindialogs.h"
#include "cvassigndialog.h"

// ---------------------------------------------------------------------------
GSmartHackathon::GSmartHackathon(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::GSmartHackathon)
{
    ui->setupUi(this);
    setupNavigation();
    setupDecor();
    setupAuth();
    showConnexion();            // l'application demarre sur l'ecran de connexion
}

GSmartHackathon::~GSmartHackathon() { delete ui; }

// ---------------------------------------------------------------------------
//  NAVIGATION : seule partie fonctionnelle de cette phase
// ---------------------------------------------------------------------------
void GSmartHackathon::setupNavigation()
{
    // Connexion -> application (verification du compte : voir doLogin) / deconnexion : voir doLogout
    connect(ui->btnLogin,  &QPushButton::clicked, this, [this]() { doLogin(); });
    connect(ui->btnLogout, &QPushButton::clicked, this, [this]() { doLogout(); });

    // bouton du menu -> index de la page dans stackedPages (meme ordre que le .ui)
    const QList<QPair<QPushButton *, int>> nav = {
        { ui->btnNavAccueil,      0 },   // Accueil / Tableau de bord
        { ui->btnNavParticipants, 1 },
        { ui->btnNavEquipes,      2 },
        { ui->btnNavProjets,      3 },
        { ui->btnNavEmployes,     5 },   // notre module
        { ui->btnNavJury,         7 },
        { ui->btnNavParametres,   8 }
    };
    for (const auto &p : nav) {
        const int index = p.second;
        connect(p.first, &QPushButton::clicked, this, [this, index]() {
            // controle des privileges : index de page -> privilege de consultation requis
            static const QMap<int, Privilege> need = {
                { 0, ViewAccueil }, { 1, ViewParticipants }, { 2, ViewEquipes }, { 3, ViewProjets },
                { 5, ViewEmployes }, { 7, ViewJury }, { 8, ViewParametres } };
            if (need.contains(index) && !AuthManager::instance().has(need.value(index))) {
                ui->stackedPages->setCurrentIndex(0);
                ui->btnNavAccueil->setChecked(true);
                return;
            }
            ui->stackedPages->setCurrentIndex(index);
        });
    }
}

static void centerOnScreen(QWidget *w)
{
    QScreen *s = w->screen() ? w->screen() : QGuiApplication::primaryScreen();
    const QRect a = s->availableGeometry();
    w->move(a.center() - QPoint(w->width() / 2, w->height() / 2));
}

void GSmartHackathon::showConnexion()
{
    ui->stackedMain->setCurrentIndex(0);
    setFixedSize(960, 640);                         // meme taille que la maquette de connexion
    centerOnScreen(this);
}

void GSmartHackathon::showApplication()
{
    ui->stackedMain->setCurrentIndex(1);
    ui->stackedPages->setCurrentIndex(0);           // Connexion -> Tableau de bord
    ui->btnNavAccueil->setChecked(true);
    setMinimumSize(0, 0);                           // le minimum est calcule par les layouts
    setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    const QRect a = (screen() ? screen() : QGuiApplication::primaryScreen())->availableGeometry();
    const QSize want(qMin(1520, a.width() - 40), qMin(900, a.height() - 60));
    resize(want.expandedTo(minimumSizeHint()));
    centerOnScreen(this);
}

// ---------------------------------------------------------------------------
//  DECOR : contenu d'exemple pour que l'interface ressemble a la maquette.
//  Aucun signal n'est connecte : boutons et cases sont purement visuels.
// ---------------------------------------------------------------------------
namespace {
struct Row { const char *id, *nom, *prenom, *mail, *role, *dept, *equipes, *dispo; };
const Row kRows[] = {
    {"1", "Ben Ali",  "Ahmed",   "ahmed.benali@email.com",  "Organisateur", "Pédagogie/Mentorat", "Team Alpha, Team Beta",            "Disponible"},
    {"2", "Trabelsi", "Sara",    "sara.trabelsi@email.com", "Technicien",   "Logistique",         "—",                                "Disponible"},
    {"3", "Khaled",   "Omar",    "omar.khaled@email.com",   "Technicien",   "Technique",          "—",                                "Indisponible"},
    {"4", "Ayari",    "Meriem",  "meriem.ayari@email.com",  "Mentor",       "Pédagogie/Mentorat", "Team Gamma",                       "Disponible"},
    {"5", "Jabri",    "Yassine", "yassine.jabri@email.com", "Technicien",   "Technique",          "Team Beta",                        "Disponible"},
    {"6", "Zribi",    "Ines",    "ines.zribi@email.com",    "Mentor",       "Technique",          "Team Delta, Team Alpha, Team Omega", "Disponible"},
    {"7", "Gharbi",   "Amine",   "amine.gharbi@email.com",  "Organisateur", "Logistique",         "—",                                "Indisponible"},
    {"8", "Mansouri", "Leila",   "leila.mansouri@email.com","Technicien",   "Logistique",         "—",                                "Disponible"},
};

QString roleBadge(const QString &r)
{
    if (r == "Organisateur")
        return "background:rgba(47,103,224,0.30); color:#8FB0FF; border:1px solid #2F67E0; border-radius:5px; padding:2px 6px; font-size:8pt; font-weight:bold;";
    if (r == "Technicien")
        return "background:rgba(11,248,246,0.13); color:#0BF8F6; border:1px solid #0BF8F6; border-radius:5px; padding:2px 6px; font-size:8pt; font-weight:bold;";
    return "background:rgba(124,77,255,0.28); color:#C2AEFF; border:1px solid #7C4DFF; border-radius:5px; padding:2px 6px; font-size:8pt; font-weight:bold;";
}
QWidget *centered(QWidget *inner)
{
    auto *w = new QWidget; auto *l = new QHBoxLayout(w);
    l->setContentsMargins(3, 6, 3, 6); l->setAlignment(Qt::AlignCenter); l->addWidget(inner);
    return w;
}
} // namespace

void GSmartHackathon::setupDecor()
{
    // liste d'equipes proposee dans la liste deroulante a cases a cocher
    ui->comboEquipes->setItems({ "Team Alpha", "Team Beta", "Team Gamma", "Team Delta", "Team Omega" });

    // ---- tableau
    auto *t = ui->tableEmployes;
    auto *h = t->horizontalHeader();
    h->setSectionResizeMode(QHeaderView::Interactive);
    h->setSectionResizeMode(7, QHeaderView::Stretch);
    h->setMinimumSectionSize(28);
    const int widths[] = { 32, 34, 80, 72, 175, 94, 118, 0, 96, 84 };
    for (int c = 0; c < 10; ++c) if (widths[c]) t->setColumnWidth(c, widths[c]);
    t->verticalHeader()->setDefaultSectionSize(46);
    t->setFocusPolicy(Qt::NoFocus);
    t->setWordWrap(true);

    const int n = int(sizeof(kRows) / sizeof(kRows[0]));
    t->setRowCount(n);
    for (int r = 0; r < n; ++r) {
        const Row &e = kRows[r];
        t->setItem(r, 0, new QTableWidgetItem(""));
        t->setItem(r, 1, new QTableWidgetItem(e.id));
        t->setItem(r, 2, new QTableWidgetItem(e.nom));
        t->setItem(r, 3, new QTableWidgetItem(e.prenom));
        t->setItem(r, 4, new QTableWidgetItem(e.mail));
        t->setItem(r, 6, new QTableWidgetItem(e.dept));
        t->setItem(r, 7, new QTableWidgetItem(e.equipes));

        auto *cw = new QWidget; auto *cl = new QHBoxLayout(cw);
        cl->setContentsMargins(0, 0, 0, 0); cl->setAlignment(Qt::AlignCenter); cl->addWidget(new QCheckBox);
        t->setCellWidget(r, 0, cw);

        auto *role = new QLabel(e.role); role->setAlignment(Qt::AlignCenter);
        role->setStyleSheet(roleBadge(e.role));
        t->setCellWidget(r, 5, centered(role));

        const bool ok = QString(e.dispo) == "Disponible";
        auto *badge = new QLabel(e.dispo); badge->setAlignment(Qt::AlignCenter);
        badge->setStyleSheet(ok
            ? "background:rgba(16,185,129,0.25); color:#34D399; border:1px solid #10B981; border-radius:5px; padding:2px 6px; font-size:8pt; font-weight:bold;"
            : "background:rgba(239,68,68,0.25);  color:#F87171; border:1px solid #EF4444; border-radius:5px; padding:2px 6px; font-size:8pt; font-weight:bold;");
        t->setCellWidget(r, 8, centered(badge));

        // icones d'action (consulter / modifier / supprimer) : visuelles uniquement
        auto *aw = new QWidget; auto *al = new QHBoxLayout(aw);
        al->setContentsMargins(2, 2, 2, 2); al->setSpacing(2);
        const char *icons[] = { ":/icons/eye_b.png", ":/icons/edit_b.png", ":/icons/trash_r.png" };
        for (const char *ic : icons) {
            auto *b = new QPushButton; b->setObjectName("rowBtn");
            b->setIcon(QIcon(ic)); b->setIconSize(QSize(16, 16)); b->setCursor(Qt::PointingHandCursor);
            al->addWidget(b);
        }
        t->setCellWidget(r, 9, aw);
    }
    ui->labelCount->setText(QString("Affichage de %1 sur %1 employés").arg(n));

    // case de l'en-tete (affichage uniquement)
    m_headCheck = new QCheckBox(h->viewport());
    m_headCheck->show();
    connect(h, &QHeaderView::sectionResized, this, [this]() { positionHeaderCheck(); });
    QTimer::singleShot(0, this, [this]() { positionHeaderCheck(); });

    // ---- graphiques (donnees d'exemple fixes)
    ui->chartRole->setData({ { "Organisateur", 2, QColor("#2F67E0") },
                             { "Technicien",   4, QColor("#0BF8F6") },
                             { "Mentor",       2, QColor("#7C4DFF") } });
    ui->chartDept->setData({ { "Logistique",         3, QColor("#0BF8F6") },
                             { "Technique",          3, QColor("#2F67E0") },
                             { "Pédagogie/Mentorat", 2, QColor("#7C4DFF") } });
}

void GSmartHackathon::positionHeaderCheck()
{
    auto *h = ui->tableEmployes->horizontalHeader();
    const QSize s = m_headCheck->sizeHint();
    m_headCheck->move(h->sectionViewportPosition(0) + (h->sectionSize(0) - s.width()) / 2,
                      (h->height() - s.height()) / 2);
}

// ===========================================================================
//  AUTHENTIFICATION + PRIVILEGES
// ===========================================================================
namespace {
void repolish(QWidget *w) { w->style()->unpolish(w); w->style()->polish(w); }

// autorise / interdit un controle ; interdit = grise + info-bulle explicative
void setAllowed(QWidget *w, bool allowed, const QString &deniedTip)
{
    w->setEnabled(allowed);
    w->setToolTip(allowed ? QString() : deniedTip);
    if (!allowed) {
        auto *e = new QGraphicsOpacityEffect(w);
        e->setOpacity(0.4);
        w->setGraphicsEffect(e);
    } else {
        w->setGraphicsEffect(nullptr);
    }
}
QString cellLabel(QTableWidget *t, int r, int c)
{
    QWidget *w = t->cellWidget(r, c);
    QLabel *l = w ? w->findChild<QLabel *>() : nullptr;
    return l ? l->text() : QString();
}
QString cellText(QTableWidget *t, int r, int c)
{
    QTableWidgetItem *it = t->item(r, c);
    return it ? it->text() : QString();
}
const int kMaxTeamSize = 6;     // effectif maximum d'une equipe (attribution automatique)
} // namespace

void GSmartHackathon::setupAuth()
{
    // ---- page de connexion
    ui->btnEyeLogin->setCheckable(true);
    connect(ui->btnEyeLogin, &QToolButton::toggled, this, [this](bool on) {
        ui->linePasswordLogin->setEchoMode(on ? QLineEdit::Normal : QLineEdit::Password);
    });
    connect(ui->lineEmailLogin, &QLineEdit::returnPressed, this, [this]() { ui->linePasswordLogin->setFocus(); });
    connect(ui->linePasswordLogin, &QLineEdit::returnPressed, this, [this]() { doLogin(); });
    auto clearErr = [this]() {
        ui->labelErrorLogin->hide();
        for (QLineEdit *l : { ui->lineEmailLogin, ui->linePasswordLogin }) { l->setProperty("error", false); repolish(l); }
    };
    connect(ui->lineEmailLogin, &QLineEdit::textEdited, this, clearErr);
    connect(ui->linePasswordLogin, &QLineEdit::textEdited, this, clearErr);

    QSettings s("HackFlow", "HackFlow");
    const QString saved = s.value("login/email").toString();
    if (!saved.isEmpty()) { ui->lineEmailLogin->setText(saved); ui->checkRememberLogin->setChecked(true); }

    connect(ui->btnForgotLogin, &QPushButton::clicked, this, [this]() {
        ForgotPasswordDialog dlg(ui->lineEmailLogin->text(), this);
        if (dlg.exec() == QDialog::Accepted) {
            ui->lineEmailLogin->setText(dlg.email());
            ui->linePasswordLogin->clear();
            ui->linePasswordLogin->setFocus();
            QMessageBox::information(this, "Mot de passe modifié",
                                     "Votre mot de passe a été modifié. Vous pouvez maintenant vous connecter.");
        }
    });
    connect(ui->btnCreateAccountLogin, &QPushButton::clicked, this, [this]() {
        CreateAccountDialog dlg(this);
        if (dlg.exec() == QDialog::Accepted) {
            ui->lineEmailLogin->setText(dlg.email());
            ui->linePasswordLogin->clear();
            ui->linePasswordLogin->setFocus();
            QMessageBox::information(this, "Compte créé",
                QString("Votre compte a été créé%1.\n\nRôle actuel : %2 (accès limité à l'accueil). "
                        "Un administrateur peut vous attribuer d'autres privilèges.\n\n"
                        "Connectez-vous avec votre e-mail et votre mot de passe.")
                .arg(dlg.verified() ? " et votre e-mail est vérifié" : "")
                .arg(AuthManager::defaultRegistrationRole()));
        }
    });

    // ---- menu du bouton Admin (haut a droite de l'application)
    m_adminMenu = new QMenu(this);
    m_adminMenu->setStyleSheet(
        "QMenu { background:#05264F; color:#FFFFFF; border:1px solid #2F67E0; padding:6px; }"
        "QMenu::item { padding:8px 22px; border-radius:6px; }"
        "QMenu::item:selected { background:#4B26A0; }"
        "QMenu::item:disabled { color:#8FB0FF; }"
        "QMenu::separator { height:1px; background:rgba(47,103,224,0.5); margin:5px 8px; }");
    m_actWho    = m_adminMenu->addAction(QString());
    m_actWho->setEnabled(false);
    m_adminMenu->addSeparator();
    m_actUsers  = m_adminMenu->addAction("Gérer les utilisateurs et les privilèges…");
    m_actSmtp   = m_adminMenu->addAction("Configurer l'e-mail (SMTP)…");
    m_adminMenu->addSeparator();
    m_actLogout = m_adminMenu->addAction("Se déconnecter");
    ui->btnAdmin->setMenu(m_adminMenu);
    ui->btnAdmin->setPopupMode(QToolButton::InstantPopup);
    ui->btnAdmin->setStyleSheet("QToolButton::menu-indicator { image: none; width: 0px; }");

    connect(m_actUsers, &QAction::triggered, this, [this]() {
        if (!AuthManager::instance().has(ManageUsers)) return;
        UserManagementDialog(this).exec();
    });
    connect(m_actSmtp, &QAction::triggered, this, [this]() {
        if (!AuthManager::instance().has(ManageUsers)) return;
        SmtpSettingsDialog(AuthManager::instance().current()->email, this).exec();
    });
    connect(m_actLogout, &QAction::triggered, this, [this]() { doLogout(); });

    // ---- attribution automatique (CV)
    connect(ui->btnAffecter, &QPushButton::clicked, this, [this]() { onAssignToTeam(); });
}

void GSmartHackathon::doLogin()
{
    QString err;
    const QString email = ui->lineEmailLogin->text().trimmed();
    if (!AuthManager::instance().login(email, ui->linePasswordLogin->text(), &err)) {
        ui->labelErrorLogin->setText(err);
        ui->labelErrorLogin->show();
        ui->linePasswordLogin->setProperty("error", true);
        repolish(ui->linePasswordLogin);
        ui->linePasswordLogin->clear();
        ui->linePasswordLogin->setFocus();
        return;
    }
    QSettings s("HackFlow", "HackFlow");
    if (ui->checkRememberLogin->isChecked()) s.setValue("login/email", email);
    else s.remove("login/email");

    ui->linePasswordLogin->clear();
    ui->btnEyeLogin->setChecked(false);
    ui->labelErrorLogin->hide();
    applySession();
    showApplication();
}

void GSmartHackathon::doLogout()
{
    AuthManager::instance().logout();
    ui->stackedPages->setCurrentIndex(0);
    ui->btnNavAccueil->setChecked(true);
    ui->linePasswordLogin->clear();
    showConnexion();
    (ui->lineEmailLogin->text().isEmpty() ? ui->lineEmailLogin : ui->linePasswordLogin)->setFocus();
}

void GSmartHackathon::applySession()
{
    const UserAccount *u = AuthManager::instance().current();
    if (!u) return;
    ui->labelAdminName->setText(u->name);
    ui->labelAdminMail->setText(u->email);
    ui->btnAdmin->setText(u->name + "  ▾\n" + u->role);
    m_actWho->setText(u->email + "  ·  " + u->role);
    applyPrivileges();
}

void GSmartHackathon::applyPrivileges()
{
    const AuthManager &A = AuthManager::instance();

    // ---- menu de gauche : un module non autorise disparait
    struct Nav { QPushButton *b; Privilege p; };
    const Nav navs[] = {
        { ui->btnNavAccueil, ViewAccueil }, { ui->btnNavParticipants, ViewParticipants },
        { ui->btnNavEquipes, ViewEquipes }, { ui->btnNavProjets, ViewProjets },
        { ui->btnNavEmployes, ViewEmployes }, { ui->btnNavJury, ViewJury },
        { ui->btnNavParametres, ViewParametres } };
    for (const Nav &n : navs) n.b->setVisible(A.has(n.p));

    // ---- menu Admin
    m_actUsers->setVisible(A.has(ManageUsers));
    m_actSmtp->setVisible(A.has(ManageUsers));

    // ---- module Employes
    auto deny = [](Privilege p) { return "Privilège requis : " + AuthManager::privilegeLabel(p); };
    ui->frameForm->setEnabled(A.has(EmployeAdd));
    setAllowed(ui->btnAjouter,  A.has(EmployeAdd),    deny(EmployeAdd));
    setAllowed(ui->btnVider,    A.has(EmployeAdd),    deny(EmployeAdd));
    setAllowed(ui->btnAffecter, A.has(EmployeAssign), deny(EmployeAssign));
    setAllowed(ui->btnExporter, A.has(EmployeExport), deny(EmployeExport));
    ui->frameForm->setToolTip(A.has(EmployeAdd) ? QString() : deny(EmployeAdd));

    auto *t = ui->tableEmployes;
    for (int r = 0; r < t->rowCount(); ++r) {
        QWidget *aw = t->cellWidget(r, 9);
        if (!aw) continue;
        const auto btns = aw->findChildren<QPushButton *>();       // oeil, crayon, poubelle
        if (btns.size() >= 3) {
            setAllowed(btns[1], A.has(EmployeEdit),   deny(EmployeEdit));
            setAllowed(btns[2], A.has(EmployeDelete), deny(EmployeDelete));
        }
    }
}

// ===========================================================================
//  ATTRIBUTION AUTOMATIQUE INTELLIGENTE (traitement de CV)
// ===========================================================================
void GSmartHackathon::onAssignToTeam()
{
    if (!AuthManager::instance().has(EmployeAssign)) return;
    auto *t = ui->tableEmployes;

    // employe vise : case cochee, sinon ligne selectionnee
    QList<int> rows;
    for (int r = 0; r < t->rowCount(); ++r) {
        QWidget *cw = t->cellWidget(r, 0);
        QCheckBox *cb = cw ? cw->findChild<QCheckBox *>() : nullptr;
        if (cb && cb->isChecked()) rows << r;
    }
    if (rows.isEmpty() && t->currentRow() >= 0) rows << t->currentRow();
    if (rows.isEmpty()) {
        QMessageBox::information(this, "Affecter à une équipe",
            "Sélectionnez d'abord un employé (clic sur sa ligne ou case à cocher).");
        return;
    }
    if (rows.size() > 1) {
        QMessageBox::information(this, "Affecter à une équipe",
            "Sélectionnez un seul employé à la fois pour analyser son CV.");
        return;
    }
    const int row = rows.first();

    auto splitTeams = [](const QString &s) {
        QStringList l;
        for (const QString &p : s.split(',', Qt::SkipEmptyParts)) {
            const QString v = p.trimmed();
            if (!v.isEmpty() && v != QString::fromUtf8("—")) l << v;
        }
        return l;
    };

    EmployeeInfo emp;
    emp.id     = cellText(t, row, 1);
    emp.nom    = cellText(t, row, 2);
    emp.prenom = cellText(t, row, 3);
    emp.email  = cellText(t, row, 4);
    emp.role   = cellLabel(t, row, 5);
    emp.dept   = cellText(t, row, 6);
    emp.teams  = splitTeams(cellText(t, row, 7));
    emp.dispo  = cellLabel(t, row, 8);

    QMap<QString, int> load;                                  // effectif actuel de chaque equipe
    for (int r = 0; r < t->rowCount(); ++r)
        for (const QString &tm : splitTeams(cellText(t, r, 7))) load[tm]++;

    CvAssignDialog dlg(emp, load, kMaxTeamSize, this);
    if (dlg.exec() != QDialog::Accepted || dlg.chosenTeam().isEmpty()) return;

    QStringList teams = emp.teams;
    if (!teams.contains(dlg.chosenTeam())) teams << dlg.chosenTeam();
    if (QTableWidgetItem *it = t->item(row, 7)) it->setText(teams.join(", "));
    QWidget *cw = t->cellWidget(row, 0);
    if (QCheckBox *cb = cw ? cw->findChild<QCheckBox *>() : nullptr) cb->setChecked(false);
    QMessageBox::information(this, "Affectation effectuée",
        QString("%1 %2 a été affecté(e) à « %3 ».").arg(emp.prenom, emp.nom, dlg.chosenTeam()));
}
