#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "scrollhelpers.h"

#include <QAction>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QLabel>
#include <QLayoutItem>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPainter>
#include <QPainterPath>
#include <QPair>
#include <QPixmap>
#include <QPushButton>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QSlider>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

// ---------------------------------------------------------------------------
// Petites fonctions utilitaires pour dessiner les icones (aucun fichier image
// externe n'est necessaire, tout est dessine par le code).
// ---------------------------------------------------------------------------
namespace {

QPixmap newIconPixmap(int logicalSize)
{
    QPixmap pm(logicalSize * 2, logicalSize * 2);
    pm.setDevicePixelRatio(2.0);
    pm.fill(Qt::transparent);
    return pm;
}

// Charge une icone PNG blanche (resources) et la recolore avec "color".
QPixmap tintedPixmap(const QString &path, const QColor &color, int logicalSize)
{
    QPixmap src = QPixmap(path).scaled(logicalSize * 2, logicalSize * 2,
                                       Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QPixmap out(src.size());
    out.fill(Qt::transparent);
    QPainter p(&out);
    p.drawPixmap(0, 0, src);
    p.setCompositionMode(QPainter::CompositionMode_SourceIn);
    p.fillRect(out.rect(), color);
    p.end();
    out.setDevicePixelRatio(2.0);
    return out;
}

QIcon tintedIcon(const QString &path, const QColor &color, int logicalSize = 20)
{
    return QIcon(tintedPixmap(path, color, logicalSize));
}

// Icone de navigation : grise au repos, blanche au survol / quand le bouton est actif
QIcon navIcon(const QString &path)
{
    const QPixmap normal = tintedPixmap(path, QColor("#B7C3DE"), 20);
    const QPixmap white = tintedPixmap(path, QColor("#FFFFFF"), 20);
    QIcon ic;
    ic.addPixmap(normal, QIcon::Normal, QIcon::Off);
    ic.addPixmap(white, QIcon::Active, QIcon::Off);
    ic.addPixmap(white, QIcon::Normal, QIcon::On);
    ic.addPixmap(white, QIcon::Active, QIcon::On);
    return ic;
}

// Avatar rond avec les initiales (comme sur la maquette)
QPixmap makeAvatar(const QString &prenom, const QString &nom)
{
    static const QList<QColor> palette = {
        QColor("#4B26A0"), QColor("#2F67E0"), QColor("#7C3AED"),
        QColor("#0E7C86"), QColor("#1D4ED8"), QColor("#6D28D9")
    };

    int h = 0;
    for (const QChar &c : prenom + nom)
        h += c.unicode();

    QPixmap pm = newIconPixmap(32);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(palette.at(h % palette.size()));
    p.drawEllipse(QRectF(0, 0, 32, 32));

    QFont f = p.font();
    f.setPixelSize(13);
    f.setBold(true);
    p.setFont(f);
    p.setPen(Qt::white);
    const QString initials = prenom.left(1).toUpper() + nom.left(1).toUpper();
    p.drawText(QRectF(0, 0, 32, 32), Qt::AlignCenter, initials);
    p.end();
    return pm;
}

// --- Cartes de la fiche d'evaluation -------------------------------------
QFrame *makeCard(const QString &title, const QString &hint, QWidget *parent, QVBoxLayout **bodyOut)
{
    QFrame *frame = new QFrame(parent);
    frame->setObjectName("evalCard");
    QVBoxLayout *lay = new QVBoxLayout(frame);
    lay->setContentsMargins(20, 16, 20, 18);
    lay->setSpacing(6);

    QLabel *t = new QLabel(title, frame);
    t->setObjectName("labelCardTitle");
    lay->addWidget(t);

    if (!hint.isEmpty()) {
        QLabel *h = new QLabel(hint, frame);
        h->setObjectName("labelCardHint");
        h->setWordWrap(true);
        lay->addWidget(h);
    }
    *bodyOut = lay;
    return frame;
}

QLabel *fieldLabel(const QString &text, QWidget *parent)
{
    QLabel *l = new QLabel(text, parent);
    l->setObjectName("labelFieldName");
    return l;
}

struct Criterion { const char *title; const char *hint; };
const Criterion kCriteria[] = {
    { "Innovation",                 "Originalité de l'idée et caractère novateur de la solution." },
    { "Faisabilité technique",      "Qualité de l'implémentation et maîtrise des technologies utilisées." },
    { "Impact et utilité",          "Pertinence du problème traité et valeur apportée aux utilisateurs." },
    { "Qualité de la présentation", "Clarté du pitch, de la démonstration et des réponses aux questions." },
    { "Travail d'équipe",           "Organisation, répartition des rôles et collaboration." }
};
const int kMaxScore = 10;

} // namespace

// ---------------------------------------------------------------------------
// MainWindow
// ---------------------------------------------------------------------------
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowTitle("HackFlow");

