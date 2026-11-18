#include "employeescontroller.h"
#include "ui_mainwindow.h"
#include "mainwindow.h"
#include "authmanager.h"
#include "cvassigndialog.h"
#include "dialogstyle.h"

#include <QAction>
#include <QCheckBox>
#include <QCollator>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QGraphicsOpacityEffect>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStringConverter>
#include <QPair>
#include <QSignalBlocker>
#include <QStandardPaths>
#include <QTableWidget>
#include <QTextStream>
#include <QVBoxLayout>
#include <algorithm>

namespace {
const int kMaxTeamSize = 6;     // effectif maximum d'une equipe (attribution automatique)

QString roleBadge(const QString &r)
{
    if (r == "Organisateur")
        return "background:rgba(47,103,224,0.30); color:#8FB0FF; border:1px solid #2F67E0; border-radius:5px; padding:2px 6px; font-size:8pt; font-weight:bold;";
    if (r == "Technicien")
        return "background:rgba(11,248,246,0.13); color:#0BF8F6; border:1px solid #0BF8F6; border-radius:5px; padding:2px 6px; font-size:8pt; font-weight:bold;";
    return "background:rgba(124,77,255,0.28); color:#C2AEFF; border:1px solid #7C4DFF; border-radius:5px; padding:2px 6px; font-size:8pt; font-weight:bold;";
}
QString dispoBadge(bool ok)
{
    return ok
        ? "background:rgba(16,185,129,0.25); color:#34D399; border:1px solid #10B981; border-radius:5px; padding:2px 6px; font-size:8pt; font-weight:bold;"
        : "background:rgba(239,68,68,0.25);  color:#F87171; border:1px solid #EF4444; border-radius:5px; padding:2px 6px; font-size:8pt; font-weight:bold;";
}
QWidget *centered(QWidget *inner)
{
    auto *w = new QWidget; auto *l = new QHBoxLayout(w);
    l->setContentsMargins(3, 6, 3, 6); l->setAlignment(Qt::AlignCenter); l->addWidget(inner);
    return w;
}
// autorise / interdit un controle ; interdit = grise + info-bulle explicative
void setAllowed(QWidget *w, bool allowed, const QString &deniedTip)
{
    w->setEnabled(allowed);
    w->setToolTip(allowed ? QString() : deniedTip);
    w->setGraphicsEffect(nullptr);
    if (!allowed) {
        auto *e = new QGraphicsOpacityEffect(w);
        e->setOpacity(0.4);
        w->setGraphicsEffect(e);
    }
}
QString teamsText(const QStringList &t) { return t.isEmpty() ? QString::fromUtf8("—") : t.join(", "); }
bool can(Privilege p) { return AuthManager::instance().has(p); }
QString deny(Privilege p) { return QString::fromUtf8("Privilège requis : ") + AuthManager::privilegeLabel(p); }
QString csvCell(QString s)
{
    s.replace(QChar('"'), QString("\"\""));
    return QString("\"") + s + QString("\"");
}
} // namespace

