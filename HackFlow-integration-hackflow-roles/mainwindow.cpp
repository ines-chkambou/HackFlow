#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "employe.h"
#include "participantscontroller.h"
#include "jurycontroller.h"
#include "projectspage.h"

#include <QFileDialog>
#include <QTextStream>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QRegularExpression>

#include <QAbstractItemView>
#include <QAction>
#include <QGraphicsOpacityEffect>
#include <QMenu>
#include <QMessageBox>
#include <QSettings>
#include <QStyle>
#include <QToolButton>
#include <QBoxLayout>
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
#include <QRadialGradient>
#include <QLinearGradient>
#include <QFontMetrics>
#include <QShortcut>
#include <QPushButton>
#include <QScreen>
#include <QStandardItemModel>
#include <QTimer>
#include <QtMath>
#include <QFormLayout>
#include <QSpinBox>
#include <QProgressBar>
#include <QInputDialog>
#include <QRegularExpressionValidator>
#include <QtConcurrent/QtConcurrent>
#include <QCryptographicHash>
#include <QRandomGenerator>

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
    setupNavigation();
    setupRoleMenu();            // page "Choix du role" (apres la connexion)
    setupDecor();
    setupAuth();                // authentification, privileges, e-mail, CV (apres setupDecor : tableau cree)
    setupEmployeCrud();         // CRUD et gestion des employes (Base de donnees Oracle)
    m_participants = new ParticipantsController(ui, this);   // page Participants (module integre)
    m_jury = new JuryController(ui, this);                   // page Jury (module integre)
    m_projects = new ProjectsPage(ui->pageProjets);          // page Projets (module integre)
    ui->layoutPageProjets->addWidget(m_projects, 1);
    // "Nouveau projet" dans l'en-tete de la page (entre le titre et le fil d'Ariane)
    if (auto *head = qobject_cast<QBoxLayout *>(ui->layoutTitleProjets))
        head->insertWidget(2, m_projects->newProjectButton());
    // barre de recherche du haut : filtre les projets quand la page Projets est affichee
    connect(ui->lineTopSearch, &QLineEdit::textChanged, this, [this](const QString &t) {
        if (ui->stackedPages->currentWidget() == ui->pageProjets) m_projects->setSearchText(t);
    });
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
    // Connexion (verification du compte : doLogin) -> Choix du role (Participant / Jury / Employe) -> application
    connect(ui->btnLogin, &QPushButton::clicked, this, [this]() { doLogin(); });
    // application -> Connexion
    connect(ui->btnLogout, &QPushButton::clicked, this, [this]() { doLogout(); });

    // bouton du menu -> page de stackedPages (par widget : independant de l'ordre des pages)
    const QList<QPair<QPushButton *, QWidget *>> nav = {
        { ui->btnNavAccueil,      ui->pageAccueil },       // Accueil / Tableau de bord
        { ui->btnNavParticipants, ui->pageParticipants },
        { ui->btnNavProjets,      ui->pageProjets },
        { ui->btnNavEmployes,     ui->pageEmployes },      // notre module
        { ui->btnNavJury,         ui->pageJury },
        { ui->btnNavParametres,   ui->pageParametres }
    };
    for (const auto &p : nav) {
        QWidget *page = p.second;
        QPushButton *button = p.first;
        connect(p.first, &QPushButton::clicked, this, [this, page, button]() {
            if (button->isHidden()) return;     // module interdit pour ce compte (applyPrivileges)
            ui->stackedPages->setCurrentWidget(page);
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
    ui->stackedMain->setCurrentIndex(0);
    setFixedSize(960, 640);                         // meme taille que la maquette de connexion
    centerOnScreen(this);
}

void MainWindow::showRoles()
{
    // aucune fonction "Employe" selectionnee a l'arrivee sur la page
    const QList<QPushButton *> subs = { ui->btnSubOrganisateur, ui->btnSubMentor, ui->btnSubTechnicien };
    for (QPushButton *b : subs) { b->setAutoExclusive(false); b->setChecked(false); b->setAutoExclusive(true); }
    ui->labelHintEmploye->hide();

    ui->stackedMain->setCurrentIndex(2);
    setMinimumSize(0, 0);
    setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    const QRect a = (screen() ? screen() : QGuiApplication::primaryScreen())->availableGeometry();
    const QSize want(qMin(1180, a.width() - 40), qMin(820, a.height() - 60));
    resize(want.expandedTo(minimumSizeHint()));
    centerOnScreen(this);
}

void MainWindow::showApplication(Role role)
{
    applyRole(role);
    ui->stackedMain->setCurrentIndex(1);
    ui->stackedPages->setCurrentWidget(ui->pageAccueil);   // Choix du role -> Tableau de bord
    ui->btnNavAccueil->setChecked(true);
    setMinimumSize(0, 0);                           // le minimum est calcule par les layouts
    setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    const QRect a = (screen() ? screen() : QGuiApplication::primaryScreen())->availableGeometry();
    const QSize want(qMin(1520, a.width() - 40), qMin(900, a.height() - 60));
    resize(want.expandedTo(minimumSizeHint()));
    centerOnScreen(this);
}

// ---------------------------------------------------------------------------
//  Role choisi : tous les modules du menu restent accessibles pour tous les roles.
// ---------------------------------------------------------------------------
void MainWindow::applyRole(Role role)
{
    m_role = role;
    QString name = "Organisateur";
    switch (role) {
    case Role::Participant:  name = "Participant";    break;
    case Role::Jury:         name = "Membre du jury"; break;
    case Role::Organisateur: name = "Organisateur";   break;
    case Role::Mentor:       name = "Mentor";         break;
    case Role::Technicien:   name = "Technicien";     break;
    }
    // Quel que soit le role (Participant, Jury, Employe), TOUS les modules du menu sont accessibles.
    const QList<QPushButton *> allModules = { ui->btnNavAccueil, ui->btnNavParticipants, ui->btnNavProjets,
                                              ui->btnNavEmployes, ui->btnNavJury, ui->btnNavParametres };
    for (QPushButton *b : allModules) { b->setVisible(true); b->setEnabled(true); }

    // carte de l'utilisateur dans la barre laterale / en haut a droite
    ui->labelAdminName->setText(name);
    ui->btnAdmin->setText(name + "  ▾\n" + (role == Role::Organisateur ? "Administrateur" : "Connecté"));

    // compte connecte (nom, e-mail) + privileges du role du compte (authmanager.h)
    applySession(name);
}

void MainWindow::enterAsEmploye()
{
    if (ui->btnSubOrganisateur->isChecked())      showApplication(Role::Organisateur);
    else if (ui->btnSubMentor->isChecked())       showApplication(Role::Mentor);
    else if (ui->btnSubTechnicien->isChecked())   showApplication(Role::Technicien);
    else                                          ui->labelHintEmploye->show();   // aucune fonction choisie
}

// ===========================================================================
//  Page "Choix du role" : fond, titre en degrade, icones, signaux
// ===========================================================================
void RolesBackdrop::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const double w = width(), h = height();

    QLinearGradient bg(0, 0, 0, h);
    bg.setColorAt(0.0, QColor("#0B1433"));
    bg.setColorAt(0.55, QColor("#08102B"));
    bg.setColorAt(1.0, QColor("#060B20"));
    p.fillRect(rect(), bg);

    auto glow = [&p](const QPointF &c, double r, QColor col, int alpha) {
        QRadialGradient g(c, r);
        col.setAlpha(alpha); g.setColorAt(0.0, col);
        col.setAlpha(0);     g.setColorAt(1.0, col);
        p.setPen(Qt::NoPen); p.setBrush(g);
        p.drawEllipse(c, r, r);
    };
    glow(QPointF(w * 0.04, h * 0.06), w * 0.30, QColor("#7C4DFF"), 46);    // halo violet (haut gauche)
    glow(QPointF(w * 0.97, h * 0.08), w * 0.24, QColor("#0BF8F6"), 40);    // halo cyan (haut droite)

    auto orbit = [&](const QPointF &c, double r, double angle, QColor col) {
        col.setAlpha(150);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(col, 1.4));
        p.drawEllipse(c, r, r);
        const QPointF d = c + QPointF(qCos(angle), qSin(angle)) * r;       // point lumineux sur l'orbite
        glow(d, 16, col, 120);
        col.setAlpha(255);
        p.setPen(Qt::NoPen); p.setBrush(col);
        p.drawEllipse(d, 4.5, 4.5);
    };
    orbit(QPointF(-w * 0.05, -h * 0.13), w * 0.27, 0.85, QColor("#8B5CF6"));
    orbit(QPointF(w * 1.085, h * 0.09),  w * 0.205, 2.63, QColor("#0BF8F6"));

    const struct { double x, y, r; const char *c; } dots[] = {
        { 0.050, 0.235, 2.2, "#8B5CF6" }, { 0.860, 0.113, 2.4, "#8B5CF6" },
        { 0.827, 0.135, 1.6, "#0BF8F6" }, { 0.873, 0.183, 1.4, "#0BF8F6" }
    };
    p.setPen(Qt::NoPen);
    for (const auto &d : dots) { p.setBrush(QColor(d.c)); p.drawEllipse(QPointF(w * d.x, h * d.y), d.r, d.r); }
}