    // Logo : fichier fourni, utilise tel quel (ressource Qt images/logo.jpg).
    // Il est seulement affiche en petit, avec des coins arrondis a l'ecran.
    const QPixmap logo(":/images/logo.jpg");
    const int logoWidth = 60;
    const QPixmap scaled = logo.scaledToWidth(logoWidth * 2, Qt::SmoothTransformation);
    QPixmap rounded(scaled.size());
    rounded.fill(Qt::transparent);
    {
        QPainter p(&rounded);
        p.setRenderHint(QPainter::Antialiasing);
        QPainterPath clip;
        clip.addRoundedRect(QRectF(rounded.rect()), 20, 20);
        p.setClipPath(clip);
        p.drawPixmap(0, 0, scaled);
    }
    rounded.setDevicePixelRatio(2.0);
    ui->labelLogo->setPixmap(rounded);
    ui->labelLogo->setFixedSize(logoWidth, rounded.height() / 2);

    // Icone de recherche dans le champ
    ui->lineEditSearch->addAction(tintedIcon(":/images/icons/search.png", QColor("#9FB0D0"), 18),
                                  QLineEdit::LeadingPosition);

    // Fleches de la pagination
    ui->btnPrev->setIcon(tintedIcon(":/images/icons/chevron-left.png", QColor("#FFFFFF"), 18));
    ui->btnNext->setIcon(tintedIcon(":/images/icons/chevron-right.png", QColor("#FFFFFF"), 18));
    ui->btnPrev->setIconSize(QSize(18, 18));
    ui->btnNext->setIconSize(QSize(18, 18));

    setupNavigation();
    setupTable();
    setupEvaluationForm();
    setupScrolling();
    setupSortAndExport();

    connect(ui->lineEditSearch, &QLineEdit::textChanged, this, &MainWindow::onSearchChanged);
    connect(ui->btnAddMember, &QPushButton::clicked, this, &MainWindow::onAddMember);
    connect(ui->btnPrev, &QPushButton::clicked, this, &MainWindow::onPrevPage);
    connect(ui->btnNext, &QPushButton::clicked, this, &MainWindow::onNextPage);
    connect(ui->tableJury, &QTableWidget::itemChanged, this, &MainWindow::onItemChanged);

    loadSampleData();
    applyFilter();

    ui->scrollAreaJury->setFocus();     // fleches / PageUp / PageDown font defiler la page
}

MainWindow::~MainWindow()
{
    delete ui;
}

// --- Navigation (QStackedWidget) -------------------------------------------
QWidget *MainWindow::makePlaceholderPage(const QString &title)
{
    QWidget *page = new QWidget;
    QVBoxLayout *lay = new QVBoxLayout(page);
    lay->setContentsMargins(24, 20, 24, 20);
    lay->setSpacing(6);

    QLabel *t = new QLabel(title, page);
    t->setObjectName("labelTitle");
    QLabel *s = new QLabel("Cette page sera disponible prochainement.", page);
    s->setObjectName("labelSubtitle");

    lay->addWidget(t);
    lay->addWidget(s);
    lay->addStretch();
    return page;
}