// ---------------------------------------------------------------------------
EmployeesController::EmployeesController(Ui::MainWindow *u, QMainWindow *host, QCheckBox *headerCheck)
    : QObject(host), ui(u), m_host(host), m_headCheck(headerCheck)
{
    load();
    if (m_all.isEmpty()) { seed(); save(); }

    // le filtre « Disponibles » est decoche au depart : tout le monde est affiche
    ui->checkDispo->setChecked(false);

    // tri : menu du bouton « Trier »
    m_sortMenu = new QMenu(ui->btnTrier);
    m_sortMenu->setStyleSheet(
        "QMenu { background:#05264F; color:#FFFFFF; border:1px solid #2F67E0; padding:6px; }"
        "QMenu::item { padding:8px 22px; border-radius:6px; }"
        "QMenu::item:selected { background:#4B26A0; }");
    struct Opt { const char *text; int key; bool asc; };
    const Opt opts[] = {
        { "ID (croissant)", 0, true }, { "ID (décroissant)", 0, false },
        { "Nom (A → Z)", 1, true },    { "Nom (Z → A)", 1, false },
        { "Prénom (A → Z)", 2, true }, { "Rôle", 3, true },
        { "Département", 4, true },    { "Disponibles d'abord", 5, true }
    };
    for (const Opt &o : opts) {
        QAction *a = m_sortMenu->addAction(QString::fromUtf8(o.text));
        const int k = o.key; const bool asc = o.asc;
        connect(a, &QAction::triggered, this, [this, k, asc]() { m_sortKey = k; m_sortAsc = asc; refresh(); });
    }

    // formulaire
    connect(ui->btnAjouter, &QPushButton::clicked, this, [this]() { onAdd(); });
    connect(ui->btnVider,   &QPushButton::clicked, this, [this]() { onClear(); });
    // liste
    connect(ui->btnProfil,  &QPushButton::clicked, this, [this]() { onProfileButton(); });
    connect(ui->btnTrier,   &QPushButton::clicked, this, [this]() { onSortMenu(); });
    connect(ui->btnExporter,&QPushButton::clicked, this, [this]() { onExport(); });
    // filtres
    connect(ui->lineSearch, &QLineEdit::textChanged, this, [this]() { refresh(); });
    connect(ui->comboFilterRole, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() { refresh(); });
    connect(ui->comboFilterDept, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() { refresh(); });
    connect(ui->checkDispo, &QCheckBox::toggled, this, [this]() { refresh(); });
    // case de l'en-tete : tout cocher / tout decocher (lignes affichees)
    connect(m_headCheck, &QCheckBox::clicked, this, [this](bool on) {
        m_updatingChecks = true;
        for (int id : visibleIds()) { if (on) m_checked.insert(id); else m_checked.remove(id); }
        for (int r = 0; r < ui->tableEmployes->rowCount(); ++r) {
            QWidget *cw = ui->tableEmployes->cellWidget(r, 0);
            if (QCheckBox *cb = cw ? cw->findChild<QCheckBox *>() : nullptr) cb->setChecked(on);
        }
        m_updatingChecks = false;
    });

    setFormMode(false);
    refresh();
}

// ---------------------------------------------------------------------------
//  DONNEES
// ---------------------------------------------------------------------------
void EmployeesController::seed()
{
    auto add = [this](const char *nom, const char *prenom, const char *mail, const char *role,
                      const char *dept, const QStringList &teams, bool ok) {
        EmployeeRecord e;
        e.id = m_nextId++;
        e.nom = QString::fromUtf8(nom); e.prenom = QString::fromUtf8(prenom); e.email = mail;
        e.role = role; e.dept = QString::fromUtf8(dept); e.teams = teams;
        e.dispo = ok ? "Disponible" : "Indisponible";
        m_all.push_back(e);
    };
    m_nextId = 1;
    add("Ben Ali",  "Ahmed",   "ahmed.benali@email.com",   "Organisateur", "Pédagogie/Mentorat", { "Team Alpha", "Team Beta" }, true);
    add("Trabelsi", "Sara",    "sara.trabelsi@email.com",  "Technicien",   "Logistique",         {}, true);
    add("Khaled",   "Omar",    "omar.khaled@email.com",    "Technicien",   "Technique",          {}, false);
    add("Ayari",    "Meriem",  "meriem.ayari@email.com",   "Mentor",       "Pédagogie/Mentorat", { "Team Gamma" }, true);
    add("Jabri",    "Yassine", "yassine.jabri@email.com",  "Technicien",   "Technique",          { "Team Beta" }, true);
    add("Zribi",    "Ines",    "ines.zribi@email.com",     "Mentor",       "Technique",          { "Team Delta", "Team Alpha", "Team Omega" }, true);
    add("Gharbi",   "Amine",   "amine.gharbi@email.com",   "Organisateur", "Logistique",         {}, false);
    add("Mansouri", "Leila",   "leila.mansouri@email.com", "Technicien",   "Logistique",         {}, true);
}

