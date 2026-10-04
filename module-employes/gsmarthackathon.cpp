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

// ---------------------------------------------------------------------------
GSmartHackathon::GSmartHackathon(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::GSmartHackathon)
{
    ui->setupUi(this);
    setupNavigation();
    setupDecor();
    showConnexion();            // l'application demarre sur l'ecran de connexion
}

GSmartHackathon::~GSmartHackathon() { delete ui; }

// ---------------------------------------------------------------------------
//  NAVIGATION : seule partie fonctionnelle de cette phase
// ---------------------------------------------------------------------------
void GSmartHackathon::setupNavigation()
{
    // Connexion -> application
    connect(ui->btnLogin, &QPushButton::clicked, this, [this]() { showApplication(); });
    // application -> Connexion
    connect(ui->btnLogout, &QPushButton::clicked, this, [this]() { showConnexion(); });

    // bouton du menu -> index de la page dans stackedPages (meme ordre que le .ui)
    const QList<QPair<QPushButton *, int>> nav = {
        { ui->btnNavAccueil,      0 },   // Accueil / Tableau de bord
        { ui->btnNavParticipants, 1 },
        { ui->btnNavEquipes,      2 },
        { ui->btnNavProjets,      3 },
        { ui->btnNavMentors,      4 },
        { ui->btnNavEmployes,     5 },   // notre module
        { ui->btnNavEquipements,  6 },
        { ui->btnNavJury,         7 },
        { ui->btnNavParametres,   8 }
    };
    for (const auto &p : nav) {
        const int index = p.second;
        connect(p.first, &QPushButton::clicked, this, [this, index]() {
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
