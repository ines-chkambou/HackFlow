#include "gjury.h"
#include "ui_gjury.h"
#include "parametresjury.h"
#include "planningevaluations.h"
#include "evaluationprojets.h"
#include <QHeaderView>
#include <QMessageBox>
#include <QFileDialog>
#include <QMenu>
#include <QAction>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QProgressBar>
#include <QSignalBlocker>
#include <QGraphicsDropShadowEffect>
#include <QSqlRecord>
#include <QSqlQuery>
#include <QSqlQueryModel>
#include <QTableWidget>
#include <QFrame>
#include <QCursor>
#include <QStyle>
#include <QPalette>
#include <QPixmap>
#include <QIcon>

namespace {

// Couleurs des avatars (ordre de la maquette)
const QStringList kAvatarCouleurs = {"#EC4899", "#14B8A6", "#F9A825", "#3B9AF5", "#3F6FF2",
                                     "#1FA8F5", "#E8468D", "#22D3D3", "#F6A632", "#22C7E8"};

int sommeCaracteres(const QString &s)
{
    int h = 0;
    for (const QChar &c : s) h += c.unicode();
    return h;
}

QString couleurRole(const QString &r)
{
    static const QMap<QString, QString> m = {
        {"Expert Tech", "#0164FB"}, {"Investisseur", "#00886E"}, {"Business", "#AB6C14"},
        {"Designer", "#CB358F"}, {"Développeur", "#3B31F4"}, {"Expert Métier", "#057DF4"},
        {"Architecte", "#4232EF"}, {"Consultant", "#00889D"}};
    if (m.contains(r)) return m.value(r);
    static const QStringList pal = {"#0164FB", "#00886E", "#AB6C14", "#CB358F", "#3B31F4", "#00889D"};
    return pal.at(sommeCaracteres(r) % pal.size());
}

QString couleurEquipe(const QString &e)
{
    static const QMap<QString, QString> m = {
        {"Technologie", "#23219D"}, {"Finance", "#025196"}, {"Finances", "#025196"},
        {"Stratégie", "#2A2A9E"}, {"UX/UI", "#1029A2"}, {"WebFactory", "#092EA2"},
        {"Innovation", "#00349B"}, {"Système", "#003299"}, {"Marketing", "#1A28A3"}};
    return m.value(e, "#123C9A");
}

void couleursDispo(const QString &d, QString &fond, QString &texte)
{
    if (d == "Disponible")      { fond = "#007E65"; texte = "#FFFFFF"; }
    else if (d == "En ligne")   { fond = "#0167F9"; texte = "#FFFFFF"; }
    else if (d == "En attente") { fond = "#6B4A14"; texte = "#F3B04A"; }
    else                        { fond = "#334155"; texte = "#E2E8F0"; }
}

QString initiales(const QString &nom)
{
    const QStringList mots = nom.simplified().split(' ');
    if (mots.size() >= 2) return (mots.at(0).left(1) + mots.at(1).left(1)).toUpper();
    return nom.simplified().left(2).toUpper();
}

const QString kCheckboxCss =
    "QCheckBox { background: transparent; spacing: 0px; } "
    "QCheckBox::indicator { width: 18px; height: 18px; border: 1.5px solid #AFC4E8; border-radius: 4px; background: transparent; } "
    "QCheckBox::indicator:hover { border-color: #19C3F0; } "
    "QCheckBox::indicator:checked { background-color: #1E90FF; border-color: #1E90FF; image: url(:/img/check.png); }";

const QString kMenuCss =
    "QMenu { background-color: #021B4A; color: #E6EEFF; border: 1px solid #1F57A4; border-radius: 6px; padding: 4px; } "
    "QMenu::item { padding: 8px 22px; border-radius: 4px; } QMenu::item:selected { background-color: #0B6BFF; }";

const QString kDialogCss =
    "QDialog { background-color: #01143C; } "
    "QLabel { color: #E6EEFF; font-size: 13px; background: transparent; } "
    "QLineEdit, QComboBox { background-color: #01286C; border: 1px solid #1F57A4; border-radius: 8px; padding: 0px 12px; "
    "min-height: 38px; color: #EAF2FF; font-size: 13px; } "
    "QLineEdit:focus, QComboBox:focus { border: 1px solid #3B82F6; } "
    "QComboBox::drop-down { border: none; width: 28px; } "
    "QComboBox::down-arrow { image: url(:/img/chevron.png); width: 12px; height: 7px; } "
    "QComboBox QAbstractItemView { background-color: #021B4A; color: #E6EEFF; border: 1px solid #1F57A4; selection-background-color: #0B6BFF; outline: none; } "
    "QPushButton { background-color: #11254F; color: #EAF2FF; border: 1px solid #1D4A8B; border-radius: 8px; padding: 0px 18px; "
    "min-height: 40px; font-size: 13px; font-weight: 600; } "
    "QPushButton:hover { background-color: #173266; border-color: #2F6FD8; } "
    "QPushButton[principal=\"true\"] { background-color: #0391FC; color: #FFFFFF; border: none; font-weight: bold; } "
    "QPushButton[principal=\"true\"]:hover { background-color: #1AA0FF; } "
    "QTableWidget { background-color: #021B4A; border: 1px solid #0F3070; border-radius: 6px; color: #E6EEFF; } "
    "QHeaderView::section { background-color: #05215A; color: #B8C7E8; border: none; padding: 6px; font-weight: bold; }";

} // namespace

GJury::GJury(QWidget *parent)
    : QMainWindow(parent),
      ui(new Ui::GJury),
      m_pageCourante(1),
      m_taillePage(10),
      m_triCritere("ID"),
      m_triOrdre("ASC"),
      m_chkTous(nullptr),
      m_modeEditionEval(false),
      m_idEvalEnEdition(-1),
      m_roleCourant("Administrateur"),
      m_nomUtilisateurCourant("Admin"),
      m_donutChart(nullptr),
      m_pageParametres(nullptr),
      m_pagePlanning(nullptr),
      m_pageEvaluations(nullptr)
{
    ui->setupUi(this);

    // Anneau de statistiques de la carte « Statistiques du jury »
    m_donutChart = new DonutChartWidget(ui->widgetDonutContainer);
    ui->innerDonutLayout->addWidget(m_donutChart);

    setupUIStyling();
    configurerBarreSuperieure();
    configurerPageParametres();
    configurerPagePlanning();
    configurerPageEvaluations();

    // Remplissage initial
    chargerFiltresJurys();
    chargerTableauJurys();
    chargerCombosProjetsEtJurys();
    chargerTableauEvaluations();
    actualiserStatistiquesEtGraphiques();
    actualiserClassementInnovant();
    actualiserAlertesDivergence();

    // Navigation par défaut sur le module Jury (Page 0)
    ui->stackedWidget->setCurrentIndex(0);
    definirNavActif(ui->btnNavJury);

    // Synchronisation de la note calculée en direct
    recalculerNoteGlobaleFormulaire();
}

GJury::~GJury()
{
    delete ui;
}

bool GJury::estAdministrateur() const
{
    return m_roleCourant.contains("Admin") || m_roleCourant.contains("Organisateur");
}

void GJury::ajouterLoupe(QLineEdit *champ, int x, int y, int taille)
{
    QLabel *loupe = new QLabel(champ);
    QPixmap pm(":/img/search.png");
    pm.setDevicePixelRatio(40.0 / taille);   // image 40x40 affichée en taille x taille (net sur écrans HiDPI)
    loupe->setPixmap(pm);
    loupe->setGeometry(x, y, taille, taille);
    loupe->setStyleSheet("background: transparent; border: none;");
    loupe->setAttribute(Qt::WA_TransparentForMouseEvents);

#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
    QPalette pal = champ->palette();
    pal.setColor(QPalette::PlaceholderText, QColor("#BDD6FF"));
    champ->setPalette(pal);
#endif
}

