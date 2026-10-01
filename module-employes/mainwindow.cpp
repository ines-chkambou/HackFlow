#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QMenu>
#include <QLabel>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QPushButton>
#include <QHeaderView>
#include <QMessageBox>
#include <QTimer>
#include <QRegularExpression>

static QString roleBadgeStyle(const QString &r)
{
    if (r == "Organisateur")
        return "background:rgba(47,103,224,0.30); color:#8FB0FF; border:1px solid #2F67E0; border-radius:5px; padding:2px 6px; font-size:8pt; font-weight:bold;";
    if (r == "Technicien")
        return "background:rgba(11,248,246,0.13); color:#0BF8F6; border:1px solid #0BF8F6; border-radius:5px; padding:2px 6px; font-size:8pt; font-weight:bold;";
    return "background:rgba(124,77,255,0.28); color:#C2AEFF; border:1px solid #7C4DFF; border-radius:5px; padding:2px 6px; font-size:8pt; font-weight:bold;";
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    m_teams = QStringList{ "Team Alpha", "Team Beta", "Team Gamma", "Team Delta", "Team Omega" };
    ui->comboEquipes->setItems(m_teams);

    setupTable();
    setupMenus();
    setupNavigation();
    seedData();

    auto *adm = new QMenu(this);
    adm->addAction("Mon profil");
    adm->addSeparator();
    adm->addAction("Deconnexion");
    ui->btnAdmin->setMenu(adm);

    refreshTable();
}

MainWindow::~MainWindow() { delete ui; }

void MainWindow::seedData()
{
    auto add = [this](const QString &n, const QString &p, const QString &m, const QString &r,
                      const QString &d, const QString &eq, const QString &di) {
        Employe e; e.id = m_nextId++; e.nom = n; e.prenom = p; e.email = m; e.role = r;
        e.dept = d; e.equipes = splitTeams(eq); e.dispo = di;
        m_data.push_back(e);
    };
    add("Ben Ali",  "Ahmed",  "ahmed.benali@email.com",  "Organisateur", "Pedagogie/Mentorat", "Team Alpha, Team Beta", "Disponible");
    add("Trabelsi", "Sara",   "sara.trabelsi@email.com", "Technicien",   "Logistique",         "",                     "Disponible");
    add("Khaled",   "Omar",   "omar.khaled@email.com",   "Technicien",   "Technique",          "",                     "Indisponible");
    add("Ayari",    "Meriem", "meriem.ayari@email.com",  "Mentor",       "Pedagogie/Mentorat", "Team Gamma",           "Disponible");
    add("Jabri",    "Yassine","yassine.jabri@email.com", "Technicien",   "Technique",          "",                     "Disponible");
    add("Zribi",    "Ines",   "ines.zribi@email.com",    "Mentor",       "Technique",          "Team Delta, Team Alpha, Team Omega", "Disponible");
    add("Gharbi",   "Amine",  "amine.gharbi@email.com",  "Organisateur", "Logistique",         "",                     "Indisponible");
    add("Mansouri", "Leila",  "leila.mansouri@email.com","Technicien",   "Logistique",         "",                     "Disponible");
}

QStringList MainWindow::splitTeams(const QString &s)
{
    QStringList out;
    for (const QString &t : s.split(QRegularExpression("[,;]"), Qt::SkipEmptyParts))
        if (!t.trimmed().isEmpty()) out << t.trimmed();
    return out;
}

void MainWindow::setupTable()
{
    auto *t = ui->tableEmployes;
    auto *h = t->horizontalHeader();
    h->setSectionResizeMode(QHeaderView::Interactive);
    h->setSectionResizeMode(7, QHeaderView::Stretch);
    h->setMinimumSectionSize(28);
    t->setColumnWidth(0, 32);
    t->setColumnWidth(1, 34);
    t->setColumnWidth(2, 80);
    t->setColumnWidth(3, 72);
    t->setColumnWidth(4, 175);
    t->setColumnWidth(5, 94);
    t->setColumnWidth(6, 118);
    t->setColumnWidth(8, 96);
    t->setColumnWidth(9, 84);
    t->verticalHeader()->setDefaultSectionSize(46);
    t->setFocusPolicy(Qt::NoFocus);
    t->setWordWrap(true);

    m_headCheck = new QCheckBox(h->viewport());
    m_headCheck->show();
    connect(m_headCheck, &QCheckBox::toggled, this, [this](bool on) {
        for (int r = 0; r < ui->tableEmployes->rowCount(); ++r)
            if (auto *w = ui->tableEmployes->cellWidget(r, 0))
                if (auto *cb = w->findChild<QCheckBox *>()) cb->setChecked(on);
    });
    connect(h, &QHeaderView::sectionResized, this, [this]() { positionHeaderCheck(); });
    QTimer::singleShot(0, this, [this]() { positionHeaderCheck(); });
}

void MainWindow::positionHeaderCheck()
{
    auto *h = ui->tableEmployes->horizontalHeader();
    const QSize s = m_headCheck->sizeHint();
    m_headCheck->move(h->sectionViewportPosition(0) + (h->sectionSize(0) - s.width()) / 2,
                      (h->height() - s.height()) / 2);
}

void MainWindow::setupMenus()
{
    // Boutons Trier / Exporter : visuels uniquement (aucun menu).
}

void MainWindow::setupNavigation()
{
    const auto buttons = ui->frameSidebar->findChildren<QPushButton *>();
    for (QPushButton *b : buttons) {
        if (!b->property("nav").toBool()) continue;
        connect(b, &QPushButton::clicked, this, [this, b]() {
            if (b == ui->btnNavEmployes) return;
            QMessageBox::information(this, "Navigation",
                QString("Le module est developpe par un autre membre du groupe."));
            ui->btnNavEmployes->setChecked(true);
        });
    }
}