void MainWindow::setupNavigation()
{
    struct NavEntry { QPushButton *button; QString title; QString icon; };
    const QList<NavEntry> items = {
        { ui->btnNavAccueil,      "Accueil",      "house" },
        { ui->btnNavParticipants, "Participants", "users" },
        { ui->btnNavEquipes,      "Équipes",      "users-round" },
        { ui->btnNavProjets,      "Projets",      "clipboard-list" },
        { ui->btnNavMentors,      "Mentors",      "lightbulb" },
        { ui->btnNavEmployes,     "Employés",     "user" },
        { ui->btnNavEquipements,  "Équipements",  "layers" },
        { ui->btnNavJury,         "Jury",         "shield-check" },
        { ui->btnNavParametres,   "Paramètres",   "settings" }
    };

    for (const auto &item : items) {
        item.button->setIcon(navIcon(QString(":/images/icons/%1.png").arg(item.icon)));
        item.button->setIconSize(QSize(20, 20));
        item.button->setText("  " + item.button->text());   // petit espace entre icone et texte

        QWidget *page = nullptr;
        if (item.button == ui->btnNavJury) {
            page = ui->pageJury;                       // page deja dessinee dans le .ui
        } else {
            page = makePlaceholderPage(item.title);   // pages des autres modules
            ui->stackedWidget->addWidget(page);
        }
        connect(item.button, &QPushButton::clicked, this, [this, page]() {
            ui->stackedWidget->setCurrentWidget(page);
        });
    }

    ui->btnNavJury->setChecked(true);
    ui->stackedWidget->setCurrentWidget(ui->pageJury);
}

// --- Tableau ---------------------------------------------------------------
void MainWindow::setupTable()
{
    QTableWidget *t = ui->tableJury;

    t->verticalHeader()->setVisible(false);
    t->verticalHeader()->setDefaultSectionSize(58);
    t->setShowGrid(false);
    t->setSelectionBehavior(QAbstractItemView::SelectItems);
    t->setSelectionMode(QAbstractItemView::SingleSelection);
    // Chaque cellule se modifie seule : double-clic, ou clic sur une cellule
    // deja selectionnee, ou touche F2.
    t->setEditTriggers(QAbstractItemView::DoubleClicked
                       | QAbstractItemView::SelectedClicked
                       | QAbstractItemView::EditKeyPressed);
    t->setIconSize(QSize(32, 32));
    t->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    t->setTextElideMode(Qt::ElideRight);
    t->setWordWrap(false);

    QHeaderView *h = t->horizontalHeader();
    h->setSectionResizeMode(QHeaderView::Stretch);
    h->setSectionResizeMode(ColActions, QHeaderView::Fixed);
    h->resizeSection(ColActions, 100);
    h->setHighlightSections(false);
    h->setSectionsClickable(false);
    h->setFixedHeight(44);
    h->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
}

void MainWindow::loadSampleData()
{
    m_all = {
        { "Ben Salah",  "Karim",   "karim.bensalah@innovtech.tn",   "+216 20 123 456", "Intelligence artificielle", "InnovTech" },
        { "Cherif",     "Leila",   "leila.cherif@creativeminds.tn", "+216 22 234 567", "Design UX / UI",            "CreativeMinds" },
        { "Trabelsi",   "Amine",   "amine.trabelsi@startuphub.tn",  "+216 23 345 678", "Business & Stratégie",      "StartUpHub" },
        { "Jelili",     "Youssef", "youssef.jelili@polytech.tn",    "+216 24 456 789", "Ingénierie logicielle",     "Polytech" },
        { "Gharbi",     "Amira",   "amira.gharbi@webfactory.tn",    "+216 25 567 890", "Développement web",         "WebFactory" },
        { "Mansour",    "Yassine", "yassine.mansour@cloudnine.tn",  "+216 26 678 901", "Cloud & DevOps",            "CloudNine" },
        { "Jlassi",     "Nour",    "nour.jlassi@datawave.tn",       "+216 27 789 012", "Data Science",              "DataWave" },
        { "Haddad",     "Sami",    "sami.haddad@applab.tn",         "+216 28 890 123", "Applications mobiles",      "AppLab" },
        { "Sassi",      "Ines",    "ines.sassi@pixelstudio.tn",     "+216 29 901 234", "Product Design",            "PixelStudio" },
        { "Chaabane",   "Omar",    "omar.chaabane@embedcorp.tn",    "+216 50 012 345", "Systèmes embarqués",        "EmbedCorp" },
        { "Dridi",      "Salma",   "salma.dridi@neuralworks.tn",    "+216 51 123 456", "Intelligence artificielle", "NeuralWorks" },
        { "Mejri",      "Hatem",   "hatem.mejri@secureit.tn",       "+216 52 234 567", "Cybersécurité",             "SecureIT" }
    };
}

int MainWindow::pageCount() const
{
    return qMax(1, (m_filtered.size() + m_pageSize - 1) / m_pageSize);
}

