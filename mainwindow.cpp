#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "participantscontroller.h"
#include "jurycontroller.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPair>
#include <QPixmap>
#include <QIcon>
#include <QPushButton>
#include <QScreen>
#include <QStandardItemModel>
#include <QTimer>
#include <QtMath>

// ===========================================================================
//  Graphiques (anciennement chartwidgets.cpp)
// ===========================================================================
static const QColor kText("#FFFFFF");
static const QColor kMuted("#AEBBDD");
static const QColor kGrid("#2B3A5E");
static const QColor kCard("#1C2740");

ChartBase::ChartBase(QWidget *parent) : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void ChartBase::setData(const ChartData &data)
{
    m_data = data;
    update();
}

void PieChartWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    double total = 0;
    for (const auto &d : m_data) total += d.value;
    if (total <= 0) {
        p.setPen(kText);
        p.drawText(rect(), Qt::AlignCenter, tr("Aucune donnée"));
        return;
    }
    const int side = qMin(height() - 8, int(width() * 0.42));
    QRectF pie(6, (height() - side) / 2.0, side, side);
    double start = 90 * 16;
    QVector<double> mids;
    for (const auto &d : m_data) {
        double span = -360.0 * 16 * d.value / total;
        p.setPen(QPen(kCard, 2));
        p.setBrush(d.color);
        p.drawPie(pie, int(start), int(span));
        mids << (start + span / 2) / 16.0;
        start += span;
    }
    // trou du donut
    const double hole = side * 0.56;
    QRectF inner(pie.center().x() - hole / 2, pie.center().y() - hole / 2, hole, hole);
    p.setPen(Qt::NoPen);
    p.setBrush(kCard);
    p.drawEllipse(inner);
    // pourcentages sur les parts
    QFont f = font(); f.setBold(true); f.setPointSizeF(f.pointSizeF() - 1); p.setFont(f);
    p.setPen(QColor("#101B31"));
    for (int i = 0; i < m_data.size(); ++i) {
        double rad = qDegreesToRadians(mids[i]);
        double r = (side / 2.0 + hole / 2) / 2.0;
        QPointF c = pie.center() + QPointF(qCos(rad), -qSin(rad)) * r;
        p.drawText(QRectF(c.x() - 24, c.y() - 9, 48, 18), Qt::AlignCenter,
                   QString("%1%").arg(qRound(100.0 * m_data[i].value / total)));
    }
    // total au centre
    p.setPen(kText);
    QFont big = font(); big.setPointSize(20); big.setBold(true); p.setFont(big);
    p.drawText(QRectF(inner.left(), inner.center().y() - 26, inner.width(), 30), Qt::AlignCenter, QString::number(int(total)));
    QFont sm = font(); sm.setPointSizeF(8.5); sm.setBold(false); p.setFont(sm);
    p.setPen(kMuted);
    p.drawText(QRectF(inner.left(), inner.center().y() + 4, inner.width(), 16), Qt::AlignCenter, m_caption);

    // legende
    QFont lf = font(); lf.setBold(false); p.setFont(lf);
    const int x = int(pie.right()) + 26;
    const int rowH = 28;
    int y = qMax(6, height() / 2 - m_data.size() * rowH / 2);
    const int pctX = qMax(x + 150, width() - 50);
    for (int i = 0; i < m_data.size(); ++i) {
        p.setBrush(m_data[i].color); p.setPen(Qt::NoPen);
        p.drawRoundedRect(x, y + i * rowH + 4, 13, 13, 3, 3);
        p.setPen(kText);
        p.drawText(x + 22, y + i * rowH + 16, QString("%1 (%2)").arg(m_data[i].label).arg(int(m_data[i].value)));
        p.setPen(kMuted);
        p.drawText(QRectF(pctX, y + i * rowH + 2, 44, 18), Qt::AlignRight | Qt::AlignVCenter,
                   QString("%1%").arg(qRound(100.0 * m_data[i].value / total)));
    }
}

void BarChartWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    if (m_data.isEmpty()) {
        p.setPen(kText);
        p.drawText(rect(), Qt::AlignCenter, tr("Aucune donnée"));
        return;
    }
    double maxV = 1;
    for (const auto &d : m_data) maxV = qMax(maxV, d.value);
    const int maxTick = int(qCeil(maxV));
    const int left = 30, bottom = 24, top = 22, right = 8;
    QRectF plot(left, top, width() - left - right, height() - top - bottom);

    p.setFont(font());
    for (int t = 0; t <= maxTick; ++t) {
        double y = plot.bottom() - plot.height() * t / maxTick;
        p.setPen(QPen(kGrid, 1, Qt::DotLine));
        p.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
        p.setPen(kMuted);
        p.drawText(QRectF(0, y - 9, left - 6, 18), Qt::AlignRight | Qt::AlignVCenter, QString::number(t));
    }
    const double slot = plot.width() / m_data.size();
    const double bw = qMin(56.0, slot * 0.55);
    for (int i = 0; i < m_data.size(); ++i) {
        double h = plot.height() * m_data[i].value / maxTick;
        QRectF r(plot.left() + slot * i + (slot - bw) / 2, plot.bottom() - h, bw, h);
        QPainterPath path;
        path.addRoundedRect(r, 4, 4);
        p.setPen(Qt::NoPen);
        QLinearGradient g(r.topLeft(), r.bottomLeft());
        g.setColorAt(0, m_data[i].color);
        g.setColorAt(1, m_data[i].color.darker(150));
        p.setBrush(g);
        p.drawPath(path);
        QFont f = font(); f.setBold(true); p.setFont(f);
        p.setPen(kText);
        p.drawText(QRectF(r.left() - 10, r.top() - 19, bw + 20, 16), Qt::AlignCenter, QString::number(int(m_data[i].value)));
        f.setBold(false); p.setFont(f);
        p.setPen(kMuted);
        QString lbl = p.fontMetrics().elidedText(m_data[i].label, Qt::ElideRight, int(slot) - 4);
        p.drawText(QRectF(plot.left() + slot * i, plot.bottom() + 4, slot, 18), Qt::AlignCenter, lbl);
    }
}

// ===========================================================================
//  CheckComboBox (anciennement checkcombobox.cpp)
// ===========================================================================
CheckComboBox::CheckComboBox(QWidget *parent) : QComboBox(parent)
{
    m_model = new QStandardItemModel(this);
    setModel(m_model);
    setEditable(true);
    lineEdit()->setReadOnly(true);
    lineEdit()->setPlaceholderText("Sélectionner une ou plusieurs équipes");
    view()->viewport()->installEventFilter(this);
    lineEdit()->installEventFilter(this);
    connect(m_model, &QStandardItemModel::itemChanged, this, [this]() { updateText(); });
}

void CheckComboBox::addItem_(const QString &t)
{
    auto *it = new QStandardItem(t);
    it->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
    it->setData(Qt::Unchecked, Qt::CheckStateRole);
    m_model->appendRow(it);
}

void CheckComboBox::setItems(const QStringList &items)
{
    m_model->clear();
    for (const QString &t : items) addItem_(t);
    updateText();
}

QStringList CheckComboBox::checkedItems() const
{
    QStringList out;
    for (int i = 0; i < m_model->rowCount(); ++i)
        if (m_model->item(i)->checkState() == Qt::Checked) out << m_model->item(i)->text();
    return out;
}

void CheckComboBox::setCheckedItems(const QStringList &items)
{
    m_model->blockSignals(true);
    for (int i = 0; i < m_model->rowCount(); ++i) m_model->item(i)->setCheckState(Qt::Unchecked);
    for (const QString &t : items) {
        bool found = false;
        for (int i = 0; i < m_model->rowCount(); ++i)
            if (m_model->item(i)->text().compare(t, Qt::CaseInsensitive) == 0) {
                m_model->item(i)->setCheckState(Qt::Checked); found = true; break;
            }
        if (!found) { addItem_(t); m_model->item(m_model->rowCount() - 1)->setCheckState(Qt::Checked); }
    }
    m_model->blockSignals(false);
    updateText();
}

void CheckComboBox::clearChecked() { setCheckedItems(QStringList()); }

void CheckComboBox::updateText()
{
    lineEdit()->setText(checkedItems().join(", "));
}

bool CheckComboBox::eventFilter(QObject *obj, QEvent *ev)
{
    if (obj == view()->viewport() && ev->type() == QEvent::MouseButtonRelease) {
        QModelIndex idx = view()->indexAt(static_cast<QMouseEvent *>(ev)->pos());
        if (idx.isValid()) {
            auto *it = m_model->itemFromIndex(idx);
            it->setCheckState(it->checkState() == Qt::Checked ? Qt::Unchecked : Qt::Checked);
            return true;     // garde la liste ouverte
        }
    }
    if (obj == lineEdit() && ev->type() == QEvent::MouseButtonRelease) {
        showPopup();
        return true;
    }
    return QComboBox::eventFilter(obj, ev);
}

// ===========================================================================
//  MainWindow (anciennement gsmarthackathon.cpp)
// ===========================================================================
// ---------------------------------------------------------------------------
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->labelBreadcrumbParticipants->setText(
        QStringLiteral("›  SmartHackathon Management  ›  Gestion des Participants"));
    if (ui->btnNavEquipes)
        ui->btnNavEquipes->hide();
    setupNavigation();
    setupDecor();
    setupMenu();                // menu d'accueil (module "menu")
    m_participants = new ParticipantsController(ui, this);   // page Participants (module integre)
    m_jury = new JuryController(ui, this);                   // page Jury (module integre)
    showConnexion();            // l'application demarre sur l'ecran de connexion
}