void GJury::configurerBarreSuperieure()
{
    // Loupes : barre de recherche du haut (47 px) et champ de la barre de filtres (41 px)
    ajouterLoupe(ui->inputRechercheHaut, 18, 13, 20);
    ajouterLoupe(ui->inputRechercheJury, 14, 11, 18);

    // La recherche globale filtre la liste des jurés
    connect(ui->inputRechercheHaut, &QLineEdit::textChanged, this, [this](const QString &t) {
        ui->inputRechercheJury->setText(t);
        if (!t.isEmpty() && ui->stackedWidget->currentIndex() != 0) {
            on_btnNavJury_clicked();
        }
    });
}

void GJury::setupUIStyling()
{
    // ---- Tableau des jurés (largeurs relevées sur la maquette : 884 px au total) ----
    QTableWidget *t = ui->tableJurys;
    QHeaderView *hh = t->horizontalHeader();
    const int largeurs[10] = {40, 36, 120, 81, 121, 97, 97, 96, 94, 102};
    for (int i = 0; i < 10; ++i) {
        hh->setSectionResizeMode(i, QHeaderView::Fixed);
        t->setColumnWidth(i, largeurs[i]);
    }
    hh->setSectionResizeMode(4, QHeaderView::Stretch);   // l'email absorbe l'espace restant
    hh->setFixedHeight(50);
    hh->setHighlightSections(false);
    hh->setSectionsClickable(false);
    hh->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    t->verticalHeader()->setVisible(false);
    t->verticalHeader()->setDefaultSectionSize(55);
    t->setWordWrap(false);
    t->setFocusPolicy(Qt::NoFocus);
    t->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

    // Décalage des titres de colonnes pour les aligner sur le contenu des cellules
    const QString nb = QString(QChar(0x00A0));
    const int decalages[10] = {0, 3, 14, 0, 0, 0, 3, 3, 3, 7};
    const QStringList titres = {"", "ID", "Nom", "Prénom", "Email", "Téléphone", "Rôle", "Équipe(s)", "Disponibilité", "Actions"};
    for (int i = 0; i < 10; ++i) {
        if (QTableWidgetItem *h = t->horizontalHeaderItem(i)) {
            h->setText(nb.repeated(decalages[i]) + titres.at(i));
        }
    }

    // Case « tout sélectionner » superposée à l'en-tête
    m_chkTous = new QCheckBox(hh);
    m_chkTous->setStyleSheet(kCheckboxCss);
    m_chkTous->setCursor(Qt::PointingHandCursor);
    m_chkTous->setGeometry(11, 16, 18, 18);
    connect(m_chkTous, &QCheckBox::toggled, this, [this](bool actif) {
        for (int r = 0; r < ui->tableJurys->rowCount(); ++r) {
            if (QWidget *w = ui->tableJurys->cellWidget(r, 0)) {
                if (QCheckBox *c = w->findChild<QCheckBox *>()) c->setChecked(actif);
            }
        }
    });

    // Configuration des colonnes du tableau des Évaluations
    ui->tableEvals->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ui->tableEvals->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    ui->tableEvals->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    ui->tableEvals->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    ui->tableEvals->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    ui->tableEvals->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    ui->tableEvals->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Stretch);
    ui->tableEvals->horizontalHeader()->setSectionResizeMode(7, QHeaderView::Fixed);
    ui->tableEvals->setColumnWidth(7, 90);
    ui->tableEvals->verticalHeader()->setVisible(false);

    // Configuration des colonnes du tableau de Classement Innovant
    ui->tableClassement->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ui->tableClassement->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    ui->tableClassement->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    ui->tableClassement->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    ui->tableClassement->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    ui->tableClassement->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    ui->tableClassement->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    ui->tableClassement->horizontalHeader()->setSectionResizeMode(7, QHeaderView::Stretch);
    ui->tableClassement->verticalHeader()->setVisible(false);

    // Configuration du tableau des divergences
    ui->tableDivergences->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ui->tableDivergences->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    ui->tableDivergences->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    ui->tableDivergences->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    ui->tableDivergences->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    ui->tableDivergences->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    ui->tableDivergences->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Stretch);
    ui->tableDivergences->verticalHeader()->setVisible(false);
}

// ====================================================================
// GESTION DU TABLEAU DES JURYS
// ====================================================================

static void viderLayout(QLayout *lay)
{
    while (QLayoutItem *it = lay->takeAt(0)) {
        if (QWidget *w = it->widget()) w->deleteLater();
        delete it;
    }
}

QWidget* GJury::creerCelluleBadge(const QString &texte, const QString &fond, const QString &couleurTexte, int largeur)
{
    QWidget *w = new QWidget();
    w->setStyleSheet("background: transparent;");
    QHBoxLayout *l = new QHBoxLayout(w);
    l->setContentsMargins(9, 0, 0, 0);
    l->setSpacing(0);
    QLabel *b = new QLabel(texte);
    b->setAlignment(Qt::AlignCenter);
    b->setFixedSize(largeur, 25);
    b->setStyleSheet(QString("background-color: %1; color: %2; border-radius: 5px; font-size: 11px; font-weight: 600;")
                         .arg(fond, couleurTexte));
    l->addWidget(b);
    l->addStretch();
    return w;
}

void GJury::remplirCombo(QComboBox *combo, const QString &premier, const QStringList &valeurs)
{
    const QString selection = combo->currentIndex() > 0 ? combo->currentText() : QString();
    QSignalBlocker blocage(combo);
    combo->clear();
    combo->addItem(premier);
    combo->addItems(valeurs);
    const int i = selection.isEmpty() ? 0 : combo->findText(selection);
    combo->setCurrentIndex(i > 0 ? i : 0);
}

void GJury::chargerFiltresJurys()
{
    remplirCombo(ui->comboFiltreRole, "Tous les rôles", Jury::rolesDistincts());
    remplirCombo(ui->comboFiltreEquipe, "Toutes les équipes", Jury::equipesDistinctes());
}