void MainWindow::applyFilter()
{
    const QString q = ui->lineEditSearch->text().trimmed();

    m_filtered.clear();
    for (int i = 0; i < m_all.size(); ++i) {
        const JuryMember &m = m_all.at(i);
        if (q.isEmpty()
            || m.nom.contains(q, Qt::CaseInsensitive)
            || m.prenom.contains(q, Qt::CaseInsensitive)
            || m.email.contains(q, Qt::CaseInsensitive)
            || m.telephone.contains(q, Qt::CaseInsensitive)
            || m.specialite.contains(q, Qt::CaseInsensitive)
            || m.entreprise.contains(q, Qt::CaseInsensitive)) {
            m_filtered.append(i);
        }
    }
    refreshTable();
}

void MainWindow::refreshTable()
{
    QTableWidget *t = ui->tableJury;

    const int total = m_filtered.size();
    const int pages = pageCount();
    m_page = qBound(0, m_page, pages - 1);

    const int start = m_page * m_pageSize;
    const int end = qMin(start + m_pageSize, total);

    {
        // On bloque les signaux pendant qu'on remplit le tableau, sinon
        // itemChanged serait declenche pour chaque cellule creee.
        QSignalBlocker blocker(t);

        t->clearContents();
        t->setRowCount(end - start);

        for (int row = 0; row < end - start; ++row) {
            const int src = m_filtered.at(start + row);
            const JuryMember &m = m_all.at(src);

            const QStringList values = { m.nom, m.prenom, m.email, m.telephone,
                                         m.specialite, m.entreprise };
            for (int col = 0; col < values.size(); ++col) {
                QTableWidgetItem *item = new QTableWidgetItem(values.at(col));
                item->setData(Qt::UserRole, src);     // lien vers m_all
                item->setToolTip(values.at(col));
                if (col == ColNom)
                    item->setIcon(QIcon(makeAvatar(m.prenom, m.nom)));
                t->setItem(row, col, item);
            }

            // Colonne Actions : crayon + poubelle
            QWidget *cell = new QWidget;
            cell->setObjectName("cellActions");
            QHBoxLayout *lay = new QHBoxLayout(cell);
            lay->setContentsMargins(4, 0, 4, 0);
            lay->setSpacing(4);

            QPushButton *btnEdit = new QPushButton(cell);
            btnEdit->setObjectName("btnEdit");
            btnEdit->setIcon(tintedIcon(":/images/icons/pencil.png", QColor("#C8D3EE")));
            btnEdit->setIconSize(QSize(18, 18));
            btnEdit->setFixedSize(32, 32);
            btnEdit->setCursor(Qt::PointingHandCursor);
            btnEdit->setToolTip("Modifier (ou cliquez directement sur une cellule)");

            QPushButton *btnDelete = new QPushButton(cell);
            btnDelete->setObjectName("btnDelete");
            btnDelete->setIcon(tintedIcon(":/images/icons/trash-2.png", QColor("#FF4D6D")));
            btnDelete->setIconSize(QSize(18, 18));
            btnDelete->setFixedSize(32, 32);
            btnDelete->setCursor(Qt::PointingHandCursor);
            btnDelete->setToolTip("Supprimer");

            lay->addWidget(btnEdit);
            lay->addWidget(btnDelete);
            lay->addStretch();
            t->setCellWidget(row, ColActions, cell);

            connect(btnEdit, &QPushButton::clicked, this, [this, row]() {
                QTableWidgetItem *it = ui->tableJury->item(row, ColNom);
                if (it) {
                    ui->tableJury->setCurrentItem(it);
                    ui->tableJury->editItem(it);
                }
            });
            connect(btnDelete, &QPushButton::clicked, this, [this, src]() {
                deleteMember(src);
            });
        }
    }

    // Pagination
    rebuildPagination(pages);
    ui->btnPrev->setEnabled(m_page > 0);
    ui->btnNext->setEnabled(m_page < pages - 1);
    ui->labelCount->setText(total == 0
        ? QString("Aucun membre trouvé")
        : QString("Affichage de %1 à %2 sur %3 membres").arg(start + 1).arg(end).arg(total));

    refreshJurorCombo();
}