QVector<Employe> MainWindow::filtered() const
{
    QVector<Employe> out;
    const QString q = ui->lineSearch->text().trimmed();
    const int fr = ui->comboFilterRole->currentIndex();
    const int fd = ui->comboFilterDept->currentIndex();
    const bool onlyAvail = ui->checkDispo->isChecked();

    for (const Employe &e : m_data) {
        if (fr > 0 && e.role != ui->comboFilterRole->currentText()) continue;
        if (fd > 0 && e.dept != ui->comboFilterDept->currentText()) continue;
        if (onlyAvail && e.dispo != "Disponible") continue;
        if (!q.isEmpty()) {
            const QString hay = QStringList{QString::number(e.id), e.nom, e.prenom, e.email, e.role,
                                            e.dept, e.equipes.join(" "), e.dispo}.join(" ");
            if (!hay.contains(q, Qt::CaseInsensitive)) continue;
        }
        out.push_back(e);
    }
    return out;
}

int MainWindow::indexOfId(int id) const
{
    for (int i = 0; i < m_data.size(); ++i) if (m_data[i].id == id) return i;
    return -1;
}

QVector<int> MainWindow::checkedIds() const { return {}; }
int MainWindow::selectedId() const { return -1; }

void MainWindow::refreshTable()
{
    const QVector<Employe> rows = filtered();
    auto *t = ui->tableEmployes;
    t->clearContents();
    t->setRowCount(rows.size());
    m_headCheck->blockSignals(true); m_headCheck->setChecked(false); m_headCheck->blockSignals(false);

    auto centered = [](QWidget *inner) {
        auto *w = new QWidget; auto *l = new QHBoxLayout(w);
        l->setContentsMargins(3, 6, 3, 6); l->setAlignment(Qt::AlignCenter); l->addWidget(inner);
        return w;
    };

    for (int r = 0; r < rows.size(); ++r) {
        const Employe &e = rows[r];
        auto mk = [](const QString &s) { return new QTableWidgetItem(s); };
        t->setItem(r, 0, mk(""));
        auto *idIt = mk(QString::number(e.id));
        idIt->setData(Qt::UserRole, e.id);
        t->setItem(r, 1, idIt);
        t->setItem(r, 2, mk(e.nom));
        t->setItem(r, 3, mk(e.prenom));
        t->setItem(r, 4, mk(e.email));
        t->setItem(r, 6, mk(e.dept));
        t->setItem(r, 7, mk(e.equipes.isEmpty() ? QString("--") : e.equipes.join(", ")));

        auto *cb = new QCheckBox;
        auto *cw = new QWidget; auto *cl = new QHBoxLayout(cw);
        cl->setContentsMargins(0, 0, 0, 0); cl->setAlignment(Qt::AlignCenter); cl->addWidget(cb);
        t->setCellWidget(r, 0, cw);

        auto *role = new QLabel(e.role);
        role->setAlignment(Qt::AlignCenter);
        role->setStyleSheet(roleBadgeStyle(e.role));
        t->setCellWidget(r, 5, centered(role));

        const bool ok = (e.dispo == "Disponible");
        auto *badge = new QLabel(e.dispo);
        badge->setAlignment(Qt::AlignCenter);
        badge->setStyleSheet(ok
            ? "background:rgba(16,185,129,0.25); color:#34D399; border:1px solid #10B981; border-radius:5px; padding:2px 6px; font-size:8pt; font-weight:bold;"
            : "background:rgba(239,68,68,0.25);  color:#F87171; border:1px solid #EF4444; border-radius:5px; padding:2px 6px; font-size:8pt; font-weight:bold;");
        t->setCellWidget(r, 8, centered(badge));

        auto *aw = new QWidget; auto *al = new QHBoxLayout(aw);
        al->setContentsMargins(2, 2, 2, 2); al->setSpacing(2);
        auto mkBtn = [](const QString &icon, const QString &tip) {
            auto *b = new QPushButton; b->setObjectName("rowBtn");
            b->setIcon(QIcon(icon)); b->setIconSize(QSize(16, 16));
            b->setToolTip(tip); b->setCursor(Qt::PointingHandCursor);
            return b;
        };
        al->addWidget(mkBtn(":/icons/eye_b.png",   "Consulter"));
        al->addWidget(mkBtn(":/icons/edit_b.png",  "Modifier"));
        al->addWidget(mkBtn(":/icons/trash_r.png", "Supprimer"));
        t->setCellWidget(r, 9, aw);
    }
    ui->labelCount->setText(QString("Affichage de %1 sur %2 employes").arg(rows.size()).arg(m_data.size()));
    positionHeaderCheck();
}

// Slots metier : conserves pour compilation, jamais appeles
void MainWindow::refreshStats() { }
void MainWindow::setFormTitle(bool) { }
bool MainWindow::readForm(Employe &) { return false; }
void MainWindow::loadToForm(const Employe &) { }
void MainWindow::onAjouter() { }
void MainWindow::onVider() { }
void MainWindow::onProfil() { }
void MainWindow::onAffecter() { }
void MainWindow::onSortChosen(QAction *) { }
void MainWindow::onExport(QAction *) { }
void MainWindow::startEdit(int) { }
void MainWindow::deleteEmploye(int) { }
void MainWindow::showProfile(int) { }
void MainWindow::exportCsv(const QVector<Employe> &) { }
void MainWindow::exportPdf(const QVector<Employe> &) { }