void GJury::chargerTableauJurys()
{
    const QString recherche = ui->inputRechercheJury->text();
    const QString role   = ui->comboFiltreRole->currentIndex() > 0 ? ui->comboFiltreRole->currentText() : QString();
    const QString equipe = ui->comboFiltreEquipe->currentIndex() > 0 ? ui->comboFiltreEquipe->currentText() : QString();
    const QString dispo  = ui->comboFiltreDispo->currentIndex() > 0 ? ui->comboFiltreDispo->currentText() : QString();

    QSqlQueryModel *model = Jury::rechercher(recherche, role, equipe, dispo, m_triCritere, m_triOrdre);
    while (model->canFetchMore()) model->fetchMore();

    // Pagination
    const int total = model->rowCount();
    const int nbPages = qMax(1, (total + m_taillePage - 1) / m_taillePage);
    m_pageCourante = qBound(1, m_pageCourante, nbPages);
    const int debut = (m_pageCourante - 1) * m_taillePage;
    const int fin = qMin(total, debut + m_taillePage);

    const bool admin = estAdministrateur();
    QTableWidget *t = ui->tableJurys;
    t->setRowCount(0);
    t->setRowCount(fin - debut);

    for (int i = debut; i < fin; ++i) {
        const int r = i - debut;
        const QSqlRecord rec = model->record(i);
        const int id = rec.value(0).toInt();
        const QString nom = rec.value(1).toString();
        const QString prenom = rec.value(2).toString();
        const QString email = rec.value(3).toString();
        const QString tel = rec.value(4).toString();
        const QString roleJury = rec.value(5).toString();
        const QString equipeJury = rec.value(6).toString();
        const QString dispoJury = rec.value(7).toString();

        // Col 0 : case à cocher
        QWidget *cellCoche = new QWidget();
        cellCoche->setStyleSheet("background: transparent;");
        QHBoxLayout *lc = new QHBoxLayout(cellCoche);
        lc->setContentsMargins(0, 0, 0, 0);
        QCheckBox *chk = new QCheckBox();
        chk->setStyleSheet(kCheckboxCss);
        chk->setCursor(Qt::PointingHandCursor);
        lc->addWidget(chk, 0, Qt::AlignCenter);
        t->setCellWidget(r, 0, cellCoche);

        // Col 1 : ID
        QWidget *cellId = new QWidget();
        cellId->setStyleSheet("background: transparent;");
        QHBoxLayout *li = new QHBoxLayout(cellId);
        li->setContentsMargins(9, 0, 0, 0);
        QLabel *lblId = new QLabel(QString::number(id));
        lblId->setStyleSheet("color: #E6EEFF; font-size: 12px; background: transparent;");
        li->addWidget(lblId);
        t->setCellWidget(r, 1, cellId);

        // Col 2 : avatar + nom
        QWidget *cellNom = new QWidget();
        cellNom->setStyleSheet("background: transparent;");
        QHBoxLayout *ln = new QHBoxLayout(cellNom);
        ln->setContentsMargins(2, 0, 0, 0);
        ln->setSpacing(14);
        QLabel *avatar = new QLabel(initiales(nom));
        avatar->setFixedSize(36, 36);
        avatar->setAlignment(Qt::AlignCenter);
        avatar->setStyleSheet(QString("background-color: %1; color: #FFFFFF; border-radius: 18px; font-size: 12px; font-weight: bold;")
                                  .arg(kAvatarCouleurs.at((id - 1 + kAvatarCouleurs.size()) % kAvatarCouleurs.size())));
        QLabel *lblNom = new QLabel(nom);
        lblNom->setStyleSheet("color: #F5FAFF; font-size: 12px; font-weight: 600; background: transparent;");
        ln->addWidget(avatar);
        ln->addWidget(lblNom, 1);
        t->setCellWidget(r, 2, cellNom);

        // Col 3, 4, 5 : prénom, email, téléphone (texte simple, l'email est tronqué avec « … »)
        QTableWidgetItem *itPrenom = new QTableWidgetItem(prenom);
        itPrenom->setData(Qt::UserRole, id);
        t->setItem(r, 3, itPrenom);
        QTableWidgetItem *itEmail = new QTableWidgetItem(email);
        itEmail->setToolTip(email);
        t->setItem(r, 4, itEmail);
        t->setItem(r, 5, new QTableWidgetItem(tel));

        // Col 6, 7, 8 : badges rôle / équipe / disponibilité
        t->setCellWidget(r, 6, creerCelluleBadge(roleJury, couleurRole(roleJury), "#FFFFFF", 80));
        t->setCellWidget(r, 7, creerCelluleBadge(equipeJury, couleurEquipe(equipeJury), "#FFFFFF", 79));
        QString fond, texteCouleur;
        couleursDispo(dispoJury, fond, texteCouleur);
        t->setCellWidget(r, 8, creerCelluleBadge(dispoJury, fond, texteCouleur, 70));

        // Col 9 : actions (voir / modifier / supprimer)
        QWidget *cellActions = new QWidget();
        cellActions->setStyleSheet("background: transparent;");
        QHBoxLayout *la = new QHBoxLayout(cellActions);
        la->setContentsMargins(10, 0, 0, 0);
        la->setSpacing(13);
        auto creerAction = [](const QString &icone, const QString &info) {
            QPushButton *b = new QPushButton();
            b->setFixedSize(20, 20);
            b->setIcon(QIcon(":/img/" + icone));
            b->setIconSize(QSize(20, 20));
            b->setFlat(true);
            b->setCursor(Qt::PointingHandCursor);
            b->setToolTip(info);
            b->setStyleSheet("QPushButton { background: transparent; border: none; } "
                             "QPushButton:hover { background-color: rgba(255, 255, 255, 0.10); border-radius: 4px; }");
            return b;
        };
        QPushButton *btnVoir = creerAction("act_eye.png", "Consulter le profil");
        QPushButton *btnEdit = creerAction("act_edit.png", "Modifier");
        QPushButton *btnSupp = creerAction("act_trash.png", "Supprimer");
        btnEdit->setEnabled(admin);
        btnSupp->setEnabled(admin);
        la->addWidget(btnVoir);
        la->addWidget(btnEdit);
        la->addWidget(btnSupp);
        la->addStretch();
        connect(btnVoir, &QPushButton::clicked, this, [this, id]() { afficherDetailJury(id); });
        connect(btnEdit, &QPushButton::clicked, this, [this, id]() { ouvrirDialogueJury(id); });
        connect(btnSupp, &QPushButton::clicked, this, [this, id]() { supprimerJury(id); });
        t->setCellWidget(r, 9, cellActions);
    }
    delete model;

    // Pied du tableau : compteur + pagination
    if (total == 0) {
        ui->lblPaginationJurys->setText("Aucun juré ne correspond à la recherche");
    } else {
        ui->lblPaginationJurys->setText(QString("Affichage de %1 à %2 sur %3 jurés").arg(debut + 1).arg(fin).arg(total));
    }
    ui->btnPageCourante->setText(QString::number(m_pageCourante));
    ui->btnPagePrecedente->setEnabled(m_pageCourante > 1);
    ui->btnPageSuivante->setEnabled(m_pageCourante < nbPages);

    if (m_chkTous) {
        QSignalBlocker blocage(m_chkTous);
        m_chkTous->setChecked(false);
    }
}

void GJury::rafraichirApresModificationJury()
{
    chargerFiltresJurys();
    chargerTableauJurys();
    chargerCombosProjetsEtJurys();
    chargerTableauEvaluations();
    actualiserStatistiquesEtGraphiques();
    actualiserClassementInnovant();
    actualiserAlertesDivergence();
    if (m_pageParametres) m_pageParametres->rafraichir();
    if (m_pagePlanning) m_pagePlanning->rafraichir();
    if (m_pageEvaluations) m_pageEvaluations->rafraichir();
}

void GJury::on_tableJurys_cellDoubleClicked(int row, int)
{
    if (QTableWidgetItem *it = ui->tableJurys->item(row, 3)) {
        afficherDetailJury(it->data(Qt::UserRole).toInt());
    }
}

// ---------------- Ajout / modification (fenêtre de saisie) ----------------

void GJury::on_btnHeaderAjouter_clicked()
{
    ouvrirDialogueJury(-1);
}