void MainWindow::rebuildPagination(int pages)
{
    // On vide les anciens boutons de page (deleteLater : sans danger meme si
    // l'appel vient d'un clic sur l'un de ces boutons).
    while (QLayoutItem *it = ui->pagesLayout->takeAt(0)) {
        if (it->widget())
            it->widget()->deleteLater();
        delete it;
    }

    int first = qMax(0, m_page - 3);
    int last = qMin(pages, first + 7);
    first = qMax(0, last - 7);

    for (int i = first; i < last; ++i) {
        QPushButton *b = new QPushButton(QString::number(i + 1));
        b->setObjectName("btnPage");
        b->setCheckable(true);
        b->setChecked(i == m_page);
        b->setFixedSize(34, 34);
        b->setCursor(Qt::PointingHandCursor);
        connect(b, &QPushButton::clicked, this, [this, i]() {
            m_page = i;
            QTimer::singleShot(0, this, [this]() { refreshTable(); });
        });
        ui->pagesLayout->addWidget(b);
    }
}

// --- Slots -----------------------------------------------------------------
void MainWindow::onSearchChanged(const QString &)
{
    m_page = 0;
    applyFilter();
}

void MainWindow::onPrevPage()
{
    --m_page;
    refreshTable();
}

void MainWindow::onNextPage()
{
    ++m_page;
    refreshTable();
}

void MainWindow::onItemChanged(QTableWidgetItem *item)
{
    // Appele quand l'utilisateur modifie UNE cellule (nom seul, prenom seul...)
    if (!item || item->column() == ColActions)
        return;

    bool ok = false;
    const int src = item->data(Qt::UserRole).toInt(&ok);
    if (!ok || src < 0 || src >= m_all.size())
        return;

    JuryMember &m = m_all[src];
    const int col = item->column();

    QString *field = nullptr;
    switch (col) {
    case ColNom:        field = &m.nom; break;
    case ColPrenom:     field = &m.prenom; break;
    case ColEmail:      field = &m.email; break;
    case ColTelephone:  field = &m.telephone; break;
    case ColSpecialite: field = &m.specialite; break;
    case ColEntreprise: field = &m.entreprise; break;
    default: return;
    }

    const QString value = item->text().trimmed();

    QString error;
    if ((col == ColNom || col == ColPrenom) && value.isEmpty()) {
        error = "Ce champ ne peut pas être vide.";
    } else if (col == ColEmail && !value.isEmpty()
               && !QRegularExpression("^[^@\\s]+@[^@\\s]+\\.[^@\\s]+$").match(value).hasMatch()) {
        error = "L'adresse e-mail n'est pas valide.";
    }

    QSignalBlocker blocker(ui->tableJury);
    if (!error.isEmpty()) {
        item->setText(*field);      // on remet l'ancienne valeur
        QTimer::singleShot(0, this, [this, error]() {
            QMessageBox::warning(this, "Valeur invalide", error);
        });
        return;
    }

    *field = value;
    item->setText(value);
    item->setToolTip(value);

    if (col == ColNom || col == ColPrenom) {
        QTableWidgetItem *nomItem = ui->tableJury->item(item->row(), ColNom);
        if (nomItem)
            nomItem->setIcon(QIcon(makeAvatar(m.prenom, m.nom)));
        refreshJurorCombo();
    }
}

void MainWindow::onAddMember()
{
    JuryMember m;
    if (!askNewMember(m))
        return;

    m_all.append(m);
    ui->lineEditSearch->clear();
    applyFilter();
    m_page = pageCount() - 1;       // on affiche la derniere page (nouveau membre)
    refreshTable();
}

void MainWindow::deleteMember(int sourceIndex)
{
    if (sourceIndex < 0 || sourceIndex >= m_all.size())
        return;

    const JuryMember &m = m_all.at(sourceIndex);

    QMessageBox box(QMessageBox::Question, "Supprimer un membre",
                    QString("Voulez-vous vraiment supprimer %1 %2 ?").arg(m.prenom, m.nom),
                    QMessageBox::NoButton, this);
    QPushButton *yes = box.addButton("Supprimer", QMessageBox::AcceptRole);
    box.addButton("Annuler", QMessageBox::RejectRole);
    box.exec();

    if (box.clickedButton() != yes)
        return;

    m_all.removeAt(sourceIndex);
    // Mise a jour differee : le bouton qui a ete clique fait partie du tableau
    // qu'on va reconstruire.
    QTimer::singleShot(0, this, [this]() { applyFilter(); });
}