void EmployeesController::load()
{
    m_all.clear();
    QFile f(AuthManager::dataDir() + "/employees.json");
    if (!f.open(QIODevice::ReadOnly)) return;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) return;
    const QJsonArray arr = doc.object().value("employees").toArray();
    int maxId = 0;
    for (const QJsonValue &v : arr) {
        const QJsonObject o = v.toObject();
        EmployeeRecord e;
        e.id = o.value("id").toInt();
        e.nom = o.value("nom").toString(); e.prenom = o.value("prenom").toString();
        e.email = o.value("email").toString(); e.role = o.value("role").toString();
        e.dept = o.value("dept").toString(); e.dispo = o.value("dispo").toString();
        for (const QJsonValue &t : o.value("teams").toArray()) e.teams << t.toString();
        if (e.id <= 0 || e.nom.isEmpty() || e.email.isEmpty()) continue;
        maxId = std::max(maxId, e.id);
        m_all.push_back(e);
    }
    m_nextId = std::max(doc.object().value("nextId").toInt(1), maxId + 1);
}

void EmployeesController::save() const
{
    QJsonArray arr;
    for (const EmployeeRecord &e : m_all) {
        QJsonArray teams;
        for (const QString &t : e.teams) teams << t;
        arr << QJsonObject{ { "id", e.id }, { "nom", e.nom }, { "prenom", e.prenom }, { "email", e.email },
                            { "role", e.role }, { "dept", e.dept }, { "dispo", e.dispo }, { "teams", teams } };
    }
    QSaveFile f(AuthManager::dataDir() + "/employees.json");
    if (!f.open(QIODevice::WriteOnly)) return;
    f.write(QJsonDocument(QJsonObject{ { "nextId", m_nextId }, { "employees", arr } }).toJson(QJsonDocument::Indented));
    f.commit();
}

int EmployeesController::indexOfId(int id) const
{
    for (int i = 0; i < m_all.size(); ++i) if (m_all[i].id == id) return i;
    return -1;
}

bool EmployeesController::emailTaken(const QString &email, int exceptId) const
{
    for (const EmployeeRecord &e : m_all)
        if (e.id != exceptId && e.email.compare(email, Qt::CaseInsensitive) == 0) return true;
    return false;
}

// ---------------------------------------------------------------------------
//  TABLEAU
// ---------------------------------------------------------------------------
QVector<int> EmployeesController::visibleIds() const
{
    const QString q = ui->lineSearch->text().trimmed();
    const int ri = ui->comboFilterRole->currentIndex();
    const int di = ui->comboFilterDept->currentIndex();
    const QString role = ri > 0 ? ui->comboFilterRole->currentText() : QString();
    const QString dept = di > 0 ? ui->comboFilterDept->currentText() : QString();
    const bool onlyOk = ui->checkDispo->isChecked();

    QVector<const EmployeeRecord *> list;
    for (const EmployeeRecord &e : m_all) {
        if (!role.isEmpty() && e.role != role) continue;
        if (!dept.isEmpty() && e.dept != dept) continue;
        if (onlyOk && e.dispo != "Disponible") continue;
        if (!q.isEmpty()) {
            const QString hay = QStringList{ QString::number(e.id), e.nom, e.prenom, e.email, e.role, e.dept,
                                             e.dispo, e.teams.join(' ') }.join(' ');
            if (!hay.contains(q, Qt::CaseInsensitive)) continue;
        }
        list.push_back(&e);
    }

    QCollator col; col.setCaseSensitivity(Qt::CaseInsensitive);
    const int key = m_sortKey; const bool asc = m_sortAsc;
    std::stable_sort(list.begin(), list.end(), [&](const EmployeeRecord *a, const EmployeeRecord *b) {
        int c = 0;
        switch (key) {
        case 0: c = a->id - b->id; break;
        case 1: c = col.compare(a->nom, b->nom); break;
        case 2: c = col.compare(a->prenom, b->prenom); break;
        case 3: c = col.compare(a->role, b->role); break;
        case 4: c = col.compare(a->dept, b->dept); break;
        case 5: c = (a->dispo == "Disponible" ? 0 : 1) - (b->dispo == "Disponible" ? 0 : 1); break;
        }
        return asc ? c < 0 : c > 0;
    });
    QVector<int> ids;
    for (const EmployeeRecord *e : list) ids << e->id;
    return ids;
}

