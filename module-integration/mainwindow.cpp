#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "participantscontroller.h"
#include "jurycontroller.h"
#include "projectspage.h"
#include "authmanager.h"
#include "forgotpassworddialog.h"
#include "createaccountdialog.h"
#include "admindialogs.h"
#include "cvassigndialog.h"
#include "employeescontroller.h"

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
    m_participants = new ParticipantsController(ui, this);   // page Participants (module integre)
    m_employees = new EmployeesController(ui, this, m_headCheck);   // page Employes (module integre)
    m_jury = new JuryController(ui, this);                   // page Jury (module integre)
    m_projects = new ProjectsPage(ui->pageProjets);          // page Projets (module integre)
    ui->layoutPageProjets->addWidget(m_projects, 1);
    // "Nouveau projet" dans l'en-tete de la page (entre le titre et le fil d'Ariane)
    if (auto *head = qobject_cast<QBoxLayout *>(ui->layoutTitleProjets))
        head->insertWidget(2, m_projects->newProjectButton());
    // barre de recherche du haut : filtre les projets quand la page Projets est affichee
    connect(ui->lineTopSearch, &QLineEdit::textChanged, this, [this](const QString &t) {
        if (ui->stackedPages->currentWidget() == ui->pageProjets) m_projects->setSearchText(t);
        else if (ui->stackedPages->currentWidget() == ui->pageEmployes) m_employees->setSearchText(t);
    });
    showConnexion();            // l'application demarre sur l'ecran de connexion
}

MainWindow::~MainWindow()
{
    delete m_participants;      // avant ui : le module utilise les widgets de ui
    m_participants = nullptr;
    delete m_employees;         // idem : avant ui
    m_employees = nullptr;
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

    // case de l'en-tete : tout cocher / decocher (EmployeesController)
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
    if (ui->checkRememberLogin->isChecked()) s.setValue("login/email", email);
    else s.remove("login/email");

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

    // ---- module Employes
    auto deny = [](Privilege p) { return "Privilège requis : " + AuthManager::privilegeLabel(p); };
    ui->frameForm->setEnabled(A.has(EmployeAdd));
    setAllowed(ui->btnAjouter,  A.has(EmployeAdd),    deny(EmployeAdd));
    setAllowed(ui->btnVider,    A.has(EmployeAdd),    deny(EmployeAdd));
    setAllowed(ui->btnAffecter, A.has(EmployeAssign), deny(EmployeAssign));
    setAllowed(ui->btnExporter, A.has(EmployeExport), deny(EmployeExport));
    ui->frameForm->setToolTip(A.has(EmployeAdd) ? QString() : deny(EmployeAdd));

    if (m_employees) m_employees->refresh();      // boutons modifier / supprimer de chaque ligne
}

// ===========================================================================
//  ATTRIBUTION AUTOMATIQUE INTELLIGENTE (traitement de CV)
// ===========================================================================
void MainWindow::onAssignToTeam()
{
    if (m_employees) m_employees->assignToTeam();
}