bool MainWindow::askNewMember(JuryMember &m)
{
    QDialog dlg(this);
    dlg.setWindowTitle("Ajouter un membre du jury");
    dlg.setMinimumWidth(440);

    QVBoxLayout *root = new QVBoxLayout(&dlg);
    root->setContentsMargins(24, 20, 24, 20);
    root->setSpacing(16);

    QLabel *title = new QLabel("Nouveau membre du jury", &dlg);
    title->setObjectName("labelDialogTitle");
    root->addWidget(title);

    QFormLayout *form = new QFormLayout;
    form->setHorizontalSpacing(16);
    form->setVerticalSpacing(10);

    QLineEdit *nom = new QLineEdit(&dlg);
    QLineEdit *prenom = new QLineEdit(&dlg);
    QLineEdit *email = new QLineEdit(&dlg);
    QLineEdit *tel = new QLineEdit(&dlg);
    QLineEdit *spec = new QLineEdit(&dlg);
    QLineEdit *ent = new QLineEdit(&dlg);
    email->setPlaceholderText("exemple@entreprise.com");
    tel->setPlaceholderText("+216 ...");

    form->addRow("Nom *", nom);
    form->addRow("Prénom *", prenom);
    form->addRow("E-mail", email);
    form->addRow("Téléphone", tel);
    form->addRow("Spécialité", spec);
    form->addRow("Entreprise", ent);
    root->addLayout(form);

    QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    box->button(QDialogButtonBox::Ok)->setText("Ajouter");
    box->button(QDialogButtonBox::Cancel)->setText("Annuler");
    root->addWidget(box);

    connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    connect(box, &QDialogButtonBox::accepted, &dlg, [&]() {
        const QString mail = email->text().trimmed();
        if (nom->text().trimmed().isEmpty() || prenom->text().trimmed().isEmpty()) {
            QMessageBox::warning(&dlg, "Champs obligatoires", "Le nom et le prénom sont obligatoires.");
            return;
        }
        if (!mail.isEmpty()
            && !QRegularExpression("^[^@\\s]+@[^@\\s]+\\.[^@\\s]+$").match(mail).hasMatch()) {
            QMessageBox::warning(&dlg, "E-mail invalide", "L'adresse e-mail n'est pas valide.");
            return;
        }
        dlg.accept();
    });

    if (dlg.exec() != QDialog::Accepted)
        return false;

    m.nom = nom->text().trimmed();
    m.prenom = prenom->text().trimmed();
    m.email = email->text().trimmed();
    m.telephone = tel->text().trimmed();
    m.specialite = spec->text().trimmed();
    m.entreprise = ent->text().trimmed();
    return true;
}

// --- Defilement de la page Jury --------------------------------------------
void MainWindow::setupScrolling()
{
    // Molette fluide sur toute la page
    new SmoothScroller(ui->scrollAreaJury);

    QWidget *viewport = ui->scrollAreaJury->viewport();

    // Le tableau montre toutes ses lignes (5 par page) : la molette doit donc
    // toujours faire defiler la page, jamais le tableau.
    ui->tableJury->viewport()->installEventFilter(new WheelGuard(viewport, false, this));

    // Curseurs, listes deroulantes et zone de texte ne gardent la molette que
    // s'ils ont le focus (sinon la molette ferait changer leur valeur par erreur).
    WheelGuard *guard = new WheelGuard(viewport, true, this);
    for (QSlider *s : std::as_const(m_scoreSliders))
        s->installEventFilter(guard);
    m_cmbProject->installEventFilter(guard);
    m_cmbJuror->installEventFilter(guard);
    m_cmbReco->installEventFilter(guard);
    m_editComment->viewport()->installEventFilter(guard);
}