GradientTitle::GradientTitle(QWidget *parent) : QWidget(parent)
{
    m_plain = "Choisissez ";
    m_accent = "votre rôle";
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

void GradientTitle::setParts(const QString &plain, const QString &accent)
{
    m_plain = plain; m_accent = accent;
    updateGeometry(); update();
}

QFont GradientTitle::titleFont() const
{
    QFont f("Segoe UI");
    f.setPointSizeF(29);
    f.setBold(true);
    return f;
}

QSize GradientTitle::sizeHint() const
{
    const QFontMetrics fm(titleFont());
    return QSize(fm.horizontalAdvance(m_plain + m_accent) + 8, fm.height() + 10);
}

void GradientTitle::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    const QFont f = titleFont();
    const QFontMetrics fm(f);
    const int wPlain = fm.horizontalAdvance(m_plain), wAcc = fm.horizontalAdvance(m_accent);
    const double x0 = (m_align & Qt::AlignLeft) ? 2.0 : (width() - wPlain - wAcc) / 2.0;
    const double base = (height() - fm.height()) / 2.0 + fm.ascent();

    QPainterPath t1;  t1.addText(x0, base, f, m_plain);
    p.fillPath(t1, QBrush(QColor("#FFFFFF")));

    QPainterPath t2;  t2.addText(x0 + wPlain, base, f, m_accent);
    QLinearGradient g(x0 + wPlain, 0, x0 + wPlain + wAcc, 0);
    g.setColorAt(0.0, QColor("#2F8BFF"));
    g.setColorAt(1.0, QColor("#8B5CF6"));
    p.fillPath(t2, QBrush(g));
}