QVector<int> EmployeesController::checkedIds() const
{
    QVector<int> out;
    for (int id : visibleIds()) if (m_checked.contains(id)) out << id;
    return out;
}

void EmployeesController::fillRow(int row, const EmployeeRecord &e)
{
    auto *t = ui->tableEmployes;
    const int id = e.id;
    t->setItem(row, 0, new QTableWidgetItem(""));
    t->setItem(row, 1, new QTableWidgetItem(QString::number(e.id)));
    t->setItem(row, 2, new QTableWidgetItem(e.nom));
    t->setItem(row, 3, new QTableWidgetItem(e.prenom));
    t->setItem(row, 4, new QTableWidgetItem(e.email));
    t->setItem(row, 6, new QTableWidgetItem(e.dept));
    t->setItem(row, 7, new QTableWidgetItem(teamsText(e.teams)));

    auto *cw = new QWidget; auto *cl = new QHBoxLayout(cw);
    cl->setContentsMargins(0, 0, 0, 0); cl->setAlignment(Qt::AlignCenter);
    auto *cb = new QCheckBox; cb->setChecked(m_checked.contains(id));
    cl->addWidget(cb);
    connect(cb, &QCheckBox::toggled, this, [this, id](bool on) {
        if (on) m_checked.insert(id); else m_checked.remove(id);
        if (!m_updatingChecks) updateHeaderCheck();
    });
    t->setCellWidget(row, 0, cw);

    auto *role = new QLabel(e.role); role->setAlignment(Qt::AlignCenter);
    role->setStyleSheet(roleBadge(e.role));
    t->setCellWidget(row, 5, centered(role));

    auto *badge = new QLabel(e.dispo); badge->setAlignment(Qt::AlignCenter);
    badge->setStyleSheet(dispoBadge(e.dispo == "Disponible"));
    t->setCellWidget(row, 8, centered(badge));

    // actions : consulter / modifier / supprimer
    auto *aw = new QWidget; auto *al = new QHBoxLayout(aw);
    al->setContentsMargins(2, 2, 2, 2); al->setSpacing(2);
    const char *icons[] = { ":/icons/eye_b.png", ":/icons/edit_b.png", ":/icons/trash_r.png" };
    const char *tips[]  = { "Consulter le profil", "Modifier", "Supprimer" };
    QPushButton *btns[3];
    for (int i = 0; i < 3; ++i) {
        auto *b = new QPushButton; b->setObjectName("rowBtn");
        b->setIcon(QIcon(icons[i])); b->setIconSize(QSize(16, 16)); b->setCursor(Qt::PointingHandCursor);
        b->setToolTip(QString::fromUtf8(tips[i]));
        al->addWidget(b); btns[i] = b;
    }
    connect(btns[0], &QPushButton::clicked, this, [this, id]() { showProfile(id); });
    connect(btns[1], &QPushButton::clicked, this, [this, id]() { startEdit(id); });
    connect(btns[2], &QPushButton::clicked, this, [this, id]() { deleteEmployee(id); });
    setAllowed(btns[1], can(EmployeEdit),   deny(EmployeEdit));
    setAllowed(btns[2], can(EmployeDelete), deny(EmployeDelete));
    t->setCellWidget(row, 9, aw);
}

void EmployeesController::refresh()
{
    auto *t = ui->tableEmployes;
    const QVector<int> ids = visibleIds();
    m_updatingChecks = true;
    t->setUpdatesEnabled(false);
    t->setRowCount(0);                 // supprime aussi les widgets des cellules
    t->setRowCount(ids.size());
    for (int r = 0; r < ids.size(); ++r) {
        const int i = indexOfId(ids[r]);
        if (i >= 0) fillRow(r, m_all[i]);
    }
    t->setUpdatesEnabled(true);
    m_updatingChecks = false;

    ui->labelCount->setText(QString::fromUtf8("Affichage de %1 sur %2 employés").arg(ids.size()).arg(m_all.size()));
    updateHeaderCheck();
    updateCharts();
}