// --- Fiche d'evaluation : une seule page, liste verticale ------------------
void MainWindow::setupEvaluationForm()
{
    QVBoxLayout *list = ui->verticalLayout_evalList;
    QWidget *parent = ui->evalContainer;
    QVBoxLayout *body = nullptr;

    // 1) Informations generales
    QFrame *card = makeCard("Informations générales", "Projet évalué et évaluateur.", parent, &body);
    m_cmbProject = new QComboBox(card);
    m_cmbProject->addItems({ "Sélectionner un projet…", "SmartCity Dashboard", "MedTech Assist",
                             "EcoSolutions", "UrbanAI", "Clean Energy" });
    m_editTeam = new QLineEdit(card);
    m_editTeam->setPlaceholderText("Nom de l'équipe");
    m_cmbJuror = new QComboBox(card);
    body->addWidget(fieldLabel("Projet évalué *", card));
    body->addWidget(m_cmbProject);
    body->addWidget(fieldLabel("Équipe", card));
    body->addWidget(m_editTeam);
    body->addWidget(fieldLabel("Membre du jury évaluateur *", card));
    body->addWidget(m_cmbJuror);
    list->addWidget(card);

    // 2) Un critere par carte, les uns sous les autres
    for (int i = 0; i < int(sizeof(kCriteria) / sizeof(kCriteria[0])); ++i) {
        QFrame *c = makeCard(QString("%1. %2").arg(i + 1).arg(kCriteria[i].title),
                             kCriteria[i].hint, parent, &body);

        QHBoxLayout *row = new QHBoxLayout;
        row->setSpacing(14);
        QSlider *slider = new QSlider(Qt::Horizontal, c);
        slider->setRange(0, kMaxScore);
        slider->setFocusPolicy(Qt::StrongFocus);
        QLabel *value = new QLabel(QString("0 / %1").arg(kMaxScore), c);
        value->setObjectName("labelScoreValue");
        value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        row->addWidget(slider, 1);
        row->addWidget(value);
        body->addSpacing(4);
        body->addLayout(row);

        connect(slider, &QSlider::valueChanged, this, [this, value](int v) {
            value->setText(QString("%1 / %2").arg(v).arg(kMaxScore));
            updateTotalScore();
        });
        m_scoreSliders.append(slider);
        list->addWidget(c);
    }

    // 3) Commentaires et recommandation
    QFrame *last = makeCard("Commentaires et recommandation",
                            "Observations libres et décision proposée au comité.", parent, &body);
    m_editComment = new QPlainTextEdit(last);
    m_editComment->setPlaceholderText("Points forts, points à améliorer, remarques…");
    m_editComment->setFixedHeight(120);
    m_cmbReco = new QComboBox(last);
    m_cmbReco->addItems({ "Finaliste", "À revoir", "Non retenu" });
    body->addWidget(fieldLabel("Commentaires", last));
    body->addWidget(m_editComment);
    body->addWidget(fieldLabel("Recommandation", last));
    body->addWidget(m_cmbReco);
    list->addWidget(last);

    connect(ui->btnEvalSave, &QPushButton::clicked, this, &MainWindow::onEvalSave);
    connect(ui->btnEvalReset, &QPushButton::clicked, this, &MainWindow::onEvalReset);
    updateTotalScore();
}

void MainWindow::updateTotalScore()
{
    int sum = 0;
    for (const QSlider *s : std::as_const(m_scoreSliders))
        sum += s->value();
    ui->labelTotalScore->setText(
        QString("Note globale : %1 / %2").arg(sum).arg(m_scoreSliders.size() * kMaxScore));
}

void MainWindow::refreshJurorCombo()
{
    if (!m_cmbJuror)
        return;
    const QString current = m_cmbJuror->currentText();
    QSignalBlocker blocker(m_cmbJuror);
    m_cmbJuror->clear();
    m_cmbJuror->addItem("Sélectionner un membre…");
    for (const JuryMember &m : std::as_const(m_all))
        m_cmbJuror->addItem(m.prenom + " " + m.nom);
    const int idx = m_cmbJuror->findText(current);
    m_cmbJuror->setCurrentIndex(idx > 0 ? idx : 0);
}

void MainWindow::onEvalReset()
{
    for (QSlider *s : std::as_const(m_scoreSliders))
        s->setValue(0);
    m_cmbProject->setCurrentIndex(0);
    m_cmbJuror->setCurrentIndex(0);
    m_cmbReco->setCurrentIndex(0);
    m_editTeam->clear();
    m_editComment->clear();
}

