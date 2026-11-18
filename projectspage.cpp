#include "projectspage.h"

#include <algorithm>

#include <QAction>
#include <QApplication>
#include <QButtonGroup>
#include <QComboBox>
#include <QDateEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLayoutItem>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPair>
#include <QPixmap>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QStackedWidget>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QVBoxLayout>

// ---------------------------------------------------------------- helpers

namespace {

const char *kStyle = R"(
QWidget { color:#e8eefc; font-family:'Segoe UI','Inter',sans-serif; font-size:13px; }
QMainWindow, QDialog, QMessageBox { background:#06152d; }
QWidget#central { background:#06152d; }
QWidget#sidebar { background:#071a3a; border-right:1px solid #12305c; }
QWidget#topbar  { background:#06152d; border-bottom:1px solid #12305c; }
QWidget#page    { background:transparent; }
QWidget#rightPanel { background:transparent; }
QWidget#projectsModule { background:transparent; }
QScrollArea, QScrollArea > QWidget > QWidget { background:transparent; border:0; }
QLabel { background:transparent; }

QLabel#brand { font-size:24px; font-weight:700; color:white; }
QLabel#brandSub { font-size:8px; font-weight:700; color:#0bf8f6; }
QLabel#pageTitle { font-size:26px; font-weight:700; color:white; }
QLabel#pageSub { color:#9fb3d9; }
QLabel#panelTitle { font-size:16px; font-weight:700; color:white; }
QLabel#muted { color:#8aa0cc; font-size:11px; }
QLabel#footer { color:#8aa0cc; font-size:11px; }

QPushButton#nav { text-align:left; padding:11px 16px; border:0; border-radius:10px;
                  background:transparent; color:#dbe5ff; font-size:14px; }
QPushButton#nav:hover { background:#0e2a56; }
QPushButton#nav:checked { background:#4b4dff; color:white; font-weight:600; }

QFrame#userCard { background:#0a1f45; border:1px solid #17407a; border-radius:12px; }
QFrame#panel, QFrame#stat { background:#0a1d3d; border:1px solid #14376b; border-radius:12px; }
QFrame#projCard { background:#0a1d3d; border:1px solid #14376b; border-radius:12px; }
QFrame#projCard:hover { border:1px solid #3a63d8; }
QLabel#projName { font-size:15px; font-weight:700; color:white; }
QLabel#projDesc { color:#b7c6e8; font-size:12px; }
QLabel#teamBadge { background:#12326a; border-radius:10px; padding:2px 10px; color:#cfe0ff; font-size:11px; }
QLabel#metaLbl { color:#aebfe3; font-size:11px; }
QLabel#statNum { font-size:22px; font-weight:700; color:white; }

QLineEdit, QComboBox, QSpinBox, QDateEdit {
    background:#071a3a; border:1px solid #17407a; border-radius:8px; padding:7px 10px; color:white;
    selection-background-color:#4b4dff; }
QLineEdit:focus, QComboBox:focus, QSpinBox:focus, QDateEdit:focus { border:1px solid #0bf8f6; }
QComboBox::drop-down { border:0; width:24px; }
QComboBox QAbstractItemView { background:#0a1d3d; color:white; border:1px solid #17407a;
                              selection-background-color:#4b4dff; }

QPushButton#primary { background:#4b4dff; color:white; border:0; border-radius:9px;
                      padding:11px 20px; font-weight:600; font-size:14px; }
QPushButton#primary:hover { background:#6567ff; }
QPushButton#primary:pressed { background:#3a3cd6; }
QPushButton#icon { background:#0a1d3d; border:1px solid #17407a; border-radius:8px;
                   min-width:36px; max-width:36px; min-height:36px; max-height:36px; font-size:14px; }
QPushButton#icon:hover { background:#143a74; }
QPushButton#view { background:#071a3a; border:1px solid #17407a; border-radius:8px;
                   min-width:40px; max-width:40px; min-height:40px; max-height:40px; font-size:15px; }
QPushButton#view:hover { background:#143a74; }
QPushButton#view:checked { background:#4b4dff; border:1px solid #4b4dff; }
QPushButton#link { background:transparent; border:0; color:#8f7bff; font-size:12px; }
QPushButton#link:hover { color:#b3a5ff; }
QPushButton#top { background:transparent; border:0; border-radius:10px; padding:6px 10px; font-size:13px; }
QPushButton#top:hover { background:#0e2a56; }
QPushButton { background:#143a74; border:1px solid #1d4c94; border-radius:8px; padding:7px 16px; }
QPushButton:hover { background:#1d4c94; }

QProgressBar { background:#12306a; border:0; border-radius:3px; max-height:6px; min-height:6px; }
QMenu { background:#0a1d3d; border:1px solid #17407a; padding:6px; }
QMenu::item { padding:7px 22px; border-radius:6px; }
QMenu::item:selected { background:#4b4dff; }
QMenu::item:disabled { color:#7f93bd; }
QListWidget { background:#071a3a; border:1px solid #17407a; border-radius:8px; }
QScrollBar:vertical { background:transparent; width:10px; }
QScrollBar::handle:vertical { background:#1d4c94; border-radius:5px; min-height:30px; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }
QScrollBar:horizontal { background:transparent; height:10px; }
QScrollBar::handle:horizontal { background:#1d4c94; border-radius:5px; min-width:30px; }
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width:0; }
)";

struct TeamStyle { QString icon; QString color; };

TeamStyle teamStyle(const QString &team)
{
    if (team == "GreenTeam")   return { QString::fromUtf8("🍃"), "#1fb56b" };
    if (team == "TechVision")  return { QString::fromUtf8("🛡"), "#16b9d6" };
    if (team == "UrbanAI")     return { QString::fromUtf8("🎮"), "#b44dff" };
    if (team == "CleanEnergy") return { QString::fromUtf8("📈"), "#2a8bff" };
    if (team == "SmartCity")   return { QString::fromUtf8("📖"), "#f29a2e" };
    if (team == "HealthCare")  return { QString::fromUtf8("💙"), "#1ea7c9" };
    return { QString::fromUtf8("💡"), "#4b4dff" };
}

struct StatusStyle { QString badgeBg; QString bar; QString icon; };

StatusStyle statusStyle(const QString &s)
{
    if (s == QString::fromUtf8("Terminé"))
        return { "#5b34b8", "#8f5bff", QString::fromUtf8("✔") };
    if (s == "En attente")
        return { "#9a5a14", "#ffb02e", QString::fromUtf8("⏸") };
    return { "#0f6e4a", "#0bf8f6", QString::fromUtf8("🚀") };
}

void clearLayout(QLayout *l)
{
    while (QLayoutItem *it = l->takeAt(0)) {
        if (QWidget *w = it->widget())
            w->deleteLater();
        delete it;
    }
}

QString fmtDate(const QDate &d)
{
    return QLocale(QLocale::French).toString(d, "d MMM yyyy");
}

const QString kDone = QString::fromUtf8("Terminé");

} // namespace

// ------------------------------------------------------------ DonutWidget

void DonutWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const int side = qMin(width(), height()) - 24;
    QRectF r((width() - side) / 2.0, (height() - side) / 2.0, side, side);
    QRectF arc = r.adjusted(9, 9, -9, -9);
    const int total = m_run + m_done + m_wait;

    QPen pen;
    pen.setWidth(18);
    pen.setCapStyle(Qt::FlatCap);

    if (total == 0) {
        pen.setColor(QColor("#16305c"));
        p.setPen(pen);
        p.drawEllipse(arc);
    } else {
        int start = 90 * 16;
        const int values[3] = { m_run, m_done, m_wait };
        const QColor colors[3] = { QColor("#14e0e6"), QColor("#a259ff"), QColor("#ffb02e") };
        for (int i = 0; i < 3; ++i) {
            if (values[i] == 0) continue;
            int span = -int(5760.0 * values[i] / total);
            pen.setColor(colors[i]);
            p.setPen(pen);
            p.drawArc(arc, start, span);
            start += span;
        }
    }

    QFont f = font();
    f.setPointSize(20);
    f.setBold(true);
    p.setFont(f);
    p.setPen(QColor("white"));
    p.drawText(r.adjusted(0, -10, 0, -10), Qt::AlignCenter, QString::number(total));

    f.setPointSize(9);
    f.setBold(false);
    p.setFont(f);
    p.setPen(QColor("#9fb3d9"));
    p.drawText(r.adjusted(0, 28, 0, 28), Qt::AlignCenter, "Projets");
}

// -------------------------------------------------------------- MainWindow

ProjectsPage::ProjectsPage(QWidget *parent) : QWidget(parent)
{
    setObjectName("projectsModule");
    setStyleSheet(QString::fromUtf8(kStyle));     // le style du module ne s'applique qu'a cette page

    if (QSqlDatabase::database("hackflow_oracle").isOpen()) {
        loadProjectsFromDatabase();
    } else {
        seedData();
    }

    QVBoxLayout *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);
    lay->addWidget(buildProjectsPage(), 1);

    refresh();
}

bool ProjectsPage::loadProjectsFromDatabase()
{
    QSqlQuery query(QSqlDatabase::database("hackflow_oracle"));
    if (!query.exec("SELECT ID_PROJET, TITRE, DESCRIPTION, STATUT, DATE_DEBUT, DATE_RENDU, EQUIPE FROM INES.PROJET ORDER BY ID_PROJET")) {
        QMessageBox::critical(this, QString::fromUtf8("Lecture Oracle"), query.lastError().text());
        return false;
    }
    m_projects.clear();
    m_nextId = 1;
    while (query.next()) {
        Project p;
        p.id = query.value(0).toInt();
        p.name = query.value(1).toString();
        p.desc = query.value(2).toString();
        p.status = query.value(3).toString();
        p.start = query.value(4).toDate();
        p.end = query.value(5).toDate();
        p.team = query.value(6).toString();
        p.members = 1;
        p.progress = (p.status == kDone) ? 100 : 0;
        const TeamStyle ts = teamStyle(p.team);
        p.icon = ts.icon;
        p.color = ts.color;
        m_nextId = qMax(m_nextId, p.id + 1);
        m_projects.push_back(p);
    }
    return true;
}

// Recherche venant de la barre du haut de HackFlow
void ProjectsPage::setSearchText(const QString &text)
{
    if (m_projectSearch) m_projectSearch->setText(text);
}

void ProjectsPage::seedData()
{
    auto add = [this](const QString &name, const QString &desc, const QString &team,
                      const QString &status, int members, int progress,
                      QDate s, QDate e, const QString &icon, const QString &color) {
        Project p;
        p.id = m_nextId++;
        p.name = name; p.desc = desc; p.team = team; p.status = status;
        p.members = members; p.progress = progress; p.start = s; p.end = e;
        p.icon = icon; p.color = color;
        m_projects.push_back(p);
    };
    const QString run = "En cours", wait = "En attente";
    const QString d = kDone;
    add("GreenTeam", QString::fromUtf8("Application de gestion des déchets et sensibilisation environnementale."),
        "GreenTeam", run, 4, 65, QDate(2025,4,12), QDate(2025,4,13), QString::fromUtf8("🍃"), "#1fb56b");
    add("HealthCare", QString::fromUtf8("Plateforme de suivi médical et prise de rendez-vous en ligne."),
        "TechVision", run, 5, 40, QDate(2025,4,12), QDate(2025,4,13), QString::fromUtf8("🛡"), "#5a44e8");
    add("SmartCity", QString::fromUtf8("Solution pour une ville plus intelligente et connectée."),
        "UrbanAI", wait, 5, 20, QDate(2025,4,12), QDate(2025,4,13), QString::fromUtf8("📖"), "#f29a2e");
    add("EcoSolutions", QString::fromUtf8("Plateforme de recyclage et de tri intelligent."),
        "CleanEnergy", run, 5, 75, QDate(2025,4,11), QDate(2025,4,13), QString::fromUtf8("📈"), "#2a8bff");
    add("UrbanAI", QString::fromUtf8("Application de mobilité urbaine intelligente."),
        "SmartCity", d, 5, 100, QDate(2025,4,10), QDate(2025,4,12), QString::fromUtf8("🎮"), "#b44dff");
    add("MedTech", QString::fromUtf8("Solution de télémédecine pour les zones rurales."),
        "HealthCare", run, 4, 60, QDate(2025,4,12), QDate(2025,4,13), QString::fromUtf8("💙"), "#1ea7c9");
    add("EduLearn", QString::fromUtf8("Plateforme d'apprentissage collaboratif en ligne."),
        "GreenTeam", run, 4, 55, QDate(2025,4,12), QDate(2025,4,13), QString::fromUtf8("🎓"), "#1fb56b");
    add("AgriSmart", QString::fromUtf8("Capteurs connectés pour une agriculture durable."),
        "GreenTeam", run, 3, 35, QDate(2025,4,12), QDate(2025,4,13), QString::fromUtf8("🌱"), "#1fb56b");
    add("WasteTrack", QString::fromUtf8("Suivi des collectes de déchets en temps réel."),
        "GreenTeam", d, 4, 100, QDate(2025,4,10), QDate(2025,4,12), QString::fromUtf8("♻"), "#1fb56b");
    add("SafeRoad", QString::fromUtf8("Prévention des accidents grâce à l'analyse du trafic."),
        "TechVision", run, 5, 80, QDate(2025,4,11), QDate(2025,4,13), QString::fromUtf8("🚦"), "#16b9d6");
    add("FinTrack", QString::fromUtf8("Suivi budgétaire pour les étudiants."),
        "TechVision", d, 3, 100, QDate(2025,4,10), QDate(2025,4,12), QString::fromUtf8("💰"), "#16b9d6");
    add("CityPulse", QString::fromUtf8("Tableau de bord de la qualité de l'air urbain."),
        "UrbanAI", run, 4, 45, QDate(2025,4,12), QDate(2025,4,13), QString::fromUtf8("🏙"), "#b44dff");

    m_activities = {
        { "Leila Sassi",     QString::fromUtf8("a créé le projet GreenTeam"),          QString::fromUtf8("il y a 2 min") },
        { "Karim Ben Ali",   QString::fromUtf8("a rejoint l'équipe TechVision"),       QString::fromUtf8("il y a 15 min") },
        { "Ahmed Trabelsi",  QString::fromUtf8("a mis à jour le projet SmartCity"),    QString::fromUtf8("il y a 32 min") },
        { "Sara Mahmoud",    QString::fromUtf8("a commenté sur HealthCare"),           QString::fromUtf8("il y a 1 h") },
        { "Youssef Jebali",  QString::fromUtf8("a terminé le projet UrbanAI"),         QString::fromUtf8("il y a 2 h") }
    };
    m_notif = 1;
}

// ------------------------------------------------------------- UI building

QWidget *ProjectsPage::makeStat(const QString &icon, const QString &color,
                              QLabel *&number, const QString &caption)
{
    QFrame *f = new QFrame;
    f->setObjectName("stat");
    QHBoxLayout *h = new QHBoxLayout(f);
    h->setContentsMargins(14, 12, 14, 12);
    h->setSpacing(12);
    QLabel *ic = new QLabel(icon);
    ic->setFixedSize(44, 44);
    ic->setAlignment(Qt::AlignCenter);
    ic->setStyleSheet(QString("background:%1;border-radius:22px;font-size:18px;color:white;").arg(color));
    QVBoxLayout *t = new QVBoxLayout;
    t->setSpacing(0);
    number = new QLabel("0");
    number->setObjectName("statNum");
    QLabel *c = new QLabel(caption);
    c->setObjectName("muted");
    c->setStyleSheet("font-size:12px;color:#b7c6e8;");
    t->addWidget(number);
    t->addWidget(c);
    h->addWidget(ic);
    h->addLayout(t);
    h->addStretch();
    return f;
}

QWidget *ProjectsPage::buildProjectsPage()
{
    QWidget *page = new QWidget;
    page->setObjectName("page");
    QHBoxLayout *root = new QHBoxLayout(page);
    root->setContentsMargins(24, 20, 24, 16);
    root->setSpacing(18);

    // ----- colonne principale
    QVBoxLayout *main = new QVBoxLayout;
    main->setSpacing(14);

    // stats
    QHBoxLayout *stats = new QHBoxLayout;
    stats->setSpacing(12);
    stats->addWidget(makeStat(QString::fromUtf8("📄"), "#2a56a8", m_totalLbl, "Total des projets"));
    stats->addWidget(makeStat(QString::fromUtf8("🚀"), "#12a566", m_runLbl, "En cours"));
    stats->addWidget(makeStat(QString::fromUtf8("✔"), "#b44dff", m_doneLbl, QString::fromUtf8("Terminés")));
    stats->addWidget(makeStat(QString::fromUtf8("⏸"), "#e08a1e", m_waitLbl, "En attente"));
    main->addLayout(stats);

    // filtres
    QHBoxLayout *fl = new QHBoxLayout;
    fl->setSpacing(10);
    m_projectSearch = new QLineEdit;
    m_projectSearch->setPlaceholderText(QString::fromUtf8("🔍  Rechercher un projet..."));
    m_projectSearch->setClearButtonEnabled(true);
    m_projectSearch->setMinimumHeight(40);
    m_statusBox = new QComboBox;
    m_statusBox->addItems({ "Tous les statuts", "En cours", kDone, "En attente" });
    m_statusBox->setMinimumHeight(40);
    m_statusBox->setMinimumWidth(150);
    m_teamBox = new QComboBox;
    m_teamBox->setMinimumHeight(40);
    m_teamBox->setMinimumWidth(170);
    m_gridBtn = new QPushButton(QString::fromUtf8("▦"));
    m_gridBtn->setObjectName("view");
    m_gridBtn->setCheckable(true);
    m_gridBtn->setToolTip("Affichage en grille");
    m_listBtn = new QPushButton(QString::fromUtf8("☰"));
    m_listBtn->setObjectName("view");
    m_listBtn->setCheckable(true);
    m_listBtn->setChecked(true);
    m_listBtn->setToolTip("Affichage en liste");
    for (QPushButton *b : { m_gridBtn, m_listBtn }) {
        b->setCursor(Qt::PointingHandCursor);
        b->setFocusPolicy(Qt::NoFocus);
    }
    fl->addWidget(m_projectSearch, 1);
    fl->addWidget(m_statusBox);
    fl->addWidget(m_teamBox);
    fl->addWidget(m_gridBtn);
    fl->addWidget(m_listBtn);
    // bouton "Nouveau projet" : affiche dans l'en-tete de la page Projets (voir MainWindow)
    m_newBtn = new QPushButton(QString::fromUtf8("＋   Nouveau projet"));
    m_newBtn->setCursor(Qt::PointingHandCursor);
    m_newBtn->setFocusPolicy(Qt::NoFocus);
    m_newBtn->setStyleSheet(
        "QPushButton { background:#4b4dff; color:white; border:0; border-radius:9px;"
        " padding:11px 20px; font-weight:600; font-size:14px; }"
        "QPushButton:hover { background:#6567ff; }"
        "QPushButton:pressed { background:#3a3cd6; }");
    connect(m_newBtn, &QPushButton::clicked, this, [this]{ addProject(); });
    main->addLayout(fl);

    refreshTeamBox();

    connect(m_projectSearch, &QLineEdit::textChanged, this, [this]{ refreshList(); });
    connect(m_statusBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, [this](int){ refreshList(); });
    connect(m_teamBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, [this](int){ refreshList(); });
    connect(m_gridBtn, &QPushButton::clicked, this, [this]{
        m_gridMode = true;
        m_gridBtn->setChecked(true);
        m_listBtn->setChecked(false);
        refreshList();
    });
    connect(m_listBtn, &QPushButton::clicked, this, [this]{
        m_gridMode = false;
        m_listBtn->setChecked(true);
        m_gridBtn->setChecked(false);
        refreshList();
    });

    // liste scrollable
    QScrollArea *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);   // petit ecran : defilement au lieu de couper les cartes
    QWidget *inner = new QWidget;
    m_listLayout = new QVBoxLayout(inner);
    m_listLayout->setContentsMargins(0, 0, 6, 0);
    m_listLayout->setSpacing(10);
    scroll->setWidget(inner);
    main->addWidget(scroll, 1);

    root->addLayout(main, 1);
    root->addWidget(buildRightPanel());
    return page;
}

QWidget *ProjectsPage::buildRightPanel()
{
    QWidget *rp = new QWidget;
    rp->setObjectName("rightPanel");
    rp->setFixedWidth(340);
    QVBoxLayout *v = new QVBoxLayout(rp);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(12);

    // vue d'ensemble
    QFrame *p1 = new QFrame;
    p1->setObjectName("panel");
    QVBoxLayout *v1 = new QVBoxLayout(p1);
    v1->setContentsMargins(16, 14, 16, 14);
    QLabel *t1 = new QLabel("Vue d'ensemble");
    t1->setObjectName("panelTitle");
    v1->addWidget(t1);
    QHBoxLayout *ov = new QHBoxLayout;
    m_donut = new DonutWidget;
    m_donut->setFixedSize(160, 160);
    ov->addWidget(m_donut);
    QVBoxLayout *legend = new QVBoxLayout;
    legend->setSpacing(10);
    struct L { const char *txt; const char *col; QLabel **lbl; };
    const L items[3] = {
        { "En cours", "#14e0e6", &m_lgRun },
        { "Terminés", "#a259ff", &m_lgDone },
        { "En attente", "#ffb02e", &m_lgWait } };
    for (int i = 0; i < 3; ++i) {
        QHBoxLayout *row = new QHBoxLayout;
        QLabel *dot = new QLabel;
        dot->setFixedSize(12, 12);
        dot->setStyleSheet(QString("background:%1;border-radius:6px;").arg(items[i].col));
        QLabel *name = new QLabel(QString::fromUtf8(items[i].txt));
        QLabel *num = new QLabel("0");
        num->setStyleSheet("font-weight:600;");
        *items[i].lbl = num;
        row->addWidget(dot);
        row->addWidget(name);
        row->addStretch();
        row->addWidget(num);
        legend->addLayout(row);
    }
    ov->addLayout(legend, 1);
    v1->addLayout(ov);
    v->addWidget(p1);

    // projets par équipe
    QFrame *p2 = new QFrame;
    p2->setObjectName("panel");
    QVBoxLayout *v2 = new QVBoxLayout(p2);
    v2->setContentsMargins(16, 14, 16, 14);
    QLabel *t2 = new QLabel(QString::fromUtf8("Projets par équipe"));
    t2->setObjectName("panelTitle");
    v2->addWidget(t2);
    m_teamLayout = new QVBoxLayout;
    m_teamLayout->setSpacing(8);
    v2->addLayout(m_teamLayout);
    v->addWidget(p2);

    // activités
    QFrame *p3 = new QFrame;
    p3->setObjectName("panel");
    QVBoxLayout *v3 = new QVBoxLayout(p3);
    v3->setContentsMargins(16, 14, 16, 14);
    QHBoxLayout *h3 = new QHBoxLayout;
    QLabel *t3 = new QLabel(QString::fromUtf8("Dernières activités"));
    t3->setObjectName("panelTitle");
    QPushButton *all = new QPushButton(QString::fromUtf8("Voir tout →"));
    all->setObjectName("link");
    all->setCursor(Qt::PointingHandCursor);
    all->setFocusPolicy(Qt::NoFocus);
    h3->addWidget(t3);
    h3->addStretch();
    h3->addWidget(all);
    v3->addLayout(h3);
    m_actLayout = new QVBoxLayout;
    m_actLayout->setSpacing(8);
    v3->addLayout(m_actLayout);
    v->addWidget(p3, 1);
    connect(all, &QPushButton::clicked, this, [this]{ showAllActivities(); });

    return rp;
}

QWidget *ProjectsPage::makeCard(const Project &p)
{
    const int id = p.id;
    const StatusStyle ss = statusStyle(p.status);

    QFrame *card = new QFrame;
    card->setObjectName("projCard");
    QHBoxLayout *h = new QHBoxLayout(card);
    h->setContentsMargins(14, 12, 14, 12);
    h->setSpacing(14);

    QLabel *ic = new QLabel(p.icon);
    ic->setFixedSize(52, 52);
    ic->setAlignment(Qt::AlignCenter);
    ic->setStyleSheet(QString("background:%1;border-radius:12px;font-size:22px;color:white;").arg(p.color));
    h->addWidget(ic, 0, Qt::AlignTop);

    QVBoxLayout *mid = new QVBoxLayout;
    mid->setSpacing(4);
    QLabel *nm = new QLabel(p.name);
    nm->setObjectName("projName");
    QLabel *ds = new QLabel(p.desc);
    ds->setObjectName("projDesc");
    ds->setWordWrap(true);
    QHBoxLayout *meta = new QHBoxLayout;
    meta->setSpacing(10);
    QLabel *mb = new QLabel(QString::fromUtf8("👥 ") + QString::number(p.members)
                            + (p.members > 1 ? " membres" : " membre"));
    mb->setObjectName("metaLbl");
    QLabel *tb = new QLabel(QString::fromUtf8("Équipe ") + p.team);
    tb->setObjectName("teamBadge");
    meta->addWidget(mb);
    meta->addWidget(tb);
    meta->addStretch();
    mid->addWidget(nm);
    mid->addWidget(ds);
    mid->addLayout(meta);
    h->addLayout(mid, 1);

    QVBoxLayout *right = new QVBoxLayout;
    right->setSpacing(5);
    QLabel *st = new QLabel(ss.icon + "  " + p.status);
    st->setStyleSheet(QString("background:%1;border-radius:9px;padding:2px 10px;font-size:11px;font-weight:600;color:white;")
                      .arg(ss.badgeBg));
    st->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
    QHBoxLayout *pr = new QHBoxLayout;
    QProgressBar *bar = new QProgressBar;
    bar->setRange(0, 100);
    bar->setValue(p.progress);
    bar->setTextVisible(false);
    bar->setFixedWidth(130);
    bar->setStyleSheet(QString("QProgressBar::chunk{background:%1;border-radius:3px;}").arg(ss.bar));
    QLabel *pc = new QLabel(QString::number(p.progress) + "%");
    pc->setObjectName("metaLbl");
    pr->addWidget(bar);
    pr->addWidget(pc);
    pr->addStretch();
    QLabel *dt = new QLabel(QString::fromUtf8("📅 ") + fmtDate(p.start) + " - " + fmtDate(p.end));
    dt->setObjectName("metaLbl");
    right->addWidget(st, 0, Qt::AlignLeft);
    right->addLayout(pr);
    right->addWidget(dt);
    h->addLayout(right);

    // boutons d'action
    QPushButton *view = new QPushButton(QString::fromUtf8("👁"));
    QPushButton *edit = new QPushButton(QString::fromUtf8("✎"));
    QPushButton *more = new QPushButton(QString::fromUtf8("⋮"));
    view->setToolTip("Voir");
    edit->setToolTip("Modifier");
    more->setToolTip("Plus d'actions");
    for (QPushButton *b : { view, edit, more }) {
        b->setObjectName("icon");
        b->setCursor(Qt::PointingHandCursor);
        b->setFocusPolicy(Qt::NoFocus);
        h->addWidget(b, 0, Qt::AlignVCenter);
    }
    connect(view, &QPushButton::clicked, this, [this, id]{ showProject(id); });
    connect(edit, &QPushButton::clicked, this, [this, id]{ editProject(id); });
    connect(more, &QPushButton::clicked, this, [this, id, more]{
        const int i = indexOfProject(id);
        if (i < 0) return;
        QMenu menu(this);
        QAction *aRun  = menu.addAction(QString::fromUtf8("🚀  Mettre en cours"));
        QAction *aDone = menu.addAction(QString::fromUtf8("✔  Marquer comme terminé"));
        QAction *aWait = menu.addAction(QString::fromUtf8("⏸  Mettre en attente"));
        menu.addSeparator();
        QAction *aDel  = menu.addAction(QString::fromUtf8("🗑  Supprimer"));
        const QString cur = m_projects[i].status;
        aRun->setEnabled(cur != "En cours");
        aDone->setEnabled(cur != kDone);
        aWait->setEnabled(cur != "En attente");
        QAction *r = menu.exec(more->mapToGlobal(QPoint(0, more->height())));
        if (!r) return;
        if (r == aRun)  setStatus(id, "En cours");
        else if (r == aDone) setStatus(id, kDone);
        else if (r == aWait) setStatus(id, "En attente");
        else if (r == aDel)  deleteProject(id);
    });

    return card;
}

// ------------------------------------------------------------------ logic

int ProjectsPage::indexOfProject(int id) const
{
    for (int i = 0; i < m_projects.size(); ++i)
        if (m_projects[i].id == id) return i;
    return -1;
}

QStringList ProjectsPage::knownTeams() const
{
    QStringList t = { "GreenTeam", "TechVision", "UrbanAI", "CleanEnergy", "SmartCity", "HealthCare" };
    for (const Project &p : m_projects)
        if (!t.contains(p.team)) t << p.team;
    return t;
}

void ProjectsPage::refresh()
{
    refreshTeamBox();
    refreshList();
    refreshStats();
    refreshSide();
}

void ProjectsPage::refreshTeamBox()
{
    if (!m_teamBox) return;
    const QString cur = m_teamBox->currentText();
    m_teamBox->blockSignals(true);
    m_teamBox->clear();
    m_teamBox->addItem(QString::fromUtf8("Toutes les équipes"));
    m_teamBox->addItems(knownTeams());
    int idx = m_teamBox->findText(cur);
    m_teamBox->setCurrentIndex(idx < 0 ? 0 : idx);
    m_teamBox->blockSignals(false);
}

//recherche


void ProjectsPage::refreshList()
{
    if (!m_listLayout) return;
    clearLayout(m_listLayout);

    const QString q = m_projectSearch->text().trimmed().toLower();
    const int si = m_statusBox->currentIndex();
    const QString status = si > 0 ? m_statusBox->currentText() : QString();
    const int ti = m_teamBox->currentIndex();
    const QString team = ti > 0 ? m_teamBox->currentText() : QString();

    QVector<const Project *> shown;
    for (const Project &p : m_projects) {
        if (!status.isEmpty() && p.status != status) continue;
        if (!team.isEmpty() && p.team != team) continue;
        if (!q.isEmpty()
            && !p.name.toLower().contains(q)
            && !p.desc.toLower().contains(q)
            && !p.team.toLower().contains(q)) continue;
        shown.push_back(&p);
    }

    if (shown.isEmpty()) {
        QLabel *empty = new QLabel(QString::fromUtf8("Aucun projet trouvé."));
        empty->setAlignment(Qt::AlignCenter);
        empty->setObjectName("pageSub");
        empty->setMinimumHeight(120);
        m_listLayout->addWidget(empty);
    } else if (!m_gridMode) {
        for (const Project *p : shown)
            m_listLayout->addWidget(makeCard(*p));
    } else {
        for (int i = 0; i < shown.size(); i += 2) {
            QWidget *row = new QWidget;
            QHBoxLayout *rh = new QHBoxLayout(row);
            rh->setContentsMargins(0, 0, 0, 0);
            rh->setSpacing(10);
            rh->addWidget(makeCard(*shown[i]), 1);
            if (i + 1 < shown.size())
                rh->addWidget(makeCard(*shown[i + 1]), 1);
            else
                rh->addStretch(1);
            m_listLayout->addWidget(row);
        }
    }
    m_listLayout->addStretch();
}

void ProjectsPage::refreshStats()
{
    int run = 0, done = 0, wait = 0;
    for (const Project &p : m_projects) {
        if (p.status == "En cours") ++run;
        else if (p.status == kDone) ++done;
        else ++wait;
    }
    m_totalLbl->setText(QString::number(m_projects.size()));
    m_runLbl->setText(QString::number(run));
    m_doneLbl->setText(QString::number(done));
    m_waitLbl->setText(QString::number(wait));
    m_lgRun->setText(QString::number(run));
    m_lgDone->setText(QString::number(done));
    m_lgWait->setText(QString::number(wait));
    m_donut->setValues(run, done, wait);
}

void ProjectsPage::refreshSide()
{
    // projets par équipe
    clearLayout(m_teamLayout);
    QStringList teams = knownTeams();
    QVector<QPair<int, QString>> counts;
    for (const QString &t : teams) {
        int c = 0;
        for (const Project &p : m_projects) if (p.team == t) ++c;
        if (c > 0) counts.push_back(qMakePair(c, t));
    }
    std::stable_sort(counts.begin(), counts.end(),
                     [](const QPair<int, QString> &a, const QPair<int, QString> &b){ return a.first > b.first; });
    int maxc = counts.isEmpty() ? 1 : counts.first().first;
    for (int i = 0; i < counts.size() && i < 6; ++i) {
        const TeamStyle ts = teamStyle(counts[i].second);
        QWidget *row = new QWidget;
        QHBoxLayout *h = new QHBoxLayout(row);
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(10);
        QLabel *ic = new QLabel(ts.icon);
        ic->setFixedSize(30, 30);
        ic->setAlignment(Qt::AlignCenter);
        ic->setStyleSheet(QString("background:%1;border-radius:15px;font-size:13px;color:white;").arg(ts.color));
        QLabel *nm = new QLabel(counts[i].second);
        nm->setMinimumWidth(90);
        QProgressBar *bar = new QProgressBar;
        bar->setRange(0, maxc);
        bar->setValue(counts[i].first);
        bar->setTextVisible(false);
        bar->setStyleSheet(QString("QProgressBar::chunk{background:%1;border-radius:3px;}").arg(ts.color));
        QLabel *n = new QLabel(QString::number(counts[i].first));
        n->setMinimumWidth(14);
        h->addWidget(ic);
        h->addWidget(nm);
        h->addWidget(bar, 1);
        h->addWidget(n);
        m_teamLayout->addWidget(row);
    }

    // activités
    clearLayout(m_actLayout);
    const int n = qMin(5, m_activities.size());
    for (int i = 0; i < n; ++i) {
        const Activity &a = m_activities[i];
        QWidget *row = new QWidget;
        QHBoxLayout *h = new QHBoxLayout(row);
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(8);
        QLabel *av = new QLabel(QString::fromUtf8("👤"));
        av->setFixedSize(30, 30);
        av->setAlignment(Qt::AlignCenter);
        av->setStyleSheet("background:#3a3cd6;border-radius:15px;font-size:13px;");
        QLabel *tx = new QLabel("<b>" + a.who.toHtmlEscaped() + "</b> " + a.text.toHtmlEscaped());
        tx->setWordWrap(true);
        tx->setStyleSheet("font-size:11px;");
        QLabel *tm = new QLabel(a.when);
        tm->setObjectName("muted");
        h->addWidget(av);
        h->addWidget(tx, 1);
        h->addWidget(tm);
        m_actLayout->addWidget(row);
    }
    m_actLayout->addStretch();
}

void ProjectsPage::addActivity(const QString &text)
{
    Activity a;
    a.who = "Admin";
    a.text = text;
    a.when = QString::fromUtf8("à l'instant");
    m_activities.prepend(a);
    ++m_notif;
}

void ProjectsPage::showAllActivities()
{
    QDialog d(this);
    d.setWindowTitle(QString::fromUtf8("Toutes les activités"));
    d.resize(460, 420);
    QVBoxLayout *v = new QVBoxLayout(&d);
    QListWidget *list = new QListWidget;
    for (const Activity &a : m_activities)
        list->addItem(a.who + " " + a.text + "   (" + a.when + ")");
    v->addWidget(list);
    QPushButton *close = new QPushButton("Fermer");
    v->addWidget(close, 0, Qt::AlignRight);
    connect(close, &QPushButton::clicked, &d, &QDialog::accept);
    d.exec();
}

bool ProjectsPage::projectDialog(Project &pr, const QString &title)
{
    QDialog d(this);
    d.setWindowTitle(title);
    d.setMinimumWidth(460);

    QLineEdit *name = new QLineEdit(pr.name);
    QLineEdit *desc = new QLineEdit(pr.desc);
    QComboBox *team = new QComboBox;
    team->setEditable(true);
    team->addItems(knownTeams());
    team->setCurrentText(pr.team);
    QComboBox *status = new QComboBox;
    status->addItems({ "En cours", kDone, "En attente" });
    status->setCurrentText(pr.status);
    QSpinBox *members = new QSpinBox;
    members->setRange(1, 50);
    members->setValue(pr.members);
    QSpinBox *progress = new QSpinBox;
    progress->setRange(0, 100);
    progress->setSuffix(" %");
    progress->setValue(pr.progress);
    QDateEdit *start = new QDateEdit(pr.start);
    start->setCalendarPopup(true);
    start->setDisplayFormat("dd/MM/yyyy");
    QDateEdit *end = new QDateEdit(pr.end);
    end->setCalendarPopup(true);
    end->setDisplayFormat("dd/MM/yyyy");

    QFormLayout *form = new QFormLayout;
    form->setSpacing(10);
    form->addRow("Nom du projet *", name);
    form->addRow("Description", desc);
    form->addRow(QString::fromUtf8("Équipe *"), team);
    form->addRow("Statut", status);
    form->addRow("Membres", members);
    form->addRow("Avancement", progress);
    form->addRow(QString::fromUtf8("Date de début"), start);
    form->addRow("Date de fin", end);

    QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    box->button(QDialogButtonBox::Ok)->setObjectName("primary");
    box->button(QDialogButtonBox::Ok)->setText("Enregistrer");
    box->button(QDialogButtonBox::Cancel)->setText("Annuler");

    QVBoxLayout *v = new QVBoxLayout(&d);
    v->addLayout(form);
    v->addSpacing(8);
    v->addWidget(box);

    connect(box, &QDialogButtonBox::rejected, &d, &QDialog::reject);
    connect(box, &QDialogButtonBox::accepted, &d, [&]{
        if (name->text().trimmed().isEmpty()) {
            QMessageBox::warning(&d, "Champ obligatoire", "Le nom du projet est obligatoire.");
            name->setFocus();
            return;
        }
        if (team->currentText().trimmed().isEmpty()) {
            QMessageBox::warning(&d, "Champ obligatoire", QString::fromUtf8("L'équipe est obligatoire."));
            team->setFocus();
            return;
        }
        if (end->date() < start->date()) {
            QMessageBox::warning(&d, "Dates invalides",
                                 QString::fromUtf8("La date de fin doit être après la date de début."));
            return;
        }
        d.accept();
    });

    if (d.exec() != QDialog::Accepted)
        return false;

    pr.name = name->text().trimmed();
    pr.desc = desc->text().trimmed();
    pr.team = team->currentText().trimmed();
    pr.status = status->currentText();
    pr.members = members->value();
    pr.progress = progress->value();
    if (pr.status == kDone) pr.progress = 100;
    pr.start = start->date();
    pr.end = end->date();
    return true;
}

// ajouter



void ProjectsPage::addProject()
{
    Project p;
    p.id = 0;
    p.status = "En cours";
    p.members = 3;
    p.progress = 0;
    p.start = QDate::currentDate();
    p.end = QDate::currentDate().addDays(2);
    p.team = "GreenTeam";
    if (!projectDialog(p, "Nouveau projet"))
        return;

    if (QSqlDatabase::database("hackflow_oracle").isOpen()) {
        QSqlQuery q(QSqlDatabase::database("hackflow_oracle"));
        q.prepare("INSERT INTO INES.PROJET (TITRE, DESCRIPTION, STATUT, DATE_DEBUT, DATE_RENDU, EQUIPE) VALUES (:t,:d,:s,:sd,:ed,:e)");
        q.bindValue(":t", p.name);
        q.bindValue(":d", p.desc);
        q.bindValue(":s", p.status);
        q.bindValue(":sd", p.start);
        q.bindValue(":ed", p.end);
        q.bindValue(":e", p.team);
        if (!q.exec()) {
            QMessageBox::critical(this, "Oracle", q.lastError().text());
            return;
        }
        QSqlQuery idq(QSqlDatabase::database("hackflow_oracle"));
        if (!idq.exec("SELECT INES.SEQ_PROJET.CURRVAL FROM DUAL") || !idq.next()) {
            QMessageBox::critical(this, "Oracle", idq.lastError().text());
            loadProjectsFromDatabase();
            refresh();
            return;
        }
        p.id = idq.value(0).toInt();
    } else {
        p.id = m_nextId++;
    }

    const TeamStyle ts = teamStyle(p.team);
    p.icon = ts.icon;
    p.color = ts.color;
    m_projects.prepend(p);
    addActivity("a créé le projet " + p.name);
    refresh();
}

// modifier



void ProjectsPage::editProject(int id)
{
    const int i = indexOfProject(id);
    if (i < 0) return;
    Project p = m_projects[i];
    if (!projectDialog(p, "Modifier le projet"))
        return;

    if (QSqlDatabase::database("hackflow_oracle").isOpen()) {
        QSqlQuery q(QSqlDatabase::database("hackflow_oracle"));
        q.prepare("UPDATE INES.PROJET SET TITRE=:t, DESCRIPTION=:d, STATUT=:s, DATE_DEBUT=:sd, DATE_RENDU=:ed, EQUIPE=:e WHERE ID_PROJET=:id");
        q.bindValue(":t", p.name);
        q.bindValue(":d", p.desc);
        q.bindValue(":s", p.status);
        q.bindValue(":sd", p.start);
        q.bindValue(":ed", p.end);
        q.bindValue(":e", p.team);
        q.bindValue(":id", id);
        if (!q.exec()) {
            QMessageBox::critical(this, "Oracle", q.lastError().text());
            return;
        }
        if (q.numRowsAffected() == 0) {
            QMessageBox::warning(this, "Oracle", "Aucune ligne modifiée dans la base.");
            return;
        }
    }

    const TeamStyle ts = teamStyle(p.team);
    p.icon = ts.icon;
    p.color = ts.color;
    m_projects[i] = p;
    addActivity("a mis à jour le projet " + p.name);
    refresh();
}

void ProjectsPage::setStatus(int id, const QString &status)
{
    const int i = indexOfProject(id);
    if (i < 0) return;

    if (QSqlDatabase::database("hackflow_oracle").isOpen()) {
        QSqlQuery q(QSqlDatabase::database("hackflow_oracle"));
        q.prepare("UPDATE INES.PROJET SET STATUT=:s WHERE ID_PROJET=:id");
        q.bindValue(":s", status);
        q.bindValue(":id", id);
        if (!q.exec()) {
            QMessageBox::critical(this, "Oracle", q.lastError().text());
            return;
        }
    }

    m_projects[i].status = status;
    if (status == kDone) m_projects[i].progress = 100;
    addActivity("a changé le statut de " + m_projects[i].name + " : " + status);
    refresh();
}

//supprim


void ProjectsPage::deleteProject(int id)
{
    const int i = indexOfProject(id);
    if (i < 0) return;
    const QString name = m_projects[i].name;
    if (QMessageBox::question(this, "Supprimer le projet",
                              "Voulez-vous vraiment supprimer le projet « " + name + " » ?")
        != QMessageBox::Yes)
        return;

    if (QSqlDatabase::database("hackflow_oracle").isOpen()) {
        QSqlQuery q(QSqlDatabase::database("hackflow_oracle"));
        q.prepare("DELETE FROM INES.PROJET WHERE ID_PROJET=:id");
        q.bindValue(":id", id);
        if (!q.exec()) {
            QMessageBox::critical(this, "Oracle", q.lastError().text());
            return;
        }
        if (q.numRowsAffected() == 0) {
            QMessageBox::warning(this, "Oracle", "Projet introuvable dans la base.");
            return;
        }
    }

    m_projects.removeAt(i);
    addActivity("a supprimé le projet " + name);
    refresh();
}

void ProjectsPage::showProject(int id)
{
    const int i = indexOfProject(id);
    if (i < 0) return;
    const Project &p = m_projects[i];
    QMessageBox box(this);
    box.setWindowTitle("Projet : " + p.name);
    box.setIcon(QMessageBox::NoIcon);
    box.setText("<h3>" + p.name.toHtmlEscaped() + "</h3>" + p.desc.toHtmlEscaped());
    box.setInformativeText(
        QString::fromUtf8("Équipe : ") + p.team + "\n"
        + "Statut : " + p.status + "\n"
        + "Membres : " + QString::number(p.members) + "\n"
        + "Avancement : " + QString::number(p.progress) + " %\n"
        + QString::fromUtf8("Période : ") + fmtDate(p.start) + " - " + fmtDate(p.end));
    box.addButton("Fermer", QMessageBox::AcceptRole);
    box.exec();
}