void GJury::ouvrirDialogueJury(int idJury)
{
    if (!estAdministrateur()) {
        QMessageBox::information(this, "Accès restreint",
                                 "Seul un administrateur / organisateur peut ajouter ou modifier un juré.");
        return;
    }

    const bool modification = (idJury > 0);
    Jury existant;
    if (modification) {
        existant = Jury::obtenirParId(idJury);
        if (existant.getId() <= 0) return;
    }

    QDialog dlg(this);
    dlg.setWindowTitle(modification ? QString("Modifier le juré #%1").arg(idJury) : "Ajouter un juré");
    dlg.setStyleSheet(kDialogCss);
    dlg.setMinimumWidth(480);

    QVBoxLayout *principal = new QVBoxLayout(&dlg);
    principal->setContentsMargins(24, 22, 24, 22);
    principal->setSpacing(14);

    QLabel *titre = new QLabel(modification ? "Modifier le juré" : "Ajouter un juré");
    titre->setStyleSheet("color: #F5FAFF; font-size: 20px; font-weight: bold;");
    principal->addWidget(titre);

    QGridLayout *grille = new QGridLayout();
    grille->setHorizontalSpacing(14);
    grille->setVerticalSpacing(10);

    QLineEdit *edNom = new QLineEdit(existant.getNom());
    QLineEdit *edPrenom = new QLineEdit(existant.getPrenom());
    QLineEdit *edEmail = new QLineEdit(existant.getEmail());
    QLineEdit *edTel = new QLineEdit(existant.getTelephone());
    edNom->setPlaceholderText("Ben Ali");
    edPrenom->setPlaceholderText("Ahmed");
    edEmail->setPlaceholderText("ahmed.benali@example.com");
    edTel->setPlaceholderText("+216 98 123 456");

    QStringList roles = {"Expert Tech", "Investisseur", "Business", "Designer", "Développeur", "Expert Métier", "Architecte", "Consultant"};
    for (const QString &r : Jury::rolesDistincts()) if (!roles.contains(r)) roles << r;
    QStringList equipes = {"Technologie", "Finance", "Stratégie", "UX/UI", "WebFactory", "Innovation", "Système", "Marketing"};
    for (const QString &e : Jury::equipesDistinctes()) if (!equipes.contains(e)) equipes << e;

    QComboBox *cbRole = new QComboBox();
    cbRole->setEditable(true);
    cbRole->addItems(roles);
    QComboBox *cbEquipe = new QComboBox();
    cbEquipe->setEditable(true);
    cbEquipe->addItems(equipes);
    QComboBox *cbDispo = new QComboBox();
    cbDispo->addItems({"Disponible", "En ligne", "En attente"});

    if (modification) {
        cbRole->setCurrentText(existant.getRole());
        cbEquipe->setCurrentText(existant.getEquipe());
        const int i = cbDispo->findText(existant.getDisponibilite());
        if (i >= 0) cbDispo->setCurrentIndex(i);
    }

    int ligne = 0;
    auto ajouterChamp = [&](const QString &libelle, QWidget *champ) {
        grille->addWidget(new QLabel(libelle), ligne, 0);
        grille->addWidget(champ, ligne, 1);
        ++ligne;
    };
    ajouterChamp("Nom", edNom);
    ajouterChamp("Prénom", edPrenom);
    ajouterChamp("Email", edEmail);
    ajouterChamp("Téléphone", edTel);
    ajouterChamp("Rôle", cbRole);
    ajouterChamp("Équipe", cbEquipe);
    ajouterChamp("Disponibilité", cbDispo);
    grille->setColumnStretch(1, 1);
    principal->addLayout(grille);

    QHBoxLayout *boutons = new QHBoxLayout();
    boutons->addStretch();
    QPushButton *btnAnnuler = new QPushButton("Annuler");
    QPushButton *btnEnregistrer = new QPushButton(modification ? "Enregistrer" : "Ajouter");
    btnEnregistrer->setProperty("principal", true);
    btnAnnuler->setCursor(Qt::PointingHandCursor);
    btnEnregistrer->setCursor(Qt::PointingHandCursor);
    boutons->addWidget(btnAnnuler);
    boutons->addWidget(btnEnregistrer);
    principal->addLayout(boutons);

    connect(btnAnnuler, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(btnEnregistrer, &QPushButton::clicked, &dlg, [&]() {
        Jury j(modification ? idJury : -1, edNom->text(), edPrenom->text(), edEmail->text(), edTel->text(),
               cbRole->currentText(), cbEquipe->currentText(), cbDispo->currentText());
        QString erreur;
        if (!j.valider(erreur)) {
            QMessageBox::warning(&dlg, "Saisie invalide", erreur);
            return;
        }
        const bool ok = modification ? j.modifier(idJury) : j.ajouter();
        if (!ok) {
            QMessageBox::critical(&dlg, "Erreur",
                                  "L'enregistrement dans la base de données a échoué :\n" +
                                  QSqlDatabase::database().lastError().text());
            return;
        }
        dlg.accept();
    });

    if (dlg.exec() == QDialog::Accepted) {
        rafraichirApresModificationJury();
        QMessageBox::information(this, "Succès", modification ? "Le juré a été modifié avec succès."
                                                              : "Le juré a été ajouté avec succès.");
    }
}

void GJury::supprimerJury(int idJury)
{
    if (!estAdministrateur()) {
        QMessageBox::information(this, "Accès restreint", "Seul un administrateur / organisateur peut supprimer un juré.");
        return;
    }
    Jury j = Jury::obtenirParId(idJury);
    QString question = QString("Êtes-vous certain de vouloir supprimer le juré : %1 %2 ?\n\n"
                               "Toutes ses évaluations associées seront également retirées.")
                           .arg(j.getPrenom(), j.getNom());

    auto reponse = QMessageBox::question(this, "Confirmation de suppression", question,
                                         QMessageBox::Yes | QMessageBox::No);
    if (reponse == QMessageBox::Yes) {
        if (Jury::supprimer(idJury)) {
            QMessageBox::information(this, "Suppression effectuée", "Le juré a été supprimé de la base.");
            rafraichirApresModificationJury();
        } else {
            QMessageBox::critical(this, "Erreur", "Impossible de supprimer ce juré.");
        }
    }
}

// ---------------- Détail d'un juré ----------------

void GJury::afficherDetailJury(int idJury)
{
    Jury j = Jury::obtenirParId(idJury);
    if (j.getId() <= 0) return;

    QDialog dialog(this);
    dialog.setWindowTitle(QString("Profil — %1 %2").arg(j.getPrenom(), j.getNom()));
    dialog.setStyleSheet(kDialogCss);
    dialog.resize(560, 470);

    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    layout->setSpacing(12);
    layout->setContentsMargins(24, 22, 24, 22);

    QLabel *lblTitle = new QLabel(QString("Profil de %1 %2").arg(j.getPrenom(), j.getNom()));
    lblTitle->setStyleSheet("color: #F5FAFF; font-size: 20px; font-weight: bold;");
    layout->addWidget(lblTitle);

    QFrame *card = new QFrame();
    card->setStyleSheet("QFrame { background-color: #011844; border: 1px solid #0F3070; border-radius: 10px; } QLabel { border: none; }");
    QVBoxLayout *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(16, 14, 16, 14);
    cardLayout->setSpacing(6);
    cardLayout->addWidget(new QLabel(QString("<b>ID :</b> #%1").arg(j.getId())));
    cardLayout->addWidget(new QLabel(QString("<b>Email :</b> %1").arg(j.getEmail().toHtmlEscaped())));
    cardLayout->addWidget(new QLabel(QString("<b>Téléphone :</b> %1").arg(j.getTelephone().toHtmlEscaped())));
    cardLayout->addWidget(new QLabel(QString("<b>Rôle :</b> <span style='color:#19C3F0;'>%1</span>").arg(j.getRole().toHtmlEscaped())));
    cardLayout->addWidget(new QLabel(QString("<b>Équipe :</b> %1").arg(j.getEquipe().toHtmlEscaped())));
    cardLayout->addWidget(new QLabel(QString("<b>Disponibilité :</b> %1").arg(j.getDisponibilite().toHtmlEscaped())));
    const int nbEvals = Jury::nombreEvaluationsParJury(idJury);
    const double moy = Jury::noteMoyenneDonneeParJury(idJury);
    cardLayout->addWidget(new QLabel(QString("<b>Projets notés :</b> %1").arg(nbEvals)));
    cardLayout->addWidget(new QLabel(QString("<b>Moyenne attribuée :</b> <span style='color:#F59E0B;'>%1 / 20</span>")
                                         .arg(QString::number(moy, 'f', 2))));
    layout->addWidget(card);

    QLabel *lblEvalsTitle = new QLabel("Historique des projets évalués");
    lblEvalsTitle->setStyleSheet("color: #F5FAFF; font-size: 14px; font-weight: bold;");
    layout->addWidget(lblEvalsTitle);

    QTableWidget *tableHisto = new QTableWidget();
    tableHisto->setColumnCount(3);
    tableHisto->setHorizontalHeaderLabels({"Projet", "Note Donnée", "Commentaire"});
    tableHisto->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    tableHisto->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    tableHisto->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    tableHisto->verticalHeader()->setVisible(false);
    tableHisto->setEditTriggers(QAbstractItemView::NoEditTriggers);

    QSqlQuery q;
    q.prepare("SELECT p.titre, e.note, e.commentaire FROM EVALUATION e JOIN PROJET p ON e.id_projet = p.id_projet WHERE e.id_jury = :id");
    q.bindValue(":id", idJury);
    if (q.exec()) {
        int r = 0;
        while (q.next()) {
            tableHisto->insertRow(r);
            tableHisto->setItem(r, 0, new QTableWidgetItem(q.value(0).toString()));
            QTableWidgetItem *itemNote = new QTableWidgetItem(QString("%1 / 20").arg(q.value(1).toDouble(), 0, 'f', 1));
            itemNote->setTextAlignment(Qt::AlignCenter);
            tableHisto->setItem(r, 1, itemNote);
            tableHisto->setItem(r, 2, new QTableWidgetItem(q.value(2).toString()));
            r++;
        }
    }
    layout->addWidget(tableHisto);

    bool evaluer = false;
    QHBoxLayout *boutons = new QHBoxLayout();
    boutons->addStretch();
    QPushButton *btnEval = new QPushButton("Évaluer un projet");
    QPushButton *btnClose = new QPushButton("Fermer");
    btnClose->setProperty("principal", true);
    boutons->addWidget(btnEval);
    boutons->addWidget(btnClose);
    layout->addLayout(boutons);
    connect(btnClose, &QPushButton::clicked, &dialog, &QDialog::accept);
    connect(btnEval, &QPushButton::clicked, &dialog, [&]() { evaluer = true; dialog.accept(); });

    dialog.exec();

    if (evaluer) {
        const int i = ui->comboEvalJury->findData(idJury);
        if (i >= 0) ui->comboEvalJury->setCurrentIndex(i);
        on_btnNavEvals_clicked();
    }
}

// ====================================================================
// RECHERCHE, FILTRES, PAGINATION, TRI ET EXPORTS JURY
// ====================================================================

void GJury::on_inputRechercheJury_textChanged(const QString &)
{
    m_pageCourante = 1;
    chargerTableauJurys();
}

void GJury::on_comboFiltreRole_currentIndexChanged(int)
{
    m_pageCourante = 1;
    chargerTableauJurys();
}

void GJury::on_comboFiltreEquipe_currentIndexChanged(int)
{
    m_pageCourante = 1;
    chargerTableauJurys();
}

void GJury::on_comboFiltreDispo_currentIndexChanged(int)
{
    m_pageCourante = 1;
    chargerTableauJurys();
}

void GJury::on_btnFiltrer_clicked()
{
    m_pageCourante = 1;
    chargerFiltresJurys();
    chargerTableauJurys();
}

void GJury::on_btnPagePrecedente_clicked()
{
    if (m_pageCourante > 1) {
        --m_pageCourante;
        chargerTableauJurys();
    }
}

void GJury::on_btnPageSuivante_clicked()
{
    ++m_pageCourante;   // chargerTableauJurys() ramène la valeur dans l'intervalle valide
    chargerTableauJurys();
}

void GJury::on_btnTrierJury_clicked()
{
    QMenu menu(this);
    menu.setStyleSheet(kMenuCss);

    QAction *a1 = menu.addAction("Nom (A → Z)");
    QAction *a2 = menu.addAction("Nom (Z → A)");
    QAction *a3 = menu.addAction("Rôle");
    QAction *a4 = menu.addAction("Équipe");
    QAction *a5 = menu.addAction("Disponibilité");
    QAction *a6 = menu.addAction("Note moyenne donnée (décroissante)");
    QAction *a7 = menu.addAction("Nombre d'évaluations (décroissant)");
    menu.addSeparator();
    QAction *a0 = menu.addAction("Réinitialiser le tri (par ID)");

    QAction *choix = menu.exec(QCursor::pos());
    if (!choix) return;

    if (choix == a1)      { m_triCritere = "Nom";           m_triOrdre = "ASC"; }
    else if (choix == a2) { m_triCritere = "Nom";           m_triOrdre = "DESC"; }
    else if (choix == a3) { m_triCritere = "Rôle";          m_triOrdre = "ASC"; }
    else if (choix == a4) { m_triCritere = "Équipe";        m_triOrdre = "ASC"; }
    else if (choix == a5) { m_triCritere = "Disponibilité"; m_triOrdre = "ASC"; }
    else if (choix == a6) { m_triCritere = "Note Moyenne";  m_triOrdre = "DESC"; }
    else if (choix == a7) { m_triCritere = "Évaluations";   m_triOrdre = "DESC"; }
    else if (choix == a0) { m_triCritere = "ID";            m_triOrdre = "ASC"; }

    m_pageCourante = 1;
    chargerTableauJurys();
}

void GJury::on_btnExporterJury_clicked()
{
    QMenu menu(this);
    menu.setStyleSheet("background-color: #1C2740; color: #FFFFFF; border: 1px solid #233354;");

    QAction *aPdf = menu.addAction("📄 Exporter la liste en PDF officiel");
    QAction *aCsv = menu.addAction("📊 Exporter la liste en CSV / Excel");

    QAction *selected = menu.exec(QCursor::pos());
    if (selected == aPdf) {
        on_btnExportPDF_clicked();
    } else if (selected == aCsv) {
        on_btnExportCSV_clicked();
    }
}

void GJury::on_btnExportPDF_clicked()
{
    QString fileName = QFileDialog::getSaveFileName(this, "Exporter en PDF", "HackFlow_Membres_Jury.pdf", "Fichiers PDF (*.pdf)");
    if (!fileName.isEmpty()) {
        if (Jury::exporterPDF(fileName)) {
            QMessageBox::information(this, "Export Réussi", "Le document PDF a été généré avec succès !");
        } else {
            QMessageBox::critical(this, "Erreur", "Impossible de générer le fichier PDF.");
        }
    }
}

void GJury::on_btnExportCSV_clicked()
{
    QString fileName = QFileDialog::getSaveFileName(this, "Exporter en CSV", "HackFlow_Membres_Jury.csv", "Fichiers CSV (*.csv)");
    if (!fileName.isEmpty()) {
        if (Jury::exporterCSV(fileName)) {
            QMessageBox::information(this, "Export Réussi", "Les données ont été exportées en CSV avec encodage Excel.");
        } else {
            QMessageBox::critical(this, "Erreur", "Impossible d'enregistrer le fichier CSV.");
        }
    }
}

// ====================================================================
// GESTION DES ÉVALUATIONS ET NOTATION
// ====================================================================

void GJury::chargerCombosProjetsEtJurys()
{
    ui->comboEvalJury->clear();
    QSqlQuery qJury("SELECT id_jury, prenom, nom, role FROM JURY ORDER BY prenom ASC");
    while (qJury.next()) {
        int id = qJury.value(0).toInt();
        QString nomComplet = QString("%1 %2 (%3)").arg(qJury.value(1).toString(), qJury.value(2).toString(), qJury.value(3).toString());
        ui->comboEvalJury->addItem(nomComplet, id);
    }

    ui->comboEvalProjet->clear();
    QSqlQuery qProj("SELECT id_projet, titre, nom_equipe FROM PROJET ORDER BY id_projet ASC");
    while (qProj.next()) {
        int id = qProj.value(0).toInt();
        QString titre = QString("%1 — Équipe %2").arg(qProj.value(1).toString(), qProj.value(2).toString());
        ui->comboEvalProjet->addItem(titre, id);
    }
}

void GJury::chargerTableauEvaluations()
{
    QSqlQueryModel *model = Evaluation::afficher();
    ui->tableEvals->setRowCount(0);

    int rows = model->rowCount();
    ui->tableEvals->setRowCount(rows);

    for (int i = 0; i < rows; ++i) {
        int idEval = model->record(i).value(0).toInt();
        QString projet = model->record(i).value(1).toString();
        QString equipe = model->record(i).value(2).toString();
        QString jure = model->record(i).value(3).toString();
        double note = model->record(i).value(5).toDouble();
        QString date = model->record(i).value(10).toString();
        QString comm = model->record(i).value(9).toString();

        ui->tableEvals->setItem(i, 0, new QTableWidgetItem(QString("#%1").arg(idEval)));
        ui->tableEvals->setItem(i, 1, new QTableWidgetItem(projet));
        ui->tableEvals->setItem(i, 2, new QTableWidgetItem(equipe));
        ui->tableEvals->setItem(i, 3, new QTableWidgetItem(jure));

        QTableWidgetItem *itemNote = new QTableWidgetItem(QString("%1 / 20").arg(note, 0, 'f', 1));
        itemNote->setTextAlignment(Qt::AlignCenter);
        itemNote->setFont(QFont("Segoe UI", 9, QFont::Bold));
        itemNote->setForeground(QColor("#0BF8F6"));
        ui->tableEvals->setItem(i, 4, itemNote);

        ui->tableEvals->setItem(i, 5, new QTableWidgetItem(date));
        ui->tableEvals->setItem(i, 6, new QTableWidgetItem(comm));

        // Bouton supprimer évaluation
        QPushButton *btnDel = new QPushButton("🗑");
        btnDel->setStyleSheet("background-color: #121D34; border: 1px solid #283754; color: #EF4444; border-radius: 4px; padding: 3px;");
        connect(btnDel, &QPushButton::clicked, [this, idEval]() {
            if (QMessageBox::question(this, "Suppression", "Voulez-vous supprimer cette évaluation ?") == QMessageBox::Yes) {
                Evaluation::supprimer(idEval);
                chargerTableauEvaluations();
                chargerTableauJurys();
                actualiserStatistiquesEtGraphiques();
                actualiserClassementInnovant();
                actualiserAlertesDivergence();
            }
        });
        ui->tableEvals->setCellWidget(i, 7, btnDel);
    }

    delete model;
}

void GJury::on_tableEvals_cellClicked(int, int)
{
}

void GJury::on_inputRechercheEval_textChanged(const QString &text)
{
    for (int i = 0; i < ui->tableEvals->rowCount(); ++i) {
        bool match = false;
        for (int j = 0; j < 7; ++j) {
            if (ui->tableEvals->item(i, j) && ui->tableEvals->item(i, j)->text().contains(text, Qt::CaseInsensitive)) {
                match = true;
                break;
            }
        }
        ui->tableEvals->setRowHidden(i, !match);
    }
}

void GJury::recalculerNoteGlobaleFormulaire()
{
    double inno = ui->sliderInnovation->value();
    double tech = ui->sliderTechnique->value();
    double pres = ui->sliderPresentation->value();
    double pitch = ui->sliderPitch->value();

    ui->lblInnoTag->setText(QString("1. Innovation & Originalité (0 - 20) : <b>%1 / 20</b>").arg(inno));
    ui->lblTechTag->setText(QString("2. Faisabilité & Qualité Technique (0 - 20) : <b>%1 / 20</b>").arg(tech));
    ui->lblPresTag->setText(QString("3. Ergonomie & Expérience UX (0 - 20) : <b>%1 / 20</b>").arg(pres));
    ui->lblPitchTag->setText(QString("4. Pitch & Démonstration (0 - 20) : <b>%1 / 20</b>").arg(pitch));

    // Formule pondérée standard : 30% Innovation, 35% Technique, 20% Présentation UX, 15% Pitch
    double noteGlobale = (inno * 0.30) + (tech * 0.35) + (pres * 0.20) + (pitch * 0.15);
    ui->lblNoteCalculee->setText(QString("%1 / 20").arg(QString::number(noteGlobale, 'f', 1)));
}

void GJury::on_sliderInnovation_valueChanged(int) { recalculerNoteGlobaleFormulaire(); }
void GJury::on_sliderTechnique_valueChanged(int) { recalculerNoteGlobaleFormulaire(); }
void GJury::on_sliderPresentation_valueChanged(int) { recalculerNoteGlobaleFormulaire(); }
void GJury::on_sliderPitch_valueChanged(int) { recalculerNoteGlobaleFormulaire(); }

void GJury::on_btnEnregistrerEval_clicked()
{
    int idJury = ui->comboEvalJury->currentData().toInt();
    int idProjet = ui->comboEvalProjet->currentData().toInt();
    double inno = ui->sliderInnovation->value();
    double tech = ui->sliderTechnique->value();
    double pres = ui->sliderPresentation->value();
    double pitch = ui->sliderPitch->value();
    double noteGlobale = (inno * 0.30) + (tech * 0.35) + (pres * 0.20) + (pitch * 0.15);
    QString comm = ui->textEvalCommentaire->toPlainText();

    Evaluation eval(idJury, idProjet, noteGlobale, inno, tech, pres, pitch, comm);
    QString msg;
    if (!eval.valider(msg)) {
        QMessageBox::warning(this, "Règle de gestion violée", msg);
        return;
    }

    if (eval.ajouter()) {
        QMessageBox::information(this, "Succès", "Évaluation enregistrée et horodatée avec succès !");
        on_btnViderEval_clicked();
        chargerTableauEvaluations();
        chargerTableauJurys();
        actualiserStatistiquesEtGraphiques();
        actualiserClassementInnovant();
        actualiserAlertesDivergence();
    } else {
        QMessageBox::critical(this, "Erreur", "Impossible d'enregistrer l'évaluation.");
    }
}

void GJury::on_btnViderEval_clicked()
{
    ui->sliderInnovation->setValue(15);
    ui->sliderTechnique->setValue(16);
    ui->sliderPresentation->setValue(14);
    ui->sliderPitch->setValue(15);
    ui->textEvalCommentaire->clear();
    recalculerNoteGlobaleFormulaire();
}

// ====================================================================
// MÉTIERS INNOVANTS : CLASSEMENT ET ÉCART ENTRE JURÉS
// ====================================================================

void GJury::actualiserClassementInnovant()
{
    QVector<ClassementProjet> classement = Evaluation::calculerClassementAutomatique();
    ui->tableClassement->setRowCount(0);
    ui->tableClassement->setRowCount(classement.size());

    for (int i = 0; i < classement.size(); ++i) {
        const auto &cp = classement[i];

        // Badge Podium
        QString rangTxt = QString::number(cp.rang);
        if (cp.rang == 1) rangTxt = "🥇 1er";
        else if (cp.rang == 2) rangTxt = "🥈 2ème";
        else if (cp.rang == 3) rangTxt = "🥉 3ème";

        QTableWidgetItem *itemRang = new QTableWidgetItem(rangTxt);
        itemRang->setTextAlignment(Qt::AlignCenter);
        itemRang->setFont(QFont("Segoe UI", 10, QFont::Bold));
        if (cp.rang == 1) itemRang->setForeground(QColor("#EAB308"));
        else if (cp.rang == 2) itemRang->setForeground(QColor("#94A3B8"));
        else if (cp.rang == 3) itemRang->setForeground(QColor("#F97316"));
        ui->tableClassement->setItem(i, 0, itemRang);

        ui->tableClassement->setItem(i, 1, new QTableWidgetItem(cp.titre));
        ui->tableClassement->setItem(i, 2, new QTableWidgetItem(cp.equipe));

        QTableWidgetItem *itemNote = new QTableWidgetItem(QString("%1 / 20").arg(cp.noteMoyenne, 0, 'f', 2));
        itemNote->setTextAlignment(Qt::AlignCenter);
        itemNote->setFont(QFont("Segoe UI", 10, QFont::Bold));
        itemNote->setForeground(QColor("#0BF8F6"));
        ui->tableClassement->setItem(i, 3, itemNote);

        ui->tableClassement->setItem(i, 4, new QTableWidgetItem(QString::number(cp.scoreInnovation, 'f', 1)));
        ui->tableClassement->setItem(i, 5, new QTableWidgetItem(QString::number(cp.scoreTechnique, 'f', 1)));
        ui->tableClassement->setItem(i, 6, new QTableWidgetItem(QString::number(cp.scorePresentation, 'f', 1)));

        QString mentionInfo = cp.mention;
        if (!cp.departageInfo.isEmpty()) {
            mentionInfo += " ⚡ " + cp.departageInfo;
        }
        QTableWidgetItem *itemMention = new QTableWidgetItem(mentionInfo);
        if (!cp.departageInfo.isEmpty()) {
            itemMention->setForeground(QColor("#38BDF8"));
        }
        ui->tableClassement->setItem(i, 7, itemMention);
    }
}

void GJury::on_btnActualiserClassement_clicked()
{
    actualiserClassementInnovant();
    QMessageBox::information(this, "Classement", "Le classement automatique et le départage ont été recalculés en temps réel !");
}

void GJury::on_btnExportClassementPDF_clicked()
{
    QString fileName = QFileDialog::getSaveFileName(this, "Palmarès Officiel du Hackathon", "HackFlow_Palmares_Officiel.pdf", "Fichiers PDF (*.pdf)");
    if (!fileName.isEmpty()) {
        if (Evaluation::exporterClassementPDF(fileName)) {
            QMessageBox::information(this, "Palmarès Généré", "Le document officiel du palmarès a été exporté en PDF !");
        } else {
            QMessageBox::critical(this, "Erreur", "Impossible d'exporter le palmarès.");
        }
    }
}

void GJury::actualiserAlertesDivergence()
{
    QVector<AlerteDivergence> alertes = Evaluation::detecterEcartsEntreJures(4.0);
    ui->tableDivergences->setRowCount(0);
    ui->tableDivergences->setRowCount(alertes.size());

    for (int i = 0; i < alertes.size(); ++i) {
        const auto &a = alertes[i];

        QTableWidgetItem *itemStatut = new QTableWidgetItem("⚠️ " + a.niveauAlerte);
        itemStatut->setTextAlignment(Qt::AlignCenter);
        itemStatut->setFont(QFont("Segoe UI", 9, QFont::Bold));
        if (a.niveauAlerte == "CRITIQUE") itemStatut->setForeground(QColor("#EF4444"));
        else itemStatut->setForeground(QColor("#F59E0B"));
        ui->tableDivergences->setItem(i, 0, itemStatut);

        ui->tableDivergences->setItem(i, 1, new QTableWidgetItem(a.titreProjet));
        ui->tableDivergences->setItem(i, 2, new QTableWidgetItem(a.equipe));
        ui->tableDivergences->setItem(i, 3, new QTableWidgetItem(QString("%1 (%2)").arg(QString::number(a.noteMin, 'f', 1), a.jureNoteMin)));
        ui->tableDivergences->setItem(i, 4, new QTableWidgetItem(QString("%1 (%2)").arg(QString::number(a.noteMax, 'f', 1), a.jureNoteMax)));

        QTableWidgetItem *itemEcart = new QTableWidgetItem(QString("Δ = %1 pts (σ=%2)").arg(QString::number(a.ecart, 'f', 1), QString::number(a.ecartType, 'f', 1)));
        itemEcart->setTextAlignment(Qt::AlignCenter);
        itemEcart->setFont(QFont("Segoe UI", 9, QFont::Bold));
        itemEcart->setForeground(QColor("#EF4444"));
        ui->tableDivergences->setItem(i, 5, itemEcart);

        ui->tableDivergences->setItem(i, 6, new QTableWidgetItem(a.recommandation));
    }
}

void GJury::on_btnAnalyserDivergences_clicked()
{
    actualiserAlertesDivergence();
    QMessageBox::information(this, "Analyse de Notation", "Analyse d'écarts terminée. Les projets soumis à forte divergence sont signalés.");
}

// ====================================================================
// AUTHENTIFICATION ET PRIVILÈGES
// ====================================================================

void GJury::on_btnSeConnecter_clicked()
{
    QString email = ui->inputLoginEmail->text().trimmed();
    QString pwd = ui->inputLoginPassword->text().trimmed();
    QString role = ui->comboLoginRole->currentText();

    if (email.isEmpty() || pwd.isEmpty()) {
        QMessageBox::warning(this, "Connexion", "Veuillez saisir votre identifiant et votre mot de passe.");
        return;
    }

    m_roleCourant = role;
    m_nomUtilisateurCourant = email.split('@').first();

    ui->lblCurrentUserName->setText(m_nomUtilisateurCourant);
    ui->lblCurrentUserRole->setText(role);
    ui->lblAdminName->setText(m_nomUtilisateurCourant);
    ui->lblAdminRole->setText(role);

    appliquerPrivilegesRole();
    chargerTableauJurys();

    QMessageBox::information(this, "Connexion Réussie", QString("Bienvenue %1 !\nPrivilèges appliqués pour le rôle : %2").arg(m_nomUtilisateurCourant, role));
    on_btnNavJury_clicked();
}

void GJury::on_btnDeconnexion_clicked()
{
    ui->stackedWidget->setCurrentIndex(4); // Page Login
}

void GJury::on_btnTogglePassword_clicked()
{
    if (ui->inputLoginPassword->echoMode() == QLineEdit::Password) {
        ui->inputLoginPassword->setEchoMode(QLineEdit::Normal);
        ui->btnTogglePassword->setText("🔒");
    } else {
        ui->inputLoginPassword->setEchoMode(QLineEdit::Password);
        ui->btnTogglePassword->setText("👁");
    }
}

void GJury::appliquerPrivilegesRole()
{
    const bool admin = estAdministrateur();
    // Seul un administrateur / organisateur peut ajouter, modifier ou supprimer un juré
    ui->btnHeaderAjouter->setEnabled(admin);
    ui->btnTrierJury->setEnabled(true);
    ui->btnExporterJury->setEnabled(true);
    if (m_pageParametres) m_pageParametres->definirModificationAutorisee(admin);
    if (m_pagePlanning) m_pagePlanning->definirModificationAutorisee(admin);
    if (m_pageEvaluations) m_pageEvaluations->definirModificationAutorisee(admin);
}

// ====================================================================
// STATISTIQUES (carte « Statistiques du jury » et « Répartition par rôle »)
// ====================================================================

void GJury::actualiserStatistiquesEtGraphiques()
{
    const int dispo = Jury::compterParDisponibilite("Disponible");
    const int ligne = Jury::compterParDisponibilite("En ligne");
    const int attente = Jury::compterParDisponibilite("En attente");

    ui->lblValDispo->setText(QString::number(dispo));
    ui->lblValLigne->setText(QString::number(ligne));
    ui->lblValAttente->setText(QString::number(attente));

    if (m_donutChart) {
        QVector<QPair<QColor, int>> parts;
        parts.append(qMakePair(QColor("#02C3B4"), dispo));
        parts.append(qMakePair(QColor("#0298FE"), ligne));
        parts.append(qMakePair(QColor("#FBAB29"), attente));
        m_donutChart->setSlices(parts, "Jury(s)");
    }

    // Répartition par rôle : une ligne = carré coloré + libellé + barre + effectif
    static const QStringList carres = {"#0184FD", "#00B5B1", "#F9A81C", "#DA489A", "#453EE0", "#015FD8"};
    static const QStringList barres = {"#0081FB", "#01B5B0", "#F9AC32", "#DF4599", "#503FF4", "#4CC3F9"};

    QVBoxLayout *lay = ui->rolesRowsVLayout;
    viderLayout(lay);

    const QVector<QPair<QString, int>> repartition = Jury::repartitionParRole();
    int maximum = 1;
    for (const auto &p : repartition) maximum = qMax(maximum, p.second);

    for (int i = 0; i < repartition.size(); ++i) {
        const QString &nom = repartition.at(i).first;
        const int effectif = repartition.at(i).second;

        QWidget *ligne = new QWidget();
        ligne->setFixedHeight(21);
        ligne->setStyleSheet("background: transparent;");
        QHBoxLayout *h = new QHBoxLayout(ligne);
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(10);

        QLabel *carre = new QLabel();
        carre->setFixedSize(19, 19);
        carre->setStyleSheet(QString("background-color: %1; border-radius: 4px;").arg(carres.at(i % carres.size())));

        QLabel *libelle = new QLabel(nom);
        libelle->setMinimumWidth(76);
        libelle->setStyleSheet("color: #E6EEFF; font-size: 12px; background: transparent;");

        QProgressBar *barre = new QProgressBar();
        barre->setFixedSize(101, 7);
        barre->setRange(0, 100);
        barre->setValue(20 + 60 * effectif / maximum);
        barre->setTextVisible(false);
        barre->setStyleSheet(QString("QProgressBar { background-color: #002B6F; border: none; border-radius: 3px; } "
                                     "QProgressBar::chunk { background-color: %1; border-radius: 3px; }").arg(barres.at(i % barres.size())));

        QLabel *compte = new QLabel(QString::number(effectif));
        compte->setMinimumWidth(14);
        compte->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        compte->setStyleSheet("color: #FFFFFF; font-size: 12px; font-weight: bold; background: transparent;");

        h->addWidget(carre);
        h->addWidget(libelle);
        h->addWidget(barre);
        h->addStretch();
        h->addWidget(compte);
        lay->addWidget(ligne);
    }
}

// ====================================================================
// NAVIGATION DANS LA SIDEBAR
// ====================================================================

void GJury::definirNavActif(QPushButton *actif)
{
    const QList<QPushButton *> boutons = {
        ui->btnNavAccueil, ui->btnNavParticipants, ui->btnNavProjets, ui->btnNavEmployes,
        ui->btnNavJury, ui->btnNavParametres
    };
    for (QPushButton *b : boutons) {
        b->setProperty("class", b == actif ? "navButtonActive" : "navButton");
        // Obligatoire : Qt ne réapplique pas la feuille de style quand une propriété dynamique change
        b->style()->unpolish(b);
        b->style()->polish(b);
        b->updateGeometry();
        b->update();
    }
}

void GJury::on_btnNavAccueil_clicked()
{
    ui->stackedWidget->setCurrentIndex(3);
    definirNavActif(ui->btnNavAccueil);
    ui->lblBreadcrumb->setText("🏠  >  SmartHackathon Management  >  Tableau de bord");
}

void GJury::on_btnNavJury_clicked()
{
    ui->stackedWidget->setCurrentIndex(0);
    definirNavActif(ui->btnNavJury);
    ui->lblBreadcrumb->setText("🏠  >  SmartHackathon Management  >  Gestion des Jurys  >  Module 6");
}

void GJury::on_btnNavEvals_clicked()
{
    ui->stackedWidget->setCurrentIndex(1);
    definirNavActif(ui->btnNavJury);   // les évaluations font partie du module Jury
    ui->lblBreadcrumb->setText("🏠  >  SmartHackathon Management  >  Évaluations & Notes");
}

void GJury::on_btnNavInnovant_clicked()
{
    ui->stackedWidget->setCurrentIndex(2);
    definirNavActif(ui->btnNavJury);
    ui->lblBreadcrumb->setText("🏠  >  SmartHackathon Management  >  Métiers Innovants (Classement & Alertes)");
}

void GJury::on_btnNavConnexion_clicked()
{
    ui->stackedWidget->setCurrentIndex(4);
    definirNavActif(ui->btnNavParametres);
    ui->lblBreadcrumb->setText("🏠  >  SmartHackathon Management  >  Authentification");
}

void GJury::on_btnNavParametres_clicked()
{
    on_btnNavConnexion_clicked();
}

static void moduleNonInclus(QWidget *parent, const QString &nom)
{
    QMessageBox::information(parent, nom,
                             QString("Le module « %1 » est développé par un autre membre de l'équipe "
                                     "et n'est pas inclus dans ce module (Gestion des Jurys).").arg(nom));
}

void GJury::on_btnNavParticipants_clicked() { moduleNonInclus(this, "Participants"); }
void GJury::on_btnNavProjets_clicked()      { moduleNonInclus(this, "Projets"); }
void GJury::on_btnNavEmployes_clicked()     { moduleNonInclus(this, "Employés"); }

// ---- Raccourcis de la colonne de droite ----

void GJury::on_btnRaccEvals_clicked()
{
    // Ouvre la page « Saisie et Grille d'Évaluation des Projets » ; la page Jury (page 0) reste intacte derrière
    m_pageEvaluations->definirModificationAutorisee(estAdministrateur());
    m_pageEvaluations->rafraichir();
    ui->stackedWidget->setCurrentWidget(m_pageEvaluations);
    definirNavActif(ui->btnNavJury);
    ui->lblBreadcrumb->setText("🏠  >  SmartHackathon Management  >  Gestion des Jurys  >  Saisie et Grille d'Évaluation");
}
void GJury::on_btnRaccEquipes_clicked()    { on_btnNavInnovant_clicked(); }
void GJury::on_btnRaccPlanning_clicked()
{
    // Ouvre la page « Planning des évaluations » (page 6) ; la page Jury (page 0) reste intacte derrière
    m_pagePlanning->definirModificationAutorisee(estAdministrateur());
    m_pagePlanning->rafraichir();
    ui->stackedWidget->setCurrentWidget(m_pagePlanning);
    definirNavActif(ui->btnNavJury);
    ui->lblBreadcrumb->setText("🏠  >  SmartHackathon Management  >  Gestion des Jurys  >  Planning des évaluations");
}
void GJury::on_btnRaccParametres_clicked()
{
    // Ouvre la page « Paramètres du jury » (page 5) ; la page Jury (page 0) reste intacte derrière
    m_pageParametres->definirModificationAutorisee(estAdministrateur());
    m_pageParametres->rafraichir();
    ui->stackedWidget->setCurrentWidget(m_pageParametres);
    definirNavActif(ui->btnNavJury);
    ui->lblBreadcrumb->setText("🏠  >  SmartHackathon Management  >  Gestion des Jurys  >  Paramètres du jury");
}

void GJury::retourDepuisParametres()
{
    on_btnNavJury_clicked();   // retour à l'interface principale Jury
}

void GJury::configurerPageParametres()
{
    m_pageParametres = new ParametresJuryPage(ui->stackedWidget);
    ui->stackedWidget->addWidget(m_pageParametres);

    connect(m_pageParametres, &ParametresJuryPage::retourDemande, this, &GJury::retourDepuisParametres);
    connect(m_pageParametres, &ParametresJuryPage::consulterJuryDemande, this, [this](int id) { afficherDetailJury(id); });
    connect(m_pageParametres, &ParametresJuryPage::modifierJuryDemande, this, [this](int id) { ouvrirDialogueJury(id); });
    m_pageParametres->definirModificationAutorisee(estAdministrateur());
}

void GJury::configurerPagePlanning()
{
    m_pagePlanning = new PlanningEvaluationsPage(ui->stackedWidget);
    ui->stackedWidget->addWidget(m_pagePlanning);

    connect(m_pagePlanning, &PlanningEvaluationsPage::retourDemande, this, &GJury::retourDepuisParametres);
    m_pagePlanning->definirModificationAutorisee(estAdministrateur());
}

void GJury::configurerPageEvaluations()
{
    m_pageEvaluations = new EvaluationProjetsPage(ui->stackedWidget);
    ui->stackedWidget->addWidget(m_pageEvaluations);

    connect(m_pageEvaluations, &EvaluationProjetsPage::retourDemande, this, &GJury::retourDepuisParametres);
    // Une évaluation ajoutée / modifiée / supprimée met à jour les autres pages qui la lisent
    connect(m_pageEvaluations, &EvaluationProjetsPage::evaluationsModifiees, this, [this]() {
        chargerTableauEvaluations();
        actualiserClassementInnovant();
        actualiserAlertesDivergence();
        if (m_pagePlanning) m_pagePlanning->rafraichir();
    });
    m_pageEvaluations->definirModificationAutorisee(estAdministrateur());
}