namespace {
const qreal kDpr = 2.0;

// icone blanche des ressources recoloree (garde seulement la transparence)
QPixmap tintedIcon(const QString &res, const QColor &c, int px)
{
    QPixmap src(res);
    if (src.isNull()) return QPixmap();
    QPixmap out = src.scaled(int(px * kDpr), int(px * kDpr), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QPainter p(&out);
    p.setCompositionMode(QPainter::CompositionMode_SourceIn);
    p.fillRect(out.rect(), c);
    p.end();
    out.setDevicePixelRatio(kDpr);
    return out;
}

// image des ressources, redimensionnee nettement (ecrans HiDPI)
QPixmap smoothPixmap(const QString &res, int px)
{
    QPixmap src(res);
    if (src.isNull()) return QPixmap();
    QPixmap out = src.scaled(int(px * kDpr), int(px * kDpr), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    out.setDevicePixelRatio(kDpr);
    return out;
}

// mallette (Employe) dessinee dans le meme style fin que les icones du projet
QPixmap briefcaseIcon(const QColor &c, int px)
{
    QPixmap pm(int(px * kDpr), int(px * kDpr));
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.scale(px * kDpr / 24.0, px * kDpr / 24.0);
    QPen pen(c, 1.9); pen.setCapStyle(Qt::RoundCap); pen.setJoinStyle(Qt::RoundJoin);
    p.setPen(pen); p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(QRectF(2.5, 7.5, 19, 13), 2.5, 2.5);
    QPainterPath handle;
    handle.moveTo(8.5, 7.5); handle.lineTo(8.5, 5.5); handle.quadTo(8.5, 3.5, 10.5, 3.5);
    handle.lineTo(13.5, 3.5); handle.quadTo(15.5, 3.5, 15.5, 5.5); handle.lineTo(15.5, 7.5);
    p.drawPath(handle);
    p.drawLine(QPointF(2.5, 13.5), QPointF(21.5, 13.5));
    p.setBrush(QColor("#0A1230"));
    p.drawRoundedRect(QRectF(10.3, 12.1, 3.4, 3.0), 0.8, 0.8);
    p.end();
    pm.setDevicePixelRatio(kDpr);
    return pm;
}

// cadenas ouvert (barre "Acces securise")
QPixmap lockOpenIcon(const QColor &c, int px)
{
    QPixmap pm(int(px * kDpr), int(px * kDpr));
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.scale(px * kDpr / 24.0, px * kDpr / 24.0);
    QPen pen(c, 1.8); pen.setCapStyle(Qt::RoundCap); pen.setJoinStyle(Qt::RoundJoin);
    p.setPen(pen); p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(QRectF(4.5, 11, 15, 10), 2.5, 2.5);
    QPainterPath shackle;
    shackle.moveTo(8, 11); shackle.lineTo(8, 7);
    shackle.arcTo(QRectF(8, 2, 8, 10), 180, -150);
    p.drawPath(shackle);
    p.setBrush(c);
    p.drawEllipse(QPointF(12, 16), 1.2, 1.2);
    p.end();
    pm.setDevicePixelRatio(kDpr);
    return pm;
}
} // namespace

void MainWindow::setupRoleMenu()
{
    ui->titleRoles->setParts("Choisissez ", "votre rôle");
    ui->titleRoles->setAlignment(Qt::AlignLeft);        // bloc d'en-tete (logo, titre, sous-titre) a gauche
    ui->labelLogoRoles->setPixmap(smoothPixmap(":/img/login_logo.png", 76));
    QFont tf = ui->labelTaglineRoles->font();
    tf.setLetterSpacing(QFont::AbsoluteSpacing, 1.2);
    ui->labelTaglineRoles->setFont(tf);

    // icones des 3 profils (memes icones que le reste de l'application)
    ui->iconParticipant->setPixmap(tintedIcon(":/part/users-round.png", QColor("#2DD4BF"), 46));
    ui->iconJury->setPixmap(tintedIcon(":/icons/ti_award.png", QColor("#A78BFA"), 46));
    ui->iconEmploye->setPixmap(briefcaseIcon(QColor("#3B82F6"), 46));

    // coches des listes
    const QPixmap check = smoothPixmap(":/icons/check_w.png", 12);
    const QList<QLabel *> tealChecks = { ui->chkParticipant1, ui->chkParticipant2, ui->chkParticipant3, ui->chkParticipant4,
                                         ui->chkJury1, ui->chkJury2, ui->chkJury3, ui->chkJury4 };
    for (QLabel *l : tealChecks) { l->setPixmap(check); l->setAlignment(Qt::AlignCenter); }

    // fleches des boutons
    const QPixmap arrow = tintedIcon(":/part/chevron-right.png", QColor("#FFFFFF"), 16);
    ui->labelBtnArrowParticipant->setPixmap(arrow);
    ui->labelBtnArrowJury->setPixmap(arrow);
    ui->labelBtnArrowEmploye->setPixmap(arrow);
    const QPixmap rowArrow = tintedIcon(":/part/chevron-right.png", QColor("#E6ECFF"), 14);
    ui->labelSubArrowOrganisateur->setPixmap(rowArrow);
    ui->labelSubArrowMentor->setPixmap(rowArrow);
    ui->labelSubArrowTechnicien->setPixmap(rowArrow);

    // icones des fonctions de l'employe
    ui->iconSubOrganisateur->setPixmap(tintedIcon(":/part/user.png", QColor("#4C8DFF"), 20));
    ui->iconSubMentor->setPixmap(tintedIcon(":/part/user.png", QColor("#B79CFF"), 20));
    ui->iconSubTechnicien->setPixmap(tintedIcon(":/part/user.png", QColor("#2DD4BF"), 20));
    for (QLabel *l : { ui->iconSubOrganisateur, ui->iconSubMentor, ui->iconSubTechnicien })
        l->setAlignment(Qt::AlignCenter);

    // barre d'information
    ui->labelRolesInfoIcon->setPixmap(tintedIcon(":/part/shield-check.png", QColor("#8B5CF6"), 36));
    ui->labelRolesInfoLock->setPixmap(lockOpenIcon(QColor("#3B6FE0"), 30));

    // les textes / icones a l'interieur des boutons ne volent pas le clic
    const QList<QPushButton *> allBtns = { ui->btnRoleParticipant, ui->btnRoleJury, ui->btnRoleEmploye,
                                           ui->btnSubOrganisateur, ui->btnSubMentor, ui->btnSubTechnicien };
    for (QPushButton *b : allBtns)
        for (QLabel *l : b->findChildren<QLabel *>()) l->setAttribute(Qt::WA_TransparentForMouseEvents);

    // navigation
    connect(ui->btnRoleParticipant, &QPushButton::clicked, this, [this]() { showApplication(Role::Participant); });
    connect(ui->btnRoleJury,        &QPushButton::clicked, this, [this]() { showApplication(Role::Jury); });
    connect(ui->btnRoleEmploye,     &QPushButton::clicked, this, [this]() { enterAsEmploye(); });
    const QList<QPushButton *> subs = { ui->btnSubOrganisateur, ui->btnSubMentor, ui->btnSubTechnicien };
    for (QPushButton *b : subs)
        connect(b, &QPushButton::clicked, this, [this]() { ui->labelHintEmploye->hide(); });

    // Echap : retour a la connexion
    auto *esc = new QShortcut(QKeySequence(Qt::Key_Escape), ui->pageRoles);
    connect(esc, &QShortcut::activated, this, [this]() {
        if (ui->stackedMain->currentIndex() == 2) doLogout();
    });
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

    // case de l'en-tete (affichage uniquement)
    m_headCheck = new QCheckBox(h->viewport());
    m_headCheck->show();
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

// ===========================================================================
//  CRUD & LOGIQUE METIER EMPLOYES (BASE DE DONNEES ORACLE)
// ===========================================================================
void MainWindow::setupEmployeCrud()
{
    ui->comboRole->clear();
    ui->comboRole->addItems({ "Organisateur", "Technicien", "Mentor" });
    ui->comboDept->clear();
    ui->comboDept->addItems({ "Logistique", "Technique", "Pédagogie/Mentorat" });
    ui->comboDispo->clear();
    ui->comboDispo->addItems({ "Disponible", "Indisponible" });

    ui->comboFilterRole->clear();
    ui->comboFilterRole->addItems({ "Tous les rôles", "Organisateur", "Technicien", "Mentor" });
    ui->comboFilterDept->clear();
    ui->comboFilterDept->addItems({ "Tous les départements", "Logistique", "Technique", "Pédagogie/Mentorat" });

    ui->lineId->setReadOnly(true);
    ui->lineId->setFocusPolicy(Qt::NoFocus);
    ui->lineId->setText(QString::number(Employe::prochainId()));
    ui->lineId->setPlaceholderText("ID automatique");
    ui->lineId->setToolTip("Identifiant unique généré automatiquement (non modifiable)");

    connect(ui->btnAjouter, &QPushButton::clicked, this, &MainWindow::onAjouterEmploye);
    connect(ui->btnVider, &QPushButton::clicked, this, &MainWindow::onViderEmploye);

    connect(ui->lineSearch, &QLineEdit::textChanged, this, &MainWindow::onFilterEmployes);
    connect(ui->comboFilterRole, &QComboBox::currentIndexChanged, this, &MainWindow::onFilterEmployes);
    connect(ui->comboFilterDept, &QComboBox::currentIndexChanged, this, &MainWindow::onFilterEmployes);
    connect(ui->checkDispo, &QCheckBox::toggled, this, &MainWindow::onFilterEmployes);

    connect(ui->btnTrier, &QPushButton::clicked, this, &MainWindow::onTrierEmploye);
    connect(ui->btnExporter, &QPushButton::clicked, this, &MainWindow::onExporterEmploye);

    connect(ui->btnProfil, &QPushButton::clicked, this, [this]() {
        int r = ui->tableEmployes->currentRow();
        if (r < 0) {
            QMessageBox::information(this, "Consulter le profil", "Veuillez sélectionner un employé dans le tableau.");
            return;
        }
        fillEmployeForm(r);
    });

    // Bouton de suppression direct sur le formulaire
    m_btnSupprimerForm = new QPushButton("Supprimer", this);
    m_btnSupprimerForm->setObjectName("danger");
    m_btnSupprimerForm->setCursor(Qt::PointingHandCursor);
    m_btnSupprimerForm->setStyleSheet("QPushButton { background-color: #EF4444; color: white; border-radius: 9px; padding: 8px 16px; font-weight: bold; border: none; } QPushButton:hover { background-color: #DC2626; }");
    m_btnSupprimerForm->setToolTip("Supprimer cet employé de la base de données Oracle");
    if (auto *formLayout = qobject_cast<QBoxLayout *>(ui->btnVider->parentWidget() ? ui->btnVider->parentWidget()->layout() : nullptr)) {
        formLayout->addWidget(m_btnSupprimerForm);
    }
    m_btnSupprimerForm->hide();
    connect(m_btnSupprimerForm, &QPushButton::clicked, this, [this]() {
        if (m_editingEmployeId > 0) onSupprimerEmploye(m_editingEmployeId);
    });

    // Raccourci clavier Touche Suppr / Delete sur le tableau des employés
    auto *delKey = new QShortcut(QKeySequence::Delete, ui->tableEmployes);
    connect(delKey, &QShortcut::activated, this, [this]() {
        int r = ui->tableEmployes->currentRow();
        if (r >= 0) {
            int empId = cellText(ui->tableEmployes, r, 1).toInt();
            if (empId > 0) onSupprimerEmploye(empId);
        }
    });

    // Menu contextuel (clic droit) sur le tableau des employés
    ui->tableEmployes->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->tableEmployes, &QTableWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        int r = ui->tableEmployes->rowAt(pos.y());
        if (r < 0) return;
        int empId = cellText(ui->tableEmployes, r, 1).toInt();
        if (empId <= 0) return;
        QMenu menu(this);
        menu.addAction(QIcon(":/icons/eye_b.png"), "Consulter le profil", [this, r]() { fillEmployeForm(r); });
        menu.addAction(QIcon(":/icons/edit_b.png"), "Modifier cet employé", [this, r]() { fillEmployeForm(r); });
        menu.addSeparator();
        menu.addAction(QIcon(":/icons/trash_r.png"), "Supprimer cet employé", [this, empId]() { onSupprimerEmploye(empId); });
        menu.exec(ui->tableEmployes->viewport()->mapToGlobal(pos));
    });

    refreshEmployesTable();
}

void MainWindow::refreshEmployesTable(QSqlQueryModel *customModel)
{
    auto *t = ui->tableEmployes;
    QScopedPointer<QSqlQueryModel> defaultModel;
    QSqlQueryModel *model = customModel;

    if (!model) {
        defaultModel.reset(m_employeModel.afficher());
        model = defaultModel.data();

        // Insertion automatique des comptes de demonstration si la table Oracle est vide
        if (model->rowCount() == 0) {
            const int n = int(sizeof(kRows) / sizeof(kRows[0]));
            for (int i = 0; i < n; ++i) {
                const Row &e = kRows[i];
                Employe emp(QString(e.id).toInt(), e.nom, e.prenom, e.mail, e.role, e.dept, e.equipes, e.dispo);
                emp.ajouter();
            }
            defaultModel.reset(m_employeModel.afficher());
            model = defaultModel.data();
        }
    }

    const int rowCount = model ? model->rowCount() : 0;
    t->setRowCount(rowCount);

    for (int r = 0; r < rowCount; ++r) {
        int empId = model->record(r).value("ID_EMPLOYE").toInt();
        QString nom = model->record(r).value("NOM").toString();
        QString prenom = model->record(r).value("PRENOM").toString();
        QString email = model->record(r).value("EMAIL").toString();
        QString role = model->record(r).value("ROLE").toString();
        QString dept = model->record(r).value("DEPARTEMENT").toString();
        QString equipes = model->record(r).value("EQUIPE").toString();
        QString dispo = model->record(r).value("DISPONIBILITE").toString();

        t->setItem(r, 0, new QTableWidgetItem(""));
        t->setItem(r, 1, new QTableWidgetItem(QString::number(empId)));
        t->setItem(r, 2, new QTableWidgetItem(nom));
        t->setItem(r, 3, new QTableWidgetItem(prenom));
        t->setItem(r, 4, new QTableWidgetItem(email));
        t->setItem(r, 6, new QTableWidgetItem(dept));
        t->setItem(r, 7, new QTableWidgetItem(equipes));

        auto *cw = new QWidget; auto *cl = new QHBoxLayout(cw);
        cl->setContentsMargins(0, 0, 0, 0); cl->setAlignment(Qt::AlignCenter); cl->addWidget(new QCheckBox);
        t->setCellWidget(r, 0, cw);

        auto *roleBadgeLbl = new QLabel(role); roleBadgeLbl->setAlignment(Qt::AlignCenter);
        roleBadgeLbl->setStyleSheet(roleBadge(role));
        t->setCellWidget(r, 5, centered(roleBadgeLbl));

        const bool ok = (dispo == "Disponible");
        auto *badge = new QLabel(dispo); badge->setAlignment(Qt::AlignCenter);
        badge->setStyleSheet(ok
            ? "background:rgba(16,185,129,0.25); color:#34D399; border:1px solid #10B981; border-radius:5px; padding:2px 6px; font-size:8pt; font-weight:bold;"
            : "background:rgba(239,68,68,0.25);  color:#F87171; border:1px solid #EF4444; border-radius:5px; padding:2px 6px; font-size:8pt; font-weight:bold;");
        t->setCellWidget(r, 8, centered(badge));

        auto *aw = new QWidget; auto *al = new QHBoxLayout(aw);
        al->setContentsMargins(2, 2, 2, 2); al->setSpacing(2);

        auto *btnView = new QPushButton; btnView->setObjectName("rowBtn");
        btnView->setIcon(QIcon(":/icons/eye_b.png")); btnView->setIconSize(QSize(16, 16)); btnView->setCursor(Qt::PointingHandCursor);
        btnView->setToolTip("Consulter cet employé");
        connect(btnView, &QPushButton::clicked, this, [this, r]() { fillEmployeForm(r); });

        auto *btnEdit = new QPushButton; btnEdit->setObjectName("rowBtn");
        btnEdit->setIcon(QIcon(":/icons/edit_b.png")); btnEdit->setIconSize(QSize(16, 16)); btnEdit->setCursor(Qt::PointingHandCursor);
        btnEdit->setToolTip("Modifier cet employé");
        connect(btnEdit, &QPushButton::clicked, this, [this, r]() { fillEmployeForm(r); });

        auto *btnDel = new QPushButton; btnDel->setObjectName("rowBtn");
        btnDel->setIcon(QIcon(":/icons/trash_r.png")); btnDel->setIconSize(QSize(16, 16)); btnDel->setCursor(Qt::PointingHandCursor);
        btnDel->setToolTip("Supprimer cet employé");
        connect(btnDel, &QPushButton::clicked, this, [this, empId]() { onSupprimerEmploye(empId); });

        al->addWidget(btnView);
        al->addWidget(btnEdit);
        al->addWidget(btnDel);
        t->setCellWidget(r, 9, aw);
    }

    ui->labelCount->setText(QString("Affichage de %1 sur %1 employés").arg(rowCount));
    updateEmployeStats();
    applyPrivileges();
}

void MainWindow::fillEmployeForm(int row)
{
    auto *t = ui->tableEmployes;
    if (row < 0 || row >= t->rowCount()) return;

    QString idStr = cellText(t, row, 1);
    ui->lineId->setText(idStr);
    ui->lineId->setReadOnly(true);
    ui->lineId->setFocusPolicy(Qt::NoFocus);
    ui->lineNom->setText(cellText(t, row, 2));
    ui->linePrenom->setText(cellText(t, row, 3));
    ui->lineEmail->setText(cellText(t, row, 4));
    ui->comboRole->setCurrentText(cellLabel(t, row, 5));
    ui->comboDept->setCurrentText(cellText(t, row, 6));

    QString teamsStr = cellText(t, row, 7);
    QStringList teamsList;
    for (const QString &p : teamsStr.split(',', Qt::SkipEmptyParts)) {
        QString item = p.trimmed();
        if (!item.isEmpty() && item != "—") teamsList << item;
    }
    ui->comboEquipes->setCheckedItems(teamsList);
    ui->comboDispo->setCurrentText(cellLabel(t, row, 8));

    m_editingEmployeId = idStr.toInt();
    ui->btnAjouter->setText("Modifier");
    ui->labelFormTitle->setText("Modifier un employé");
    if (m_btnSupprimerForm) m_btnSupprimerForm->show();
}

void MainWindow::onAjouterEmploye()
{
    static const QRegularExpression nameRegex(R"(^[\p{L}]+([ '-][\p{L}]+)*$)");

    QString nom = ui->lineNom->text().trimmed();
    if (nom.length() < 2 || !nameRegex.match(nom).hasMatch()) {
        QMessageBox::warning(this, "Contrôle de saisie",
            "Le nom de l'employé doit comporter au moins 2 caractères et ne contenir que des lettres (aucun chiffre ni symbole).");
        ui->lineNom->setFocus();
        return;
    }

    QString prenom = ui->linePrenom->text().trimmed();
    if (prenom.length() < 2 || !nameRegex.match(prenom).hasMatch()) {
        QMessageBox::warning(this, "Contrôle de saisie",
            "Le prénom de l'employé doit comporter au moins 2 caractères et ne contenir que des lettres (aucun chiffre ni symbole).");
        ui->linePrenom->setFocus();
        return;
    }

    QString email = ui->lineEmail->text().trimmed();
    // Format standard conforme aux e-mails du quotidien (RFC 5322) :
    // - Identifiant : lettres, chiffres, '.', '_', '-', '+' (ne commence/finit pas par un point, pas de '..')
    // - Domaine : lettres uniquement (au moins 2 lettres, sans chiffres)
    // - Extension : point obligatoire suivi de 2 à 10 lettres (.com, .tn, .org, etc.)
    static const QRegularExpression emailRegex(
        R"(^[a-zA-Z0-9]+([._%+-][a-zA-Z0-9]+)*@[a-zA-Z]{2,}(\.[a-zA-Z]{2,})*\.[a-zA-Z]{2,10}$)");

    if (email.isEmpty() || email.length() > 254 || email.contains(' ') || !emailRegex.match(email).hasMatch()) {
        QMessageBox::warning(this, "Contrôle de saisie",
            "Veuillez saisir une adresse e-mail valide respectant les standards usuels (ex: utilisateur@domaine.com ou .tn).\n\n"
            "Conditions requises :\n"
            "• Format : identifiant@domaine.extension\n"
            "• Ne peut pas contenir d'espace ni de points consécutifs (..)\n"
            "• Le nom de domaine doit comporter au moins 2 lettres (sans chiffres)\n"
            "• L'extension (.com, .tn, .fr, etc.) est obligatoire.");
        ui->lineEmail->setFocus();
        return;
    }

    QString role = ui->comboRole->currentText();
    QString dept = ui->comboDept->currentText();
    QString equipes = ui->comboEquipes->checkedItems().join(", ");
    if (equipes.trimmed().isEmpty()) equipes = "—";
    QString dispo = ui->comboDispo->currentText();

    if (m_editingEmployeId > 0) {
        // Mode Modification (UPDATE)
        if (Employe::emailExiste(email, m_editingEmployeId)) {
            QMessageBox::warning(this, "Contrainte d'unicité",
                QString("L'adresse e-mail « %1 » est déjà utilisée par un autre employé.").arg(email));
            ui->lineEmail->setFocus();
            return;
        }

        Employe emp(m_editingEmployeId, nom, prenom, email, role, dept, equipes, dispo);
        if (emp.modifier(m_editingEmployeId)) {
            QMessageBox::information(this, "Modification réussie",
                QString("L'employé #%1 a été mis à jour avec succès dans la base Oracle.").arg(m_editingEmployeId));
            onViderEmploye();
            refreshEmployesTable();
        } else {
            QMessageBox::critical(this, "Erreur Oracle",
                "Échec de la modification dans la base Oracle :\n" + emp.getLastError());
        }
    } else {
        // Mode Ajout (CREATE)
        // L'ID est géré automatiquement sans possibilité de le modifier et garanti unique
        int id = Employe::prochainId();
        while (Employe::existe(id)) {
            id++;
        }
        ui->lineId->setText(QString::number(id));

        if (Employe::emailExiste(email)) {
            QMessageBox::warning(this, "Contrainte d'unicité",
                QString("L'adresse e-mail « %1 » est déjà enregistrée dans la base Oracle.").arg(email));
            ui->lineEmail->setFocus();
            return;
        }

        Employe emp(id, nom, prenom, email, role, dept, equipes, dispo);
        if (emp.ajouter()) {
            QMessageBox::information(this, "Ajout réussi",
                QString("L'employé %1 %2 (ID: %3) a été ajouté avec succès dans la base Oracle.").arg(prenom, nom).arg(id));
            onViderEmploye();
            refreshEmployesTable();
        } else {
            QMessageBox::critical(this, "Erreur Oracle",
                "Échec de l'ajout dans la base Oracle :\n" + emp.getLastError());
        }
    }
}

void MainWindow::onViderEmploye()
{
    ui->lineId->setReadOnly(true);
    ui->lineId->setFocusPolicy(Qt::NoFocus);
    ui->lineId->setText(QString::number(Employe::prochainId()));
    ui->lineNom->clear();
    ui->linePrenom->clear();
    ui->lineEmail->clear();
    ui->comboRole->setCurrentIndex(0);
    ui->comboDept->setCurrentIndex(0);
    ui->comboEquipes->clearChecked();
    ui->comboDispo->setCurrentIndex(0);
    m_editingEmployeId = -1;
    ui->btnAjouter->setText("Ajouter");
    ui->labelFormTitle->setText("Ajouter un nouvel employé");
    if (m_btnSupprimerForm) m_btnSupprimerForm->hide();
}

void MainWindow::onSupprimerEmploye(int id)
{
    if (QMessageBox::question(this, "Confirmation",
        QString("Êtes-vous sûr de vouloir supprimer définitivement l'employé #%1 de la base Oracle ?").arg(id),
        QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) {
        return;
    }

    if (m_employeModel.supprimer(id)) {
        QMessageBox::information(this, "Suppression réussie",
            QString("L'employé #%1 a été supprimé. Les identifiants ont été réorganisés de façon ordonnée.").arg(id));
        if (m_editingEmployeId == id) onViderEmploye();
        else ui->lineId->setText(QString::number(Employe::prochainId()));
        refreshEmployesTable();
    } else {
        QMessageBox::critical(this, "Erreur Oracle",
            "Impossible de supprimer l'employé de la base de données :\n" + m_employeModel.getLastError());
    }
}

void MainWindow::onFilterEmployes()
{
    QString txt = ui->lineSearch->text().trimmed().toLower();
    int roleIdx = ui->comboFilterRole->currentIndex();
    int deptIdx = ui->comboFilterDept->currentIndex();
    QString role = ui->comboFilterRole->currentText();
    QString dept = ui->comboFilterDept->currentText();
    bool onlyDispo = ui->checkDispo->isChecked();

    QString sql = "SELECT ID_EMPLOYE, NOM, PRENOM, EMAIL, ROLE, DEPARTEMENT, EQUIPE, DISPONIBILITE FROM EMPLOYE WHERE 1=1";
    if (!txt.isEmpty()) {
        sql += " AND (LOWER(NOM) LIKE :txt OR LOWER(PRENOM) LIKE :txt OR LOWER(EMAIL) LIKE :txt OR TO_CHAR(ID_EMPLOYE) LIKE :txt)";
    }
    if (roleIdx > 0) {
        sql += " AND ROLE = :role";
    }
    if (deptIdx > 0) {
        sql += " AND DEPARTEMENT = :dept";
    }
    if (onlyDispo) {
        sql += " AND DISPONIBILITE = 'Disponible'";
    }
    sql += " ORDER BY ID_EMPLOYE ASC";

    QSqlQuery q;
    q.prepare(sql);
    if (!txt.isEmpty()) q.bindValue(":txt", "%" + txt + "%");
    if (roleIdx > 0) q.bindValue(":role", role);
    if (deptIdx > 0) q.bindValue(":dept", dept);

    if (q.exec()) {
        auto *model = new QSqlQueryModel(this);
        model->setQuery(std::move(q));
        refreshEmployesTable(model);
    }
}

void MainWindow::onTrierEmploye()
{
    QMenu menu(this);
    menu.setStyleSheet("QMenu { background:#05264F; color:#FFFFFF; border:1px solid #2F67E0; padding:6px; }"
                       "QMenu::item { padding:8px 20px; }"
                       "QMenu::item:selected { background:#4B26A0; }");

    QAction *actNomAsc  = menu.addAction("Nom (A → Z)");
    QAction *actNomDesc = menu.addAction("Nom (Z → A)");
    QAction *actRole    = menu.addAction("Rôle");
    QAction *actDept    = menu.addAction("Département");
    QAction *actId      = menu.addAction("Identifiant (ID)");

    QAction *sel = menu.exec(QCursor::pos());
    if (!sel) return;

    QSqlQueryModel *sorted = nullptr;
    if (sel == actNomAsc)       sorted = Employe::trier("Nom", true);
    else if (sel == actNomDesc) sorted = Employe::trier("Nom", false);
    else if (sel == actRole)    sorted = Employe::trier("Rôle", true);
    else if (sel == actDept)    sorted = Employe::trier("Département", true);
    else if (sel == actId)      sorted = Employe::trier("ID", true);

    if (sorted) {
        refreshEmployesTable(sorted);
    }
}

void MainWindow::onExporterEmploye()
{
    if (!AuthManager::instance().has(EmployeExport)) return;

    QString filename = QFileDialog::getSaveFileName(this, "Exporter les employés", "employes.csv", "Fichiers CSV (*.csv)");
    if (filename.isEmpty()) return;

    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Erreur export", "Impossible de créer le fichier d'export.");
        return;
    }

    QTextStream out(&file);
    out << "ID;Nom;Prenom;Email;Role;Departement;Equipes;Disponibilite\n";

    auto *t = ui->tableEmployes;
    for (int r = 0; r < t->rowCount(); ++r) {
        out << cellText(t, r, 1) << ";"
            << cellText(t, r, 2) << ";"
            << cellText(t, r, 3) << ";"
            << cellText(t, r, 4) << ";"
            << cellLabel(t, r, 5) << ";"
            << cellText(t, r, 6) << ";"
            << cellText(t, r, 7) << ";"
            << cellLabel(t, r, 8) << "\n";
    }
    file.close();

    QMessageBox::information(this, "Export réussi", QString("Export CSV effectué avec succès dans :\n%1").arg(filename));
}

void MainWindow::updateEmployeStats()
{
    QSqlQuery qRole("SELECT ROLE, COUNT(*) FROM EMPLOYE GROUP BY ROLE");
    ChartData roleData;
    while (qRole.next()) {
        QString r = qRole.value(0).toString();
        int count = qRole.value(1).toInt();
        QColor col = (r == "Organisateur") ? QColor("#2F67E0") : (r == "Technicien") ? QColor("#0BF8F6") : QColor("#7C4DFF");
        roleData.append({ r, static_cast<double>(count), col });
    }
    if (!roleData.isEmpty()) ui->chartRole->setData(roleData);

    QSqlQuery qDept("SELECT DEPARTEMENT, COUNT(*) FROM EMPLOYE GROUP BY DEPARTEMENT");
    ChartData deptData;
    while (qDept.next()) {
        QString d = qDept.value(0).toString();
        int count = qDept.value(1).toInt();
        QColor col = (d == "Logistique") ? QColor("#0BF8F6") : (d == "Technique") ? QColor("#2F67E0") : QColor("#7C4DFF");
        deptData.append({ d, static_cast<double>(count), col });
    }
    if (!deptData.isEmpty()) ui->chartDept->setData(deptData);
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
const int kMaxTeamSize = 6;     // effectif maximum d'une equipe (attribution automatique)
} // namespace

void MainWindow::setupAuth()
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
    const QString saved = s.value("auth/username", s.value("login/email")).toString();
    const bool rem = s.value("auth/rememberMe", !saved.isEmpty()).toBool();
    if (!saved.isEmpty() && rem) {
        ui->lineEmailLogin->setText(saved);
        ui->checkRememberLogin->setChecked(true);
    }

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
                QString("Votre compte a été créé avec succès%1.\n\n"
                        "Vous bénéficiez d'un ACCÈS COMPLET à l'ensemble des modules et fonctionnalités de l'application.\n\n"
                        "Connectez-vous avec votre e-mail et votre mot de passe.")
                .arg(dlg.verified() ? " et votre e-mail est vérifié" : ""));
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