void EmployeesController::updateHeaderCheck()
{
    const QVector<int> ids = visibleIds();
    bool all = !ids.isEmpty();
    for (int id : ids) if (!m_checked.contains(id)) { all = false; break; }
    QSignalBlocker b(m_headCheck);
    m_headCheck->setChecked(all);
}

void EmployeesController::updateCharts()
{
    int nOrg = 0, nTech = 0, nMen = 0, nLog = 0, nTec = 0, nPed = 0;
    for (const EmployeeRecord &e : m_all) {
        if (e.role == "Organisateur") ++nOrg; else if (e.role == "Technicien") ++nTech; else if (e.role == "Mentor") ++nMen;
        if (e.dept == "Logistique") ++nLog; else if (e.dept == "Technique") ++nTec; else ++nPed;
    }
    ChartData roles;
    if (nOrg)  roles.push_back({ "Organisateur", double(nOrg),  QColor("#2F67E0") });
    if (nTech) roles.push_back({ "Technicien",   double(nTech), QColor("#0BF8F6") });
    if (nMen)  roles.push_back({ "Mentor",       double(nMen),  QColor("#7C4DFF") });
    ui->chartRole->setData(roles);
    ui->chartDept->setData({ { "Logistique", double(nLog), QColor("#0BF8F6") },
                             { "Technique",  double(nTec), QColor("#2F67E0") },
                             { QString::fromUtf8("Pédagogie/Mentorat"), double(nPed), QColor("#7C4DFF") } });
}

void EmployeesController::setSearchText(const QString &t)
{
    if (ui->lineSearch->text() != t) ui->lineSearch->setText(t);
}

// ---------------------------------------------------------------------------
//  FORMULAIRE : Ajouter / Modifier / Vider
// ---------------------------------------------------------------------------
void EmployeesController::setInfo(const QString &title, const QString &text, const char *kind)
{
    Q_UNUSED(kind);
    ui->labelInfoTitle->setText(title);
    ui->labelInfoText->setText(text);
}

void EmployeesController::setFormMode(bool editing)
{
    if (editing) {
        ui->labelFormTitle->setText(QString::fromUtf8("Modifier l'<span style=\"color:#7C4DFF;\">employé</span>"));
        ui->btnAjouter->setText(" Enregistrer");
        ui->btnAjouter->setIcon(QIcon(":/icons/check_w.png"));
        ui->btnVider->setText(" Annuler");
        setInfo(QString::fromUtf8("Modification en cours"),
                QString::fromUtf8("Changez les champs puis cliquez sur « Enregistrer ». « Annuler » abandonne la modification."), "info");
    } else {
        ui->labelFormTitle->setText(QString::fromUtf8("Ajouter un <span style=\"color:#7C4DFF;\">nouvel employé</span>"));
        ui->btnAjouter->setText(" Ajouter");
        ui->btnAjouter->setIcon(QIcon(":/icons/plus_w.png"));
        ui->btnVider->setText(" Vider");
        setInfo(QString::fromUtf8("Enregistrez les modifications"),
                QString::fromUtf8("Remplissez les champs puis cliquez sur « Ajouter » pour enregistrer l'employé. Le champ « Équipe(s) » est facultatif."), "info");
    }
}

void EmployeesController::resetForm()
{
    ui->lineNom->clear();
    ui->linePrenom->clear();
    ui->lineEmail->clear();
    ui->comboRole->setCurrentIndex(0);
    ui->comboDept->setCurrentIndex(0);
    ui->comboDispo->setCurrentIndex(0);
    ui->comboEquipes->clearChecked();
    ui->lineId->setText(QString::fromUtf8("Généré automatiquement"));
    ui->lineId->setProperty("employeeId", 0);
    m_editId = 0;
}

void EmployeesController::cancelEdit()
{
    resetForm();
    setFormMode(false);
}

void EmployeesController::onClear()
{
    // « Vider » efface le formulaire (et annule une modification en cours)
    cancelEdit();
    ui->lineNom->setFocus();
}