MainWindow::~MainWindow()
{
    delete m_participants;      // avant ui : le module utilise les widgets de ui
    m_participants = nullptr;
    delete m_jury;              // idem : avant ui
    m_jury = nullptr;
    delete ui;
}

// ---------------------------------------------------------------------------
//  NAVIGATION : seule partie fonctionnelle de cette phase
// ---------------------------------------------------------------------------
void MainWindow::setupNavigation()
{
    // Connexion -> menu d'accueil (ecran seul)
    connect(ui->btnLogin, &QPushButton::clicked, this, [this]() { showMenu(); });
    // application -> Connexion
    connect(ui->btnLogout, &QPushButton::clicked, this, [this]() { showConnexion(); });
    // "Accueil" du menu de gauche -> retour au menu d'accueil
    connect(ui->btnNavAccueil, &QPushButton::clicked, this, [this]() { showMenu(); });

    // bouton du menu -> index de la page dans stackedPages (meme ordre que le .ui)
    const QList<QPair<QPushButton *, int>> nav = {
        { ui->btnNavParticipants, 1 },
        { ui->btnNavProjets,      3 },
        { ui->btnNavEmployes,     5 },   // notre module
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

void MainWindow::showConnexion()
{
    m_expanded = false;
    ui->stackedMain->setCurrentWidget(ui->pageConnexion);
    setFixedSize(960, 640);                         // meme taille que la maquette de connexion
    centerOnScreen(this);
}

// Fenetre agrandie et centree. Faite une seule fois apres la connexion : passer du menu
// a l'application (ou inversement) ne change plus la taille choisie par l'utilisateur.
void MainWindow::expandWindow()
{
    if (m_expanded)
        return;
    m_expanded = true;
    setMinimumSize(0, 0);                           // le minimum est calcule par les layouts
    setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    const QRect a = (screen() ? screen() : QGuiApplication::primaryScreen())->availableGeometry();
    const QSize want(qMin(1520, a.width() - 40), qMin(900, a.height() - 60));
    resize(want.expandedTo(minimumSizeHint()));
    centerOnScreen(this);
}

// Connexion -> menu d'accueil : ecran seul, sans barre laterale (comme la page de connexion)
void MainWindow::showMenu()
{
    ui->stackedMain->setCurrentWidget(ui->pageMenu);
    expandWindow();
}

// Menu -> application : barre laterale + pages, ouverte sur le module choisi.
// Le clic simule celui du menu de gauche : la page change ET l'entree est mise en surbrillance.
void MainWindow::showApplication(QPushButton *nav)
{
    ui->stackedMain->setCurrentWidget(ui->pageApp);
    expandWindow();
    nav->click();
}

// ---------------------------------------------------------------------------
//  MENU D'ACCUEIL (module "menu") : page pageMenu de stackedMain (comme la Connexion, SANS
//  barre laterale), affichee juste apres "Se connecter". Les 4 cartes ouvrent l'application
//  sur le module correspondant (showApplication).
//  Images : :/images/ (dossier images/). Style limite a pageMenu / widgetMenuAccueil.
// ---------------------------------------------------------------------------
void MainWindow::setupMenu()
{
    // "Admin" repris de la carte utilisateur de la barre laterale
    ui->labelWelcome->setText(
        QStringLiteral("Bienvenue, <span style='color:#38bdf8;'>%1</span> !")
            .arg(ui->labelAdminName->text().toHtmlEscaped()));

    // Logo de la banniere
    const QPixmap logo(":/images/logo.png");
    if (!logo.isNull())
        ui->labelLogoImg->setPixmap(logo.scaled(56, 56, Qt::KeepAspectRatio, Qt::SmoothTransformation));

    // Illustration de la banniere
    const QPixmap hero(":/images/hero.png");
    if (!hero.isNull())
        ui->labelIllustration->setPixmap(hero.scaledToHeight(250, Qt::SmoothTransformation));
    else
        ui->labelIllustration->setText(QStringLiteral("💡"));

    // Icones des cartes
    auto setIcon = [](QLabel *l, const QString &path) {
        l->setPixmap(QPixmap(path).scaled(26, 26, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        l->setAlignment(Qt::AlignCenter);
    };
    setIcon(ui->iconParticipants, ":/images/icon_participants.png");
    setIcon(ui->iconProjets,      ":/images/icon_projets.png");
    setIcon(ui->iconEmployes,     ":/images/icon_employes.png");
    setIcon(ui->iconJury,         ":/images/icon_jury.png");

    // Titre "Actions rapides" avec l'eclair
    ui->labelQuick->setText(
        QStringLiteral("<img src=':/images/zap.png' width='18' height='18'> &nbsp;Actions rapides"));

    // Fond de l'ecran du menu (celui de la fenetre du projet d'origine)
    ui->pageMenu->setStyleSheet(QStringLiteral(
        "QWidget#pageMenu { background: qlineargradient(x1:0, y1:0, x2:1, y2:1,"
        " stop:0 #0b1030, stop:1 #050818); }"));

    // Style du menu (selecteurs #id : aucun effet sur le reste de l'application)
    ui->widgetMenuAccueil->setStyleSheet(QStringLiteral(R"(
        QLabel { color: white; background: transparent; }

        /* ---------- Banniere ---------- */
        #frameHero {
            border: 1px solid #1e3a8a;
            border-radius: 18px;
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                        stop:0 #0c1a4a, stop:0.6 #141a6b, stop:1 #1b1f8a);
        }
        #labelLogoImg { background: transparent; border-radius: 10px; }
        #labelLogoMenu    { font-size: 22px; font-weight: bold; color: white; }
        #labelTaglineMenu { font-size: 8px; font-weight: bold; color: #22d3ee; letter-spacing: 1px; }
        #labelWelcome { font-size: 30px; font-weight: bold; color: white; }
        #labelSubtitle{ font-size: 13px; color: #e5e7eb; }
        #labelDesc    { font-size: 11px; color: #9ca3af; }
        #labelIllustration { font-size: 90px; }

        /* ---------- Titre section ---------- */
        #labelQuick { font-size: 15px; font-weight: bold; color: #e5e7eb; }

        /* ---------- Cartes ---------- */
        #cardParticipants, #cardProjets, #cardEmployes, #cardJury {
            border-radius: 16px;
        }
        #cardParticipants {
            border: 1px solid #6366f1;
            background: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #3b2a9c, stop:1 #1b1f6b);
        }
        #cardProjets {
            border: 1px solid #14b8a6;
            background: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #0f4c5c, stop:1 #12305a);
        }
        #cardEmployes {
            border: 1px solid #a855f7;
            background: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #5b21b6, stop:1 #2e1a7a);
        }
        #cardJury {
            border: 1px solid #3b82f6;
            background: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #1e3a8a, stop:1 #14235e);
        }
        #cardParticipants:hover { border: 2px solid #818cf8; }
        #cardProjets:hover      { border: 2px solid #2dd4bf; }
        #cardEmployes:hover     { border: 2px solid #c084fc; }
        #cardJury:hover         { border: 2px solid #60a5fa; }

        #iconParticipants, #iconProjets, #iconEmployes, #iconJury {
            font-size: 22px; background: rgba(255,255,255,0.12);
            border-radius: 12px; min-width: 48px; max-width: 48px;
            min-height: 48px; max-height: 48px; qproperty-alignment: AlignCenter;
        }
        #titleParticipants, #titleProjets, #titleEmployes, #titleJury {
            font-size: 15px; font-weight: bold; color: white;
        }
        #descParticipants, #descProjets, #descEmployes, #descJury {
            font-size: 10px; color: #cbd5e1;
        }

        /* ---------- Boutons fleche ronds (padding:0 : annule le style global des QPushButton) ---------- */
        #btnParticipants, #btnProjets, #btnEmployes, #btnJury {
            color: white; font-size: 16px; font-weight: bold;
            border: none; border-radius: 18px; padding: 0px;
            background: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #2563eb, stop:1 #4f46e5);
        }
        #btnParticipants:hover, #btnProjets:hover,
        #btnEmployes:hover, #btnJury:hover { background: #60a5fa; border: none; }
    )"));

    // Cartes -> application ouverte sur le module choisi
    connect(ui->btnParticipants, &QPushButton::clicked, this, [this]() { showApplication(ui->btnNavParticipants); });
    connect(ui->btnProjets,      &QPushButton::clicked, this, [this]() { showApplication(ui->btnNavProjets); });
    connect(ui->btnEmployes,     &QPushButton::clicked, this, [this]() { showApplication(ui->btnNavEmployes); });
    connect(ui->btnJury,         &QPushButton::clicked, this, [this]() { showApplication(ui->btnNavJury); });
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

void MainWindow::setupDecor()
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

void MainWindow::positionHeaderCheck()
{
    auto *h = ui->tableEmployes->horizontalHeader();
    const QSize s = m_headCheck->sizeHint();
    m_headCheck->move(h->sectionViewportPosition(0) + (h->sectionSize(0) - s.width()) / 2,
                      (h->height() - s.height()) / 2);
}