void MainWindow::doLogin()
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
    if (ui->checkRememberLogin->isChecked()) {
        s.setValue("auth/username", email);
        s.setValue("auth/rememberMe", true);
        s.setValue("login/email", email);
    } else {
        s.remove("auth/username");
        s.setValue("auth/rememberMe", false);
        s.remove("login/email");
    }

    ui->linePasswordLogin->clear();
    ui->btnEyeLogin->setChecked(false);
    ui->labelErrorLogin->hide();
    showRoles();                    // compte verifie -> Choix du role (Participant / Jury / Employe)
}

void MainWindow::doLogout()
{
    AuthManager::instance().logout();
    ui->stackedPages->setCurrentWidget(ui->pageAccueil);
    ui->btnNavAccueil->setChecked(true);
    ui->linePasswordLogin->clear();
    showConnexion();
    (ui->lineEmailLogin->text().isEmpty() ? ui->lineEmailLogin : ui->linePasswordLogin)->setFocus();
}

// nom / e-mail du compte connecte + privileges (appele apres le choix du role)
void MainWindow::applySession(const QString &profile)
{
    const UserAccount *u = AuthManager::instance().current();
    if (!u) return;
    ui->labelAdminName->setText(u->name);
    ui->labelAdminMail->setText(u->email);
    ui->btnAdmin->setText(u->name + "  ▾\n" + profile);
    m_actWho->setText(u->email + "  ·  " + u->role + "  ·  " + profile);
    applyPrivileges();
}