bool EmployeesController::validateForm(QString *error, QWidget **focus) const
{
    static const QRegularExpression nameRe(QStringLiteral("^[\\p{L}][\\p{L} '\\-]*$"));
    const QString nom = ui->lineNom->text().trimmed();
    const QString prenom = ui->linePrenom->text().trimmed();
    const QString email = ui->lineEmail->text().trimmed();
    if (nom.size() < 2 || !nameRe.match(nom).hasMatch()) {
        *error = QString::fromUtf8("Le nom est obligatoire (au moins 2 lettres, sans chiffres).");
        *focus = ui->lineNom; return false;
    }
    if (prenom.size() < 2 || !nameRe.match(prenom).hasMatch()) {
        *error = QString::fromUtf8("Le prénom est obligatoire (au moins 2 lettres, sans chiffres).");
        *focus = ui->linePrenom; return false;
    }
    if (email.isEmpty()) {
        *error = QString::fromUtf8("L'e-mail est obligatoire.");
        *focus = ui->lineEmail; return false;
    }
    if (!AuthManager::isValidEmail(email)) {
        *error = QString::fromUtf8("L'adresse e-mail n'est pas valide (exemple : prenom.nom@email.com).");
        *focus = ui->lineEmail; return false;
    }
    if (emailTaken(email, m_editId)) {
        *error = QString::fromUtf8("Cet e-mail est déjà utilisé par un autre employé.");
        *focus = ui->lineEmail; return false;
    }
    return true;
}

void EmployeesController::onAdd()
{
    const bool editing = m_editId != 0;
    if (!can(editing ? EmployeEdit : EmployeAdd)) {
        QMessageBox::warning(m_host, "Accès refusé", deny(editing ? EmployeEdit : EmployeAdd));
        return;
    }
    QString err; QWidget *focus = nullptr;
    if (!validateForm(&err, &focus)) {
        QMessageBox::warning(m_host, editing ? "Modifier l'employé" : "Ajouter un employé", err);
        if (focus) focus->setFocus();
        return;
    }

    EmployeeRecord e;
    e.nom = ui->lineNom->text().trimmed();
    e.prenom = ui->linePrenom->text().trimmed();
    e.email = ui->lineEmail->text().trimmed();
    e.role = ui->comboRole->currentText();
    e.dept = ui->comboDept->currentText();
    e.dispo = ui->comboDispo->currentText();
    e.teams = ui->comboEquipes->checkedItems();

    if (editing) {
        const int i = indexOfId(m_editId);
        if (i < 0) { cancelEdit(); refresh(); return; }
        e.id = m_editId;
        m_all[i] = e;
        save();
        cancelEdit();
        refresh();
        setInfo(QString::fromUtf8("Employé modifié"),
                QString::fromUtf8("Les informations de %1 %2 ont été enregistrées.").arg(e.prenom, e.nom), "ok");
    } else {
        e.id = m_nextId++;
        m_all.push_back(e);
        save();
        resetForm();
        refresh();
        setInfo(QString::fromUtf8("Employé ajouté"),
                QString::fromUtf8("%1 %2 a été ajouté(e) avec l'ID %3.").arg(e.prenom, e.nom).arg(e.id), "ok");
    }
    ui->lineNom->setFocus();
}

void EmployeesController::startEdit(int id)
{
    if (!can(EmployeEdit)) return;
    const int i = indexOfId(id);
    if (i < 0) return;
    const EmployeeRecord &e = m_all[i];
    m_editId = id;
    ui->lineId->setText(QString::number(e.id));
    ui->lineNom->setText(e.nom);
    ui->linePrenom->setText(e.prenom);
    ui->lineEmail->setText(e.email);
    ui->comboRole->setCurrentText(e.role);
    ui->comboDept->setCurrentText(e.dept);
    ui->comboDispo->setCurrentText(e.dispo);
    ui->comboEquipes->setCheckedItems(e.teams);
    setFormMode(true);
    ui->lineNom->setFocus();
}