void MainWindow::onEvalSave()
{
    if (m_cmbProject->currentIndex() <= 0 || m_cmbJuror->currentIndex() <= 0) {
        QMessageBox::warning(this, "Champs obligatoires",
                             "Veuillez sélectionner le projet évalué et le membre du jury évaluateur.");
        return;
    }

    int sum = 0;
    for (const QSlider *s : std::as_const(m_scoreSliders))
        sum += s->value();

    QMessageBox::information(this, "Évaluation enregistrée",
        QString("Projet : %1\nÉvaluateur : %2\nNote globale : %3 / %4\nRecommandation : %5")
            .arg(m_cmbProject->currentText(), m_cmbJuror->currentText())
            .arg(sum).arg(m_scoreSliders.size() * kMaxScore)
            .arg(m_cmbReco->currentText()));

    onEvalReset();
}

// --- Trier / Exporter ------------------------------------------------------
void MainWindow::setupSortAndExport()
{
    ui->btnSort->setIcon(tintedIcon(":/images/icons/arrow-up-down.png", QColor("#FFFFFF"), 18));
    ui->btnSort->setIconSize(QSize(18, 18));
    ui->btnSort->setText("  " + ui->btnSort->text());

    ui->btnExport->setIcon(tintedIcon(":/images/icons/download.png", QColor("#FFFFFF"), 18));
    ui->btnExport->setIconSize(QSize(18, 18));
    ui->btnExport->setText("  " + ui->btnExport->text());

    struct SortOption { QString text; Column column; bool ascending; };
    const QList<SortOption> options = {
        { "Nom (A → Z)",         ColNom,        true  },
        { "Nom (Z → A)",         ColNom,        false },
        { "Prénom (A → Z)",      ColPrenom,     true  },
        { "Spécialité (A → Z)",  ColSpecialite, true  },
        { "Entreprise (A → Z)",  ColEntreprise, true  }
    };

    QMenu *menu = new QMenu(this);
    for (const SortOption &o : options) {
        QAction *a = menu->addAction(o.text);
        connect(a, &QAction::triggered, this, [this, o]() { sortMembers(o.column, o.ascending); });
    }

    connect(ui->btnSort, &QPushButton::clicked, this, [this, menu]() {
        menu->exec(ui->btnSort->mapToGlobal(QPoint(0, ui->btnSort->height() + 4)));
    });
    connect(ui->btnExport, &QPushButton::clicked, this, &MainWindow::exportMembers);
}

void MainWindow::sortMembers(Column column, bool ascending)
{
    auto key = [column](const JuryMember &m) -> QString {
        switch (column) {
        case ColNom:        return m.nom;
        case ColPrenom:     return m.prenom;
        case ColEmail:      return m.email;
        case ColTelephone:  return m.telephone;
        case ColSpecialite: return m.specialite;
        case ColEntreprise: return m.entreprise;
        default:            return QString();
        }
    };

    std::stable_sort(m_all.begin(), m_all.end(),
                     [&](const JuryMember &a, const JuryMember &b) {
        const int c = QString::localeAwareCompare(key(a), key(b));
        return ascending ? c < 0 : c > 0;
    });

    m_page = 0;
    applyFilter();
}

void MainWindow::exportMembers()
{
    QString path = QFileDialog::getSaveFileName(this, "Exporter les membres du jury",
                                                "membres_jury.csv", "Fichiers CSV (*.csv)");
    if (path.isEmpty())
        return;
    if (!path.endsWith(".csv", Qt::CaseInsensitive))
        path += ".csv";

    auto cell = [](QString s) {
        s.replace(QLatin1Char('"'), QString("\"\""));
        return QString("\"") + s + QString("\"");
    };

    // Export de la liste affichee (resultat de la recherche), toutes pages confondues.
    QString out = "Nom;Prénom;E-mail;Téléphone;Spécialité;Entreprise\r\n";
    for (int idx : std::as_const(m_filtered)) {
        const JuryMember &m = m_all.at(idx);
        out += cell(m.nom) + ";" + cell(m.prenom) + ";" + cell(m.email) + ";"
             + cell(m.telephone) + ";" + cell(m.specialite) + ";" + cell(m.entreprise) + "\r\n";
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(this, "Export impossible",
                             "Impossible d'écrire le fichier :\n" + path);
        return;
    }
    file.write("\xEF\xBB\xBF");            // BOM UTF-8 : accents corrects dans Excel
    file.write(out.toUtf8());
    file.close();

    QMessageBox::information(this, "Export terminé",
        QString("%1 membre(s) exporté(s) vers :\n%2").arg(m_filtered.size()).arg(path));
}