void MainWindow::applyPrivileges()
{
    const AuthManager &A = AuthManager::instance();

    // ---- menu de gauche : un module non autorise disparait
    struct Nav { QPushButton *b; Privilege p; };
    const Nav navs[] = {
        { ui->btnNavAccueil, ViewAccueil }, { ui->btnNavParticipants, ViewParticipants },
        { ui->btnNavProjets, ViewProjets },
        { ui->btnNavEmployes, ViewEmployes }, { ui->btnNavJury, ViewJury },
        { ui->btnNavParametres, ViewParametres } };
    for (const Nav &n : navs) n.b->setVisible(A.has(n.p));

    // ---- menu Admin
    m_actUsers->setVisible(A.has(ManageUsers));
    m_actSmtp->setVisible(A.has(ManageUsers));

    // ---- module Employes : tous les boutons CRUD sont 100% actifs et operationnels
    bool canAdd = true;
    bool canEdit = true;
    bool canDelete = true;
    bool canExport = true;
    bool canAssign = true;

    auto deny = [](Privilege p) { return "Privilège requis : " + AuthManager::privilegeLabel(p); };
    ui->frameForm->setEnabled(canAdd);
    setAllowed(ui->btnAjouter,  canAdd,    deny(EmployeAdd));
    setAllowed(ui->btnVider,    canAdd,    deny(EmployeAdd));
    setAllowed(ui->btnAffecter, canAssign, deny(EmployeAssign));
    setAllowed(ui->btnExporter, canExport, deny(EmployeExport));
    ui->frameForm->setToolTip(canAdd ? QString() : deny(EmployeAdd));

    auto *t = ui->tableEmployes;
    for (int r = 0; r < t->rowCount(); ++r) {
        QWidget *aw = t->cellWidget(r, 9);
        if (!aw) continue;
        const auto btns = aw->findChildren<QPushButton *>();       // oeil, crayon, poubelle
        if (btns.size() >= 3) {
            setAllowed(btns[1], canEdit,   deny(EmployeEdit));
            setAllowed(btns[2], canDelete, deny(EmployeDelete));
        }
    }
}