// ---------------------------------------------------------------------------
//  SUPPRIMER / PROFIL
// ---------------------------------------------------------------------------
void EmployeesController::deleteEmployee(int id)
{
    if (!can(EmployeDelete)) return;
    const int i = indexOfId(id);
    if (i < 0) return;
    const EmployeeRecord e = m_all[i];
    const auto r = QMessageBox::question(m_host, "Supprimer l'employé",
        QString::fromUtf8("Supprimer définitivement %1 %2 (ID %3) ?").arg(e.prenom, e.nom).arg(e.id),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (r != QMessageBox::Yes) return;
    m_all.removeAt(i);
    m_checked.remove(id);
    if (m_editId == id) cancelEdit();
    save();
    refresh();
    setInfo(QString::fromUtf8("Employé supprimé"),
            QString::fromUtf8("%1 %2 a été supprimé(e) de la liste.").arg(e.prenom, e.nom), "ok");
}

void EmployeesController::showProfile(int id)
{
    const int i = indexOfId(id);
    if (i < 0) return;
    const EmployeeRecord e = m_all[i];

    QDialog dlg(m_host);
    dlg.setWindowTitle("Profil de l'employé");
    dlg.setStyleSheet(hackflowDialogStyle());
    dlg.setMinimumWidth(460);
    auto *lay = new QVBoxLayout(&dlg);
    lay->setContentsMargins(26, 22, 26, 22);
    lay->setSpacing(12);

    auto *title = new QLabel(e.prenom + " " + e.nom); title->setObjectName("dlgTitle");
    auto *sub = new QLabel(QString::fromUtf8("Employé n° %1").arg(e.id)); sub->setObjectName("dlgSub");
    lay->addWidget(title); lay->addWidget(sub);

    auto *role = new QLabel(e.role); role->setStyleSheet(roleBadge(e.role));
    auto *dispo = new QLabel(e.dispo); dispo->setStyleSheet(dispoBadge(e.dispo == "Disponible"));
    auto *badges = new QHBoxLayout; badges->setSpacing(8);
    badges->addWidget(role); badges->addWidget(dispo); badges->addStretch();
    lay->addLayout(badges);

    auto *grid = new QGridLayout; grid->setHorizontalSpacing(18); grid->setVerticalSpacing(8);
    const QPair<QString, QString> rows[] = {
        { QString::fromUtf8("E-mail"), e.email },
        { QString::fromUtf8("Département"), e.dept },
        { QString::fromUtf8("Équipe(s)"), teamsText(e.teams) },
    };
    int r = 0;
    for (const auto &p : rows) {
        auto *k = new QLabel(p.first); k->setStyleSheet("color:#AEBBDD;");
        auto *v = new QLabel(p.second); v->setStyleSheet("color:#FFFFFF; font-weight:bold;");
        v->setTextInteractionFlags(Qt::TextSelectableByMouse); v->setWordWrap(true);
        grid->addWidget(k, r, 0); grid->addWidget(v, r, 1); ++r;
    }
    grid->setColumnStretch(1, 1);
    lay->addLayout(grid);

    auto *row = new QHBoxLayout; row->addStretch();
    QPushButton *edit = nullptr;
    if (can(EmployeEdit)) {
        edit = new QPushButton("Modifier"); edit->setObjectName("secondary"); row->addWidget(edit);
        connect(edit, &QPushButton::clicked, &dlg, &QDialog::accept);
    }
    auto *close = new QPushButton("Fermer"); row->addWidget(close);
    connect(close, &QPushButton::clicked, &dlg, &QDialog::reject);
    lay->addLayout(row);

    if (dlg.exec() == QDialog::Accepted && edit) startEdit(id);
}

int EmployeesController::singleTarget(const QString &caption)
{
    QVector<int> ids = checkedIds();
    if (ids.isEmpty() && ui->tableEmployes->currentRow() >= 0) {
        QTableWidgetItem *it = ui->tableEmployes->item(ui->tableEmployes->currentRow(), 1);
        if (it) ids << it->text().toInt();
    }
    if (ids.isEmpty()) {
        QMessageBox::information(m_host, caption,
            QString::fromUtf8("Sélectionnez d'abord un employé (clic sur sa ligne ou case à cocher)."));
        return 0;
    }
    if (ids.size() > 1) {
        QMessageBox::information(m_host, caption,
            QString::fromUtf8("Sélectionnez un seul employé à la fois."));
        return 0;
    }
    return ids.first();
}

void EmployeesController::onProfileButton()
{
    const int id = singleTarget("Consulter le profil");
    if (id) showProfile(id);
}

// ---------------------------------------------------------------------------
//  TRIER / EXPORTER
// ---------------------------------------------------------------------------
void EmployeesController::onSortMenu()
{
    m_sortMenu->exec(ui->btnTrier->mapToGlobal(QPoint(0, ui->btnTrier->height() + 4)));
}

void EmployeesController::onExport()
{
    if (!can(EmployeExport)) {
        QMessageBox::warning(m_host, "Accès refusé", deny(EmployeExport));
        return;
    }
    const QVector<int> ids = visibleIds();
    if (ids.isEmpty()) {
        QMessageBox::information(m_host, "Exporter", QString::fromUtf8("Aucun employé à exporter (la liste affichée est vide)."));
        return;
    }
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    const QString path = QFileDialog::getSaveFileName(m_host, "Exporter les employés",
                                                      dir + "/employes.csv", "Fichier CSV (*.csv)");
    if (path.isEmpty()) return;

    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(m_host, "Exporter", QString::fromUtf8("Impossible d'écrire le fichier :\n%1").arg(path));
        return;
    }
    QTextStream out(&f);
    out.setEncoding(QStringConverter::Utf8);
    out.setGenerateByteOrderMark(true);          // Excel reconnait ainsi les accents
    out << QString::fromUtf8("ID;Nom;Prénom;Email;Rôle;Département;Équipe(s);Disponibilité\n");
    for (int id : ids) {
        const EmployeeRecord &e = m_all[indexOfId(id)];
        out << e.id << ';' << csvCell(e.nom) << ';' << csvCell(e.prenom) << ';' << csvCell(e.email) << ';'
            << csvCell(e.role) << ';' << csvCell(e.dept) << ';' << csvCell(e.teams.join(", ")) << ';'
            << csvCell(e.dispo) << '\n';
    }
    out.flush();
    if (!f.commit()) {
        QMessageBox::warning(m_host, "Exporter", QString::fromUtf8("Échec de l'écriture du fichier."));
        return;
    }
    QMessageBox::information(m_host, "Exporter", QString::fromUtf8("%1 employé(s) exporté(s) vers :\n%2").arg(ids.size()).arg(QDir::toNativeSeparators(path)));
}