// ===========================================================================
//  ATTRIBUTION AUTOMATIQUE INTELLIGENTE (traitement de CV)
// ===========================================================================
void MainWindow::onAssignToTeam()
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

    // Mettre à jour l'équipe dans la base Oracle
    bool okId = false;
    int idVal = emp.id.toInt(&okId);
    if (okId) {
        Employe empObj(idVal, emp.nom, emp.prenom, emp.email, emp.role, emp.dept, teams.join(", "), emp.dispo);
        empObj.modifier(idVal);
    }

    QMessageBox::information(this, "Affectation effectuée",
        QString("%1 %2 a été affecté(e) à « %3 ».").arg(emp.prenom, emp.nom, dlg.chosenTeam()));
}

// ===========================================================================
//  IMPLÉMENTATION DES DIALOGUES INTÉGRÉS DANS MAINWINDOW.CPP
// ===========================================================================
static QString hackflowDialogStyle()
{
    return QStringLiteral(R"(
QDialog { background-color: #00132F; }
QLabel { color: #D5DDF0; font-size: 10.5pt; background: transparent; }
QLabel#dlgTitle { color: #FFFFFF; font-size: 17pt; font-weight: bold; }
QLabel#dlgSub { color: #AEBBDD; font-size: 10.5pt; }
QLabel#dlgError { color: #F87171; background-color: rgba(239,68,68,0.12); border: 1px solid #EF4444; border-radius: 8px; padding: 8px 12px; }
QLabel#dlgInfo  { color: #8FD9FF; background-color: rgba(56,182,246,0.10); border: 1px solid rgba(56,182,246,0.6); border-radius: 8px; padding: 8px 12px; }
QLabel#dlgOk    { color: #34D399; background-color: rgba(16,185,129,0.12); border: 1px solid #10B981; border-radius: 8px; padding: 8px 12px; }
QLineEdit, QComboBox, QSpinBox { background-color: #05264F; border: 1px solid rgba(47,103,224,0.85); border-radius: 10px; padding: 9px 12px; color: #FFFFFF; font-size: 11pt; selection-background-color: #4B26A0; }
QLineEdit:focus, QComboBox:focus, QSpinBox:focus { border: 1px solid #0BF8F6; }
QComboBox QAbstractItemView { background: #05264F; color: #FFFFFF; selection-background-color: #4B26A0; border: 1px solid #2F67E0; }
QCheckBox { color: #D5DDF0; font-size: 10pt; }
QPushButton { background-color: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #38ABFD, stop:0.5 #6D44FA, stop:1 #A04CF6); color: #FFFFFF; font-weight: bold; font-size: 10.5pt; border: none; border-radius: 10px; padding: 10px 18px; min-height: 20px; }
QPushButton:hover { background-color: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #5BBDFF, stop:0.5 #8260FF, stop:1 #B565FF); }
QPushButton:disabled { background-color: #1B2B4F; color: #6F7FA8; }
QPushButton[flat="true"], QPushButton#linkBtn { background: transparent; color: #20B4F2; padding: 4px 6px; }
QPushButton#secondary { background-color: rgba(47,103,224,0.25); border: 1px solid #2F67E0; }
QPushButton#danger { background-color: rgba(239,68,68,0.30); border: 1px solid #EF4444; }
QTableWidget { background-color: #031A42; alternate-background-color: #05204F; color: #E6ECFA; gridline-color: transparent; border: 1px solid rgba(47,103,224,0.5); border-radius: 8px; selection-background-color: rgba(124,77,255,0.45); }
QHeaderView::section { background-color: #0A2A5E; color: #AEBBDD; border: none; padding: 6px; font-weight: bold; }
QProgressBar { background: #031A42; border: 1px solid rgba(47,103,224,0.5); border-radius: 6px; text-align: center; color: #FFFFFF; min-height: 14px; }
QProgressBar::chunk { background-color: #0BF8F6; border-radius: 5px; }
QGroupBox { color: #AEBBDD; border: 1px solid rgba(47,103,224,0.5); border-radius: 8px; margin-top: 10px; padding-top: 10px; }
QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }
QToolTip { background-color: #05264F; color: #FFFFFF; border: 1px solid #2F67E0; }
)");
}

namespace {
static QByteArray hashPwCode(const QString &code, const QByteArray &salt)
{
    return QCryptographicHash::hash(salt + code.toUtf8(), QCryptographicHash::Sha256);
}
const int kCodeValidMinutes = 10;
const int kMaxAttempts      = 5;
const int kResendSeconds    = 30;
}

// ---------------------------------------------------------------------------
// 1. FORGOTPASSWORDDIALOG
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
    for (int j = 0; j < m_stack->count(); ++j)
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

    if (!m_pendingKnownUser) {
        m_codeHash.clear();
        m_info2->setText(QString("Si un compte existe pour %1, un code de vérification vient d'être envoyé.").arg(email));
        showStep(1);
        return;
    }

    const quint32 code = QRandomGenerator::system()->bounded(100000u, 1000000u);
    const QString codeStr = QString::number(code);
    m_codeSalt = QByteArray::number(QRandomGenerator::system()->generate64(), 16);
    m_codeHash = hashPwCode(codeStr, m_codeSalt);
    m_codeExpiry = QDateTime::currentDateTime().addSecs(kCodeValidMinutes * 60);

    const QString name = AuthManager::instance().userName(email);
    const QString body = QString(
        "Bonjour %1,\n\n"
        "Vous avez demandé la réinitialisation de votre mot de passe HackFlow.\n\n"
        "Votre code de vérification : %2\n\n"
        "Ce code est valable %3 minutes. Si vous n'êtes pas à l'origine de cette demande, "
        "ignorez simplement ce message : votre mot de passe ne sera pas modifié.\n\n"
        "— L'équipe HackFlow\n").arg(name.isEmpty() ? "Utilisateur" : name, codeStr).arg(kCodeValidMinutes);

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
    m_info2->setText(QString("Un code de vérification réel a été envoyé à %1.\n"
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
    if (hashPwCode(code, m_codeSalt) != m_codeHash)
        return fail(QString("Code incorrect (%1 tentative(s) restante(s)).").arg(kMaxAttempts - m_attempts));

    QString err;
    if (!AuthManager::instance().setPassword(m_email, m_linePw->text(), &err))
        return fail(err);
    m_codeHash.clear();
    accept();
}

// ---------------------------------------------------------------------------
// 2. CREATEACCOUNTDIALOG
// ---------------------------------------------------------------------------
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

    auto *p1 = new QWidget; auto *l1 = new QVBoxLayout(p1);
    l1->setContentsMargins(0, 0, 0, 0); l1->setSpacing(10);
    auto *t1 = new QLabel("Créer un compte"); t1->setObjectName("dlgTitle");
    auto *s1 = new QLabel("Rejoignez HackFlow. Votre compte bénéficiera immédiatement d'un ACCÈS COMPLET.");
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
    const quint32 code = QRandomGenerator::system()->bounded(100000u, 1000000u);
    const QString codeStr = QString::number(code);
    m_codeSalt = QByteArray::number(QRandomGenerator::system()->generate64(), 16);
    m_codeHash = hashPwCode(codeStr, m_codeSalt);
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
    if (hashPwCode(code, m_codeSalt) != m_codeHash)
        return fail(QString("Code incorrect (%1 tentative(s) restante(s)).").arg(5 - m_attempts));
    m_verified = true;
    m_codeHash.clear();
    finishCreation();
}

// ---------------------------------------------------------------------------
// 3. USERMANAGEMENTDIALOG
// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
// 4. SMTPSETTINGSDIALOG
// ---------------------------------------------------------------------------
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
    auto *sub = new QLabel("Utilisé pour envoyer les codes « Mot de passe oublié ». Avec Gmail, "
                           "utilisez un mot de passe d'application.");
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
        const QString to = m_testTo.isEmpty() ? "dhaouafihanin@gmail.com" : m_testTo;
        m_btnTest->setEnabled(false); m_btnTest->setText("Envoi en cours…");
        m_watcher.setFuture(QtConcurrent::run([cfg, to]() {
            return Mailer::send(cfg, to, "HackFlow - E-mail de test",
                                "Ceci est un e-mail de test envoyé par HackFlow.\nLa configuration SMTP fonctionne parfaitement.\n");
        }));
    });
    connect(&m_watcher, &QFutureWatcher<MailResult>::finished, this, [=]() {
        m_btnTest->setEnabled(true); m_btnTest->setText("Envoyer un e-mail de test");
        const MailResult r = m_watcher.result();
        if (r.ok && !r.savedToOutbox) show("E-mail de test envoyé avec succès.", "dlgOk");
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

// ---------------------------------------------------------------------------
// 5. CVASSIGNDIALOG
// ---------------------------------------------------------------------------
CvAssignDialog::CvAssignDialog(const EmployeeInfo &emp, const QMap<QString, int> &teamLoad,
                               int maxTeamSize, QWidget *parent)
    : QDialog(parent), m_emp(emp), m_load(teamLoad), m_max(maxTeamSize)
{
    setWindowTitle("Attribution automatique à une équipe");
    setStyleSheet(hackflowDialogStyle());
    resize(860, 700);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 18); root->setSpacing(10);

    auto *title = new QLabel(QString("Attribution automatique — %1 %2").arg(emp.prenom, emp.nom));
    title->setObjectName("dlgTitle");
    const QString teams = emp.teams.isEmpty() ? "aucune" : emp.teams.join(", ");
    auto *sub = new QLabel(QString("%1 · %2 · %3 · Équipe(s) actuelle(s) : %4")
                           .arg(emp.role, emp.dept, emp.dispo, teams));
    sub->setObjectName("dlgSub"); sub->setWordWrap(true);
    root->addWidget(title); root->addWidget(sub);

    auto *rowCv = new QHBoxLayout;
    auto *btnBrowse = new QPushButton("Charger le CV…");
    m_cvName = new QLabel(CvAnalyzer::pdfSupported() ? "Aucun CV chargé (PDF, DOCX, TXT)"
                                                     : "Aucun CV chargé (DOCX, TXT — PDF indisponible dans ce Qt)");
    m_cvName->setObjectName("dlgSub");
    rowCv->addWidget(btnBrowse); rowCv->addWidget(m_cvName, 1);
    root->addLayout(rowCv);

    m_status = new QLabel; m_status->setWordWrap(true); m_status->hide();
    root->addWidget(m_status);
    if (emp.dispo != "Disponible")
        setStatus("Attention : cet employé est marqué « " + emp.dispo + " ».", "dlgInfo");

    auto *lblS = new QLabel("<b>Compétences détectées</b>");
    m_skills = new QTableWidget(0, 3);
    m_skills->setHorizontalHeaderLabels({ "Domaine", "Niveau", "Mots-clés trouvés dans le CV" });
    auto *lblT = new QLabel("<b>Compatibilité avec les équipes</b>");
    m_teams = new QTableWidget(0, 5);
    m_teams->setHorizontalHeaderLabels({ "Équipe", "Domaine", "Compatibilité", "Effectif", "Justification" });
    for (QTableWidget *t : { m_skills, m_teams }) {
        t->verticalHeader()->hide();
        t->setEditTriggers(QAbstractItemView::NoEditTriggers);
        t->setSelectionMode(QAbstractItemView::NoSelection);
        t->setAlternatingRowColors(true);
        t->setShowGrid(false);
        t->setWordWrap(true);
        t->setFocusPolicy(Qt::NoFocus);
    }
    m_skills->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_skills->setColumnWidth(1, 150);
    m_skills->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_teams->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_teams->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_teams->setColumnWidth(2, 150);
    m_teams->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_teams->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    root->addWidget(lblS); root->addWidget(m_skills, 2);
    root->addWidget(lblT); root->addWidget(m_teams, 2);

    m_suggest = new QLabel; m_suggest->setObjectName("dlgInfo"); m_suggest->setWordWrap(true); m_suggest->hide();
    root->addWidget(m_suggest);

    auto *foot = new QHBoxLayout;
    foot->addWidget(new QLabel("Équipe retenue :"));
    m_combo = new QComboBox; m_combo->setMinimumWidth(340); m_combo->setEnabled(false);
    foot->addWidget(m_combo); foot->addStretch();
    auto *btnCancel = new QPushButton("Annuler"); btnCancel->setObjectName("secondary");
    m_btnOk = new QPushButton("Affecter à cette équipe"); m_btnOk->setEnabled(false);
    foot->addWidget(btnCancel); foot->addWidget(m_btnOk);
    root->addLayout(foot);

    connect(btnBrowse, &QPushButton::clicked, this, &CvAssignDialog::onBrowse);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_btnOk, &QPushButton::clicked, this, &CvAssignDialog::onConfirm);
}

void CvAssignDialog::setStatus(const QString &text, const char *kind)
{
    m_status->setObjectName(kind);
    m_status->setText(text);
    m_status->style()->unpolish(m_status); m_status->style()->polish(m_status);
    m_status->setMinimumHeight(m_status->heightForWidth(width() - 48) > 0 ? m_status->heightForWidth(width() - 48) + 20 : 0);
    m_status->updateGeometry();
    m_status->show();
}

void CvAssignDialog::onBrowse()
{
    const QString path = QFileDialog::getOpenFileName(this, "Choisir le CV de l'employé", QString(),
                                                      CvAnalyzer::fileFilter());
    if (!path.isEmpty()) loadCvFile(path);
}

void CvAssignDialog::loadCvFile(const QString &path)
{
    m_cvName->setText(QFileInfo(path).fileName());
    m_skills->setRowCount(0); m_teams->setRowCount(0);
    m_combo->clear(); m_combo->setEnabled(false); m_btnOk->setEnabled(false); m_suggest->hide();

    QString err;
    const QString text = CvAnalyzer::extractText(path, &err);
    if (text.isEmpty() && !err.isEmpty()) { setStatus(err, "dlgError"); return; }
    const CvAnalysis a = CvAnalyzer::analyze(text, m_load, m_emp.teams, m_max);
    if (!a.ok) { setStatus(a.error, "dlgError"); return; }
    showAnalysis(a);
}

void CvAssignDialog::showAnalysis(const CvAnalysis &a)
{
    m_skills->setRowCount(a.skills.size());
    for (int i = 0; i < a.skills.size(); ++i) {
        const SkillHit &h = a.skills[i];
        m_skills->setItem(i, 0, new QTableWidgetItem(h.category));
        auto *pb = new QProgressBar; pb->setRange(0, 100); pb->setValue(int(h.score * 100 + 0.5));
        m_skills->setCellWidget(i, 1, pb);
        m_skills->setItem(i, 2, new QTableWidgetItem(h.keywords.mid(0, 12).join(", ")));
    }
    m_skills->resizeRowsToContents();

    m_teams->setRowCount(a.teams.size());
    for (int i = 0; i < a.teams.size(); ++i) {
        const TeamScore &t = a.teams[i];
        m_teams->setItem(i, 0, new QTableWidgetItem(t.team));
        m_teams->setItem(i, 1, new QTableWidgetItem(t.label));
        auto *pb = new QProgressBar; pb->setRange(0, 100); pb->setValue(int(t.finalScore * 100 + 0.5));
        m_teams->setCellWidget(i, 2, pb);
        QString eff = QString("%1 / %2").arg(t.load).arg(m_max);
        if (t.full) eff += " (pleine)";
        m_teams->setItem(i, 3, new QTableWidgetItem(eff));
        QString why = t.reason;
        if (t.alreadyMember) why = "Déjà membre de cette équipe. " + why;
        m_teams->setItem(i, 4, new QTableWidgetItem(why));
        if (!t.eligible())
            for (int c : { 0, 1, 3, 4 }) m_teams->item(i, c)->setForeground(QColor("#6F7FA8"));
    }
    m_teams->resizeRowsToContents();

    m_combo->clear();
    for (const TeamScore &t : a.teams)
        if (t.eligible()) m_combo->addItem(QString("%1 — %2 (%3 %)").arg(t.team, t.label)
                                           .arg(int(t.finalScore * 100 + 0.5)), t.team);
    m_combo->setEnabled(m_combo->count() > 0);
    m_btnOk->setEnabled(m_combo->count() > 0);
    if (m_combo->count()) m_combo->setCurrentIndex(0);

    QStringList notes;
    if (!a.cvEmail.isEmpty() && !m_emp.email.isEmpty() && a.cvEmail != m_emp.email.toLower())
        notes << QString("L'e-mail trouvé dans le CV (%1) ne correspond pas à celui de l'employé (%2) : "
                         "vérifiez qu'il s'agit du bon CV.").arg(a.cvEmail, m_emp.email);
    if (m_combo->count() == 0)
        setStatus("Aucune équipe disponible (toutes pleines ou déjà rejointes).", "dlgError");
    else if (!a.hasSkills())
        setStatus("Aucune compétence reconnue dans ce CV : l'attribution automatique n'est pas fiable. "
                  "Vous pouvez choisir une équipe manuellement ci-dessous.", "dlgInfo");
    else if (const TeamScore *b = a.best())
        setStatus(QString("Équipe recommandée : %1 — %2 (compatibilité %3 %).%4")
                  .arg(b->team, b->label).arg(int(b->finalScore * 100 + 0.5))
                  .arg(notes.isEmpty() ? QString() : "\n" + notes.join("\n")),
                  notes.isEmpty() ? "dlgOk" : "dlgInfo");
    else
        setStatus(notes.join("\n"), "dlgInfo");

    QStringList sug;
    if (!a.suggestedDept.isEmpty())
        sug << QString("Profil suggéré : rôle <b>%1</b>, département <b>%2</b>").arg(a.suggestedRole, a.suggestedDept);
    if (a.years >= 0) sug << QString("expérience détectée : <b>%1 an(s)</b>").arg(a.years);
    if (!sug.isEmpty()) { m_suggest->setText(sug.join(" · ") + " (à titre indicatif)."); m_suggest->show(); }
}

void CvAssignDialog::onConfirm()
{
    if (m_combo->currentIndex() < 0) return;
    m_chosen = m_combo->currentData().toString();
    accept();
}