// ---------------------------------------------------------------------------
//  ATTRIBUTION AUTOMATIQUE (CV)
// ---------------------------------------------------------------------------
void EmployeesController::assignToTeam()
{
    if (!can(EmployeAssign)) return;
    const int id = singleTarget(QString::fromUtf8("Affecter à une équipe"));
    if (!id) return;
    const int i = indexOfId(id);
    if (i < 0) return;
    const EmployeeRecord &e = m_all[i];

    EmployeeInfo emp;
    emp.id = QString::number(e.id); emp.nom = e.nom; emp.prenom = e.prenom; emp.email = e.email;
    emp.role = e.role; emp.dept = e.dept; emp.dispo = e.dispo; emp.teams = e.teams;

    QMap<QString, int> load;                                  // effectif actuel de chaque equipe
    for (const EmployeeRecord &x : m_all) for (const QString &tm : x.teams) load[tm]++;

    CvAssignDialog dlg(emp, load, kMaxTeamSize, m_host);
    if (dlg.exec() != QDialog::Accepted || dlg.chosenTeam().isEmpty()) return;

    EmployeeRecord &w = m_all[i];
    if (!w.teams.contains(dlg.chosenTeam())) w.teams << dlg.chosenTeam();
    m_checked.remove(id);
    save();
    refresh();
    QMessageBox::information(m_host, QString::fromUtf8("Affectation effectuée"),
        QString::fromUtf8("%1 %2 a été affecté(e) à « %3 ».").arg(w.prenom, w.nom, dlg.chosenTeam()));
}
