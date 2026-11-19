#include "parametresjury.h"
#include "jury.h"
#include "sqlutils.h"
#include "uihelpers.h"

#include <QActionGroup>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QDebug>
#include <QEvent>
#include <QFontMetrics>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlQueryModel>
#include <QSqlRecord>
#include <QTableWidget>
#include <QVBoxLayout>
#include <algorithm>

namespace {

const int kColCoche = 0, kColNom = 1, kColPrenom = 2, kColSpecialite = 3,
          kColEquipe = 4, kColStatut = 5, kColActions = 6;

const QString kCarteCss =
    "QFrame#carteParam { background-color: #011846; border: 1px solid #14449A; border-radius: 8px; }";

using UiHelpers::EnteteTri;
using UiHelpers::initiales;

const QString &kCheckboxCss = UiHelpers::cssCheckbox();
const QString &kMenuCss = UiHelpers::cssMenu();

QPushButton *creerBoutonAction(const QString &icone, const QString &info)
{
    return UiHelpers::boutonAction(icone, info);
}

} // namespace

// ====================================================================
// PERSISTANCE
// ====================================================================

ParametresJury ParametresJury::charger()
{
    ParametresJury p;
    p.nombreJurys = qMax(1, Jury::compterTotal());   // valeur par défaut : effectif réel des jurés

    QSqlQuery q;
    if (!q.exec(QStringLiteral("SELECT cle, valeur FROM PARAMETRES_JURY"))) {
        signalerErreurSql(QStringLiteral("Impossible de charger les paramètres du jury"), q.lastError());
        return p;
    }
    while (q.next()) {
        const QString cle = q.value(0).toString();
        bool ok = false;
        const int v = q.value(1).toString().toInt(&ok);
        if (!ok) continue;
        if (cle == "nombre_jurys")            p.nombreJurys = v;
        else if (cle == "membres_par_session") p.membresParSession = v;
        else if (cle == "duree_evaluation")    p.dureeEvaluationMin = v;
        else if (cle == "note_totale")         p.noteTotaleParProjet = v;
    }
    return p;
}

bool ParametresJury::enregistrer(QString *messageErreur) const
{
    QSqlDatabase db = QSqlDatabase::database();

    if (!db.transaction()) {
        const QString message = QStringLiteral("Impossible de démarrer la transaction : %1")
                                    .arg(db.lastError().text());
        qWarning() << "Erreur ouverture transaction paramètres jury:" << db.lastError().text();
        if (messageErreur) *messageErreur = message;
        return false;
    }

    const QList<QPair<QString, int>> valeurs = {
        {"nombre_jurys", nombreJurys},
        {"membres_par_session", membresParSession},
        {"duree_evaluation", dureeEvaluationMin},
        {"note_totale", noteTotaleParProjet}};

    for (const auto &v : valeurs) {
        QSqlQuery del;
        del.prepare("DELETE FROM PARAMETRES_JURY WHERE cle = ?");
        del.addBindValue(v.first);
        if (!del.exec()) {
            const QString message = QStringLiteral("Impossible de mettre à jour le paramètre « %1 » : %2")
                                        .arg(v.first, del.lastError().text());
            qWarning() << "Erreur suppression paramètre" << v.first << del.lastError().text();
            if (!db.rollback())
                qWarning() << "Erreur rollback paramètres jury:" << db.lastError().text();
            if (messageErreur) *messageErreur = message;
            return false;
        }

        QSqlQuery q;
        q.prepare("INSERT INTO PARAMETRES_JURY (cle, valeur) VALUES (?, ?)");
        q.addBindValue(v.first);
        q.addBindValue(QString::number(v.second));
        if (!q.exec()) {
            const QString message = QStringLiteral("Impossible d'enregistrer le paramètre « %1 » : %2")
                                        .arg(v.first, q.lastError().text());
            qWarning() << "Erreur enregistrement paramètre" << v.first << q.lastError().text();
            if (!db.rollback())
                qWarning() << "Erreur rollback paramètres jury:" << db.lastError().text();
            if (messageErreur) *messageErreur = message;
            return false;
        }
    }

    if (!db.commit()) {
        const QString message = QStringLiteral("Impossible de valider la transaction : %1")
                                    .arg(db.lastError().text());
        qWarning() << "Erreur validation transaction paramètres jury:" << db.lastError().text();
        if (!db.rollback())
            qWarning() << "Erreur rollback après échec de validation:" << db.lastError().text();
        if (messageErreur) *messageErreur = message;
        return false;
    }

    if (messageErreur) messageErreur->clear();
    return true;
}

// ====================================================================
// PAGE
// ====================================================================

ParametresJuryPage::ParametresJuryPage(QWidget *parent)
    : QWidget(parent),
      m_spinJurys(nullptr), m_spinMembres(nullptr), m_spinDuree(nullptr), m_spinNote(nullptr),
      m_btnEnregistrer(nullptr), m_inputRecherche(nullptr), m_btnFiltrer(nullptr), m_menuFiltre(nullptr),
      m_table(nullptr), m_lblAffichage(nullptr), m_btnPrecedent(nullptr),
      m_btnSuivant(nullptr), m_pagesLayout(nullptr),
      m_triColonne(-1), m_triCroissant(true), m_page(1), m_taillePage(8),
      m_modificationAutorisee(true)
{
    setObjectName("pageParametresJury");
    construireInterface();
    rafraichir();
}

void ParametresJuryPage::construireInterface()
{
    // Page défilante (comme la page principale des jurés) pour les petites fenêtres
    QVBoxLayout *externe = new QVBoxLayout(this);
    externe->setContentsMargins(0, 0, 0, 0);

    QScrollArea *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet("QScrollArea { background: transparent; border: none; } "
                          "QScrollArea > QWidget > QWidget { background: transparent; } "
                          "QScrollBar:vertical { background: transparent; width: 10px; } "
                          "QScrollBar::handle:vertical { background: #1F57A4; border-radius: 5px; min-height: 30px; } "
                          "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }");
    externe->addWidget(scroll);

    QWidget *contenu = new QWidget();
    contenu->setObjectName("contenuParametresJury");
    scroll->setWidget(contenu);

    QVBoxLayout *principal = new QVBoxLayout(contenu);
    principal->setContentsMargins(22, 22, 22, 22);
    principal->setSpacing(20);

    principal->addWidget(construireEnTete());

    QHBoxLayout *corps = new QHBoxLayout();
    corps->setSpacing(20);
    QWidget *carteInfos = construireCarteInfos();
    QWidget *carteListe = construireCarteListe();
    corps->addWidget(carteInfos, 0);
    corps->addWidget(carteListe, 1);
    principal->addLayout(corps, 1);
}

QWidget *ParametresJuryPage::construireEnTete()
{
    QWidget *w = new QWidget();
    w->setStyleSheet("background: transparent;");
    QHBoxLayout *h = new QHBoxLayout(w);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(16);

    QPushButton *btnRetour = new QPushButton(" Retour");
    btnRetour->setObjectName("btnRetourParametres");
    btnRetour->setIcon(QIcon(":/img/arrow_left.png"));
    btnRetour->setIconSize(QSize(18, 18));
    btnRetour->setFixedSize(110, 42);
    btnRetour->setCursor(Qt::PointingHandCursor);
    btnRetour->setStyleSheet("QPushButton { background-color: #011846; color: #FFFFFF; border: 1px solid #1E5BC0; "
                             "border-radius: 6px; font-size: 13px; font-weight: 600; } "
                             "QPushButton:hover { background-color: #06306F; border-color: #3B82F6; } "
                             "QPushButton:pressed { background-color: #031A48; }");
    connect(btnRetour, &QPushButton::clicked, this, &ParametresJuryPage::retourDemande);

    QLabel *icone = new QLabel();
    QPixmap pm(":/img/title_award.png");
    pm.setDevicePixelRatio(80.0 / 46.0);
    icone->setPixmap(pm);
    icone->setFixedSize(46, 46);
    icone->setStyleSheet("background: transparent;");

    QVBoxLayout *textes = new QVBoxLayout();
    textes->setSpacing(2);
    QLabel *titre = new QLabel("Paramètres du jury");
    titre->setStyleSheet("color: #FFFFFF; font-size: 21px; font-weight: bold; background: transparent;");
    QLabel *sousTitre = new QLabel("Gérez les paramètres du jury et leurs affectations.");
    sousTitre->setStyleSheet("color: #19C3F0; font-size: 12px; background: transparent;");
    textes->addWidget(titre);
    textes->addWidget(sousTitre);

    h->addWidget(btnRetour, 0, Qt::AlignVCenter);
    h->addWidget(icone, 0, Qt::AlignVCenter);
    h->addLayout(textes);
    h->addStretch();
    return w;
}

QWidget *ParametresJuryPage::creerLigneSpin(const QString &libelle, const QString &icone,
                                            QSpinBox *spin, const QString &suffixe)
{
    QWidget *w = new QWidget();
    w->setStyleSheet("background: transparent;");
    QVBoxLayout *v = new QVBoxLayout(w);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(8);

    QLabel *lbl = new QLabel(libelle);
    lbl->setStyleSheet("color: #E6EEFF; font-size: 13px; font-weight: 500; background: transparent;");
    v->addWidget(lbl);

    QHBoxLayout *h = new QHBoxLayout();
    h->setSpacing(12);
    QLabel *ic = new QLabel();
    QPixmap pm(":/img/" + icone);
    pm.setDevicePixelRatio(pm.width() / 26.0);
    ic->setPixmap(pm);
    ic->setFixedSize(26, 26);
    ic->setStyleSheet("background: transparent;");
    h->addWidget(ic);

    spin->setFixedHeight(38);
    spin->setCursor(Qt::PointingHandCursor);
    spin->setStyleSheet(
        "QSpinBox { background-color: #01286C; border: 1px solid #1F57A4; border-radius: 6px; "
        "padding-left: 12px; padding-right: 30px; color: #FFFFFF; font-size: 13px; selection-background-color: #0B6BFF; } "
        "QSpinBox:focus { border: 1px solid #3B82F6; } "
        "QSpinBox:disabled { color: #7F93B8; } "
        "QSpinBox::up-button { subcontrol-origin: border; subcontrol-position: top right; width: 28px; "
        "border: none; background: transparent; margin-top: 3px; } "
        "QSpinBox::down-button { subcontrol-origin: border; subcontrol-position: bottom right; width: 28px; "
        "border: none; background: transparent; margin-bottom: 3px; } "
        "QSpinBox::up-arrow { image: url(:/img/chevron_up.png); width: 12px; height: 12px; } "
        "QSpinBox::down-arrow { image: url(:/img/chevron_down.png); width: 12px; height: 12px; }");
    h->addWidget(spin, 1);
    v->addLayout(h);

    if (!suffixe.isEmpty()) {
        // Unité (« min ») alignée à droite dans le champ, avant les flèches
        QLabel *unite = new QLabel(suffixe, spin);
        unite->setObjectName("lblUnite");
        unite->setStyleSheet("color: #E6EEFF; font-size: 13px; background: transparent;");
        unite->setAttribute(Qt::WA_TransparentForMouseEvents);
        unite->adjustSize();
        spin->installEventFilter(this);
    }
    return w;
}

QWidget *ParametresJuryPage::construireCarteInfos()
{
    QFrame *carte = new QFrame();
    carte->setObjectName("carteParam");
    carte->setStyleSheet(kCarteCss);
    carte->setFixedWidth(315);
    carte->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);

    QVBoxLayout *v = new QVBoxLayout(carte);
    v->setContentsMargins(16, 16, 16, 18);
    v->setSpacing(16);

    // En-tête de la carte
    QHBoxLayout *entete = new QHBoxLayout();
    entete->setSpacing(10);
    QLabel *ic = new QLabel();
    QPixmap pm(":/img/sc_gear.png");
    pm.setDevicePixelRatio(pm.width() / 24.0);
    ic->setPixmap(pm);
    ic->setFixedSize(24, 24);
    ic->setStyleSheet("background: transparent; border: none;");
    QLabel *titre = new QLabel("Informations générales");
    titre->setStyleSheet("color: #FFFFFF; font-size: 15px; font-weight: bold; background: transparent; border: none;");
    entete->addWidget(ic);
    entete->addWidget(titre);
    entete->addStretch();
    v->addLayout(entete);

    QFrame *separateur = new QFrame();
    separateur->setFixedHeight(1);
    separateur->setStyleSheet("background-color: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #1F57A4, stop:1 transparent); border: none;");
    v->addWidget(separateur);

    m_spinJurys = new QSpinBox();
    m_spinJurys->setRange(1, 99);
    m_spinMembres = new QSpinBox();
    m_spinMembres->setRange(1, 99);
    m_spinDuree = new QSpinBox();
    m_spinDuree->setRange(1, 240);
    m_spinNote = new QSpinBox();
    m_spinNote->setRange(1, 100);
    for (QSpinBox *s : {m_spinJurys, m_spinMembres, m_spinDuree, m_spinNote}) s->setKeyboardTracking(false);

    v->addWidget(creerLigneSpin("Nombre de jurys", "sc_users.png", m_spinJurys));
    v->addWidget(creerLigneSpin("Nombre de membres par session", "sc_users.png", m_spinMembres));
    v->addWidget(creerLigneSpin("Durée d'évaluation (par projet)", "clock.png", m_spinDuree, "min"));
    v->addWidget(creerLigneSpin("Note totale par projet", "star_fill.png", m_spinNote));

    m_btnEnregistrer = new QPushButton(" Enregistrer les paramètres");
    m_btnEnregistrer->setObjectName("btnEnregistrerParametres");
    m_btnEnregistrer->setIcon(QIcon(":/img/save.png"));
    m_btnEnregistrer->setIconSize(QSize(18, 18));
    m_btnEnregistrer->setFixedHeight(40);
    m_btnEnregistrer->setCursor(Qt::PointingHandCursor);
    m_btnEnregistrer->setStyleSheet(
        "QPushButton { background-color: #0391FC; color: #FFFFFF; border: none; border-radius: 6px; font-size: 13px; font-weight: 600; } "
        "QPushButton:hover { background-color: #1AA0FF; } "
        "QPushButton:pressed { background-color: #0279D6; } "
        "QPushButton:disabled { background-color: #1A3A6B; color: #7F93B8; }");
    connect(m_btnEnregistrer, &QPushButton::clicked, this, &ParametresJuryPage::enregistrerParametres);
    v->addSpacing(4);
    v->addWidget(m_btnEnregistrer);
    v->addStretch();
    return carte;
}

QWidget *ParametresJuryPage::construireCarteListe()
{
    QFrame *carte = new QFrame();
    carte->setObjectName("carteParam");
    carte->setStyleSheet(kCarteCss);

    QVBoxLayout *v = new QVBoxLayout(carte);
    v->setContentsMargins(14, 14, 14, 14);
    v->setSpacing(14);

    // ---- Ligne titre + recherche + filtre ----
    QHBoxLayout *entete = new QHBoxLayout();
    entete->setSpacing(12);
    QLabel *ic = new QLabel();
    QPixmap pm(":/img/sc_users.png");
    pm.setDevicePixelRatio(pm.width() / 26.0);
    ic->setPixmap(pm);
    ic->setFixedSize(26, 26);
    ic->setStyleSheet("background: transparent; border: none;");
    QLabel *titre = new QLabel("Liste des jurys");
    titre->setStyleSheet("color: #FFFFFF; font-size: 16px; font-weight: bold; background: transparent; border: none;");
    entete->addWidget(ic);
    entete->addWidget(titre);
    entete->addStretch();

    m_inputRecherche = new QLineEdit();
    m_inputRecherche->setObjectName("inputRechercheParametres");
    m_inputRecherche->setPlaceholderText("Rechercher un juré...");
    m_inputRecherche->setClearButtonEnabled(true);
    m_inputRecherche->setFixedSize(268, 40);
    m_inputRecherche->setStyleSheet("QLineEdit { background-color: #01286C; border: 1px solid #1F57A4; border-radius: 8px; "
                                    "padding-left: 38px; padding-right: 8px; color: #EAF2FF; font-size: 12px; } "
                                    "QLineEdit:focus { border: 1px solid #3B82F6; }");
    QPalette pal = m_inputRecherche->palette();
    pal.setColor(QPalette::PlaceholderText, QColor("#BDD6FF"));
    m_inputRecherche->setPalette(pal);
    QLabel *loupe = new QLabel(m_inputRecherche);
    QPixmap pl(":/img/search.png");
    pl.setDevicePixelRatio(pl.width() / 18.0);
    loupe->setPixmap(pl);
    loupe->setGeometry(13, 11, 18, 18);
    loupe->setStyleSheet("background: transparent; border: none;");
    loupe->setAttribute(Qt::WA_TransparentForMouseEvents);
    connect(m_inputRecherche, &QLineEdit::textChanged, this, &ParametresJuryPage::surRecherche);
    entete->addWidget(m_inputRecherche);

    m_btnFiltrer = new QPushButton(" Filtrer");
    m_btnFiltrer->setObjectName("btnFiltrerParametres");
    m_btnFiltrer->setIcon(QIcon(":/img/filter.png"));
    m_btnFiltrer->setIconSize(QSize(16, 16));
    m_btnFiltrer->setFixedSize(90, 40);
    m_btnFiltrer->setCursor(Qt::PointingHandCursor);
    m_menuFiltre = new QMenu(this);
    m_menuFiltre->setStyleSheet(kMenuCss);
    connect(m_btnFiltrer, &QPushButton::clicked, this, [this]() {
        m_menuFiltre->exec(m_btnFiltrer->mapToGlobal(QPoint(0, m_btnFiltrer->height() + 4)));
    });
    entete->addWidget(m_btnFiltrer);
    v->addLayout(entete);

    // ---- Tableau ----
    m_table = new QTableWidget();
    m_table->setObjectName("tableParametresJurys");
    EnteteTri *entetes = new EnteteTri(m_table);
    entetes->triables = {kColNom, kColStatut};
    entetes->margesGauche[kColNom] = 0;
    m_table->setHorizontalHeader(entetes);
    m_table->setColumnCount(7);
    m_table->setHorizontalHeaderLabels({"", "Nom", "Prénom", "Spécialité", "Équipe", "Statut", "Actions"});
    m_table->verticalHeader()->setVisible(false);
    m_table->verticalHeader()->setDefaultSectionSize(47);
    m_table->setShowGrid(false);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setFocusPolicy(Qt::NoFocus);
    m_table->setWordWrap(false);
    m_table->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_table->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_table->setFrameShape(QFrame::NoFrame);
    m_table->setStyleSheet("QTableWidget { background: transparent; border: none; color: #E6EEFF; font-size: 12px; } "
                           "QTableWidget::item { border-bottom: 1px solid #0F3070; padding-left: 4px; }");
    entetes->setSectionResizeMode(QHeaderView::Fixed);
    entetes->setStretchLastSection(false);
    entetes->setFixedHeight(44);
    entetes->setSectionsClickable(true);
    entetes->setHighlightSections(false);
    entetes->setStyleSheet("QHeaderView { background: transparent; border: none; }");
    entetes->viewport()->setAutoFillBackground(false);
    connect(entetes, &QHeaderView::sectionClicked, this, &ParametresJuryPage::surTriDemande);
    m_table->viewport()->installEventFilter(this);
    v->addWidget(m_table, 1);

    // ---- Pied : compteur + pagination ----
    QHBoxLayout *pied = new QHBoxLayout();
    pied->setSpacing(8);
    m_lblAffichage = new QLabel();
    m_lblAffichage->setStyleSheet("color: #B8C7E8; font-size: 11px; background: transparent; border: none;");
    pied->addWidget(m_lblAffichage);
    pied->addStretch();

    const QString cssNav = "QPushButton { background-color: transparent; border: 1px solid #1F57A4; border-radius: 6px; color: #FFFFFF; "
                           "font-size: 12px; font-weight: bold; } "
                           "QPushButton:hover { background-color: #173266; border-color: #3B82F6; } "
                           "QPushButton:disabled { border-color: #12326B; }";
    m_btnPrecedent = new QPushButton();
    m_btnPrecedent->setObjectName("btnPagePrecedenteParametres");
    m_btnPrecedent->setIcon(QIcon(":/img/page_prev.png"));
    m_btnPrecedent->setIconSize(QSize(14, 14));
    m_btnPrecedent->setFixedSize(34, 34);
    m_btnPrecedent->setCursor(Qt::PointingHandCursor);
    m_btnPrecedent->setStyleSheet(cssNav);
    m_btnPrecedent->setToolTip("Page précédente");
    connect(m_btnPrecedent, &QPushButton::clicked, this, [this]() { allerPage(m_page - 1); });

    m_btnSuivant = new QPushButton();
    m_btnSuivant->setObjectName("btnPageSuivanteParametres");
    m_btnSuivant->setIcon(QIcon(":/img/page_next.png"));
    m_btnSuivant->setIconSize(QSize(14, 14));
    m_btnSuivant->setFixedSize(34, 34);
    m_btnSuivant->setCursor(Qt::PointingHandCursor);
    m_btnSuivant->setStyleSheet(cssNav);
    m_btnSuivant->setToolTip("Page suivante");
    connect(m_btnSuivant, &QPushButton::clicked, this, [this]() { allerPage(m_page + 1); });

    m_pagesLayout = new QHBoxLayout();
    m_pagesLayout->setSpacing(8);
    m_pagesLayout->setContentsMargins(0, 0, 0, 0);

    pied->addWidget(m_btnPrecedent);
    pied->addLayout(m_pagesLayout);
    pied->addWidget(m_btnSuivant);
    v->addLayout(pied);
    return carte;
}

// ====================================================================
// DONNÉES
// ====================================================================

void ParametresJuryPage::rafraichir()
{
    chargerParametres();
    reconstruireMenuFiltre();
    chargerListe();
}

void ParametresJuryPage::definirModificationAutorisee(bool autorisee)
{
    m_modificationAutorisee = autorisee;
    for (QSpinBox *s : {m_spinJurys, m_spinMembres, m_spinDuree, m_spinNote}) s->setEnabled(autorisee);
    m_btnEnregistrer->setEnabled(autorisee);
    m_btnEnregistrer->setToolTip(autorisee ? QString() : "Réservé aux administrateurs / organisateurs");
    chargerListe();
}

void ParametresJuryPage::chargerParametres()
{
    const ParametresJury p = ParametresJury::charger();
    m_spinJurys->setValue(p.nombreJurys);
    m_spinMembres->setValue(p.membresParSession);
    m_spinDuree->setValue(p.dureeEvaluationMin);
    m_spinNote->setValue(p.noteTotaleParProjet);
}

int ParametresJuryPage::nombreTotalJurys() const
{
    return Jury::compterTotal();
}

QVector<ParametresJuryPage::Ligne> ParametresJuryPage::lireJurys() const
{
    QVector<Ligne> res;
    QSqlQueryModel *model = Jury::rechercher();
    while (model->canFetchMore()) model->fetchMore();
    for (int i = 0; i < model->rowCount(); ++i) {
        const QSqlRecord rec = model->record(i);
        Ligne l;
        l.id = rec.value(0).toInt();
        l.nom = rec.value(1).toString();
        l.prenom = rec.value(2).toString();
        l.specialite = rec.value(5).toString();   // rôle du juré = spécialité
        l.equipe = rec.value(6).toString();
        // « Actif » : juré mobilisable (Disponible / En ligne) ; « Inactif » : en attente
        l.actif = rec.value(7).toString() != "En attente";
        res.append(l);
    }
    delete model;
    return res;
}

void ParametresJuryPage::chargerListe()
{
    if (!m_table) return;
    QVector<Ligne> lignes = lireJurys();

    // Filtres : texte, statut, équipe
    const QString recherche = m_inputRecherche->text().trimmed();
    QVector<Ligne> filtrees;
    for (const Ligne &l : lignes) {
        if (!m_filtreStatut.isEmpty() && (l.actif ? "Actif" : "Inactif") != m_filtreStatut) continue;
        if (!m_filtreEquipe.isEmpty() && l.equipe != m_filtreEquipe) continue;
        if (!recherche.isEmpty()) {
            const QString bloc = l.nom + ' ' + l.prenom + ' ' + l.prenom + ' ' + l.nom + ' ' + l.specialite + ' ' +
                                 l.equipe + ' ' + (l.actif ? "Actif" : "Inactif");
            if (!bloc.contains(recherche, Qt::CaseInsensitive)) continue;
        }
        filtrees.append(l);
    }

    // Tri (Nom ou Statut)
    if (m_triColonne == kColNom) {
        std::stable_sort(filtrees.begin(), filtrees.end(), [this](const Ligne &a, const Ligne &b) {
            const int c = QString::localeAwareCompare(a.nom.toLower(), b.nom.toLower());
            return m_triCroissant ? c < 0 : c > 0;
        });
    } else if (m_triColonne == kColStatut) {
        std::stable_sort(filtrees.begin(), filtrees.end(), [this](const Ligne &a, const Ligne &b) {
            return m_triCroissant ? (a.actif && !b.actif) : (!a.actif && b.actif);
        });
    }
    if (EnteteTri *e = dynamic_cast<EnteteTri *>(m_table->horizontalHeader())) {
        e->colonneActive = m_triColonne;
        e->viewport()->update();
    }

    // Pagination
    const int total = filtrees.size();
    const int nbPages = qMax(1, (total + m_taillePage - 1) / m_taillePage);
    m_page = qBound(1, m_page, nbPages);
    const int debut = (m_page - 1) * m_taillePage;
    const int fin = qMin(total, debut + m_taillePage);

    m_table->setRowCount(0);
    m_table->setRowCount(fin - debut);
    for (int i = debut; i < fin; ++i) {
        const int r = i - debut;
        const Ligne &l = filtrees.at(i);

        // Case à cocher
        QWidget *cCoche = new QWidget();
        cCoche->setStyleSheet("background: transparent;");
        QHBoxLayout *lc = new QHBoxLayout(cCoche);
        lc->setContentsMargins(0, 0, 0, 0);
        QCheckBox *chk = new QCheckBox();
        chk->setStyleSheet(kCheckboxCss);
        chk->setCursor(Qt::PointingHandCursor);
        connect(chk, &QCheckBox::toggled, this, &ParametresJuryPage::surCaseLigne);
        lc->addWidget(chk, 0, Qt::AlignCenter);
        m_table->setCellWidget(r, kColCoche, cCoche);

        // Avatar + nom
        QWidget *cNom = new QWidget();
        cNom->setStyleSheet("background: transparent;");
        QHBoxLayout *ln = new QHBoxLayout(cNom);
        ln->setContentsMargins(0, 0, 0, 0);
        ln->setSpacing(12);
        QLabel *avatar = new QLabel(initiales(l.nom));
        avatar->setFixedSize(28, 28);
        avatar->setAlignment(Qt::AlignCenter);
        avatar->setStyleSheet(QString("background-color: %1; color: #FFFFFF; border-radius: 14px; font-size: 10px; font-weight: bold;")
                                  .arg(UiHelpers::couleurAvatar(l.id - 1)));
        QLabel *lblNom = new QLabel(l.nom);
        lblNom->setStyleSheet("color: #E6EEFF; font-size: 12px; background: transparent;");
        ln->addWidget(avatar);
        ln->addWidget(lblNom, 1);
        m_table->setCellWidget(r, kColNom, cNom);
        QTableWidgetItem *itNom = new QTableWidgetItem();   // porte la bordure basse de la ligne
        itNom->setData(Qt::UserRole, l.id);
        m_table->setItem(r, kColNom, itNom);

        auto texte = [&](int col, const QString &t) {
            QTableWidgetItem *it = new QTableWidgetItem(t);
            it->setToolTip(t);
            it->setData(Qt::UserRole, l.id);
            m_table->setItem(r, col, it);
        };
        texte(kColPrenom, l.prenom);
        texte(kColSpecialite, l.specialite);
        texte(kColEquipe, l.equipe);
        m_table->setItem(r, kColCoche, new QTableWidgetItem());

        // Badge de statut
        QWidget *cStatut = new QWidget();
        cStatut->setStyleSheet("background: transparent;");
        QHBoxLayout *ls = new QHBoxLayout(cStatut);
        ls->setContentsMargins(4, 0, 0, 0);
        QLabel *badge = new QLabel(l.actif ? "Actif" : "Inactif");
        badge->setAlignment(Qt::AlignCenter);
        badge->setFixedSize(l.actif ? 54 : 56, 26);
        badge->setStyleSheet(QString("background-color: %1; color: #FFFFFF; border-radius: 5px; font-size: 11px; font-weight: bold;")
                                 .arg(l.actif ? "#087F6B" : "#F0426A"));
        ls->addWidget(badge);
        ls->addStretch();
        m_table->setCellWidget(r, kColStatut, cStatut);
        m_table->setItem(r, kColStatut, new QTableWidgetItem());

        // Actions : consulter / modifier
        QWidget *cActions = new QWidget();
        cActions->setStyleSheet("background: transparent;");
        QHBoxLayout *la = new QHBoxLayout(cActions);
        la->setContentsMargins(4, 0, 0, 0);
        la->setSpacing(12);
        QPushButton *btnVoir = creerBoutonAction("act_eye.png", "Consulter le profil");
        QPushButton *btnEdit = creerBoutonAction("act_edit.png", "Modifier");
        btnEdit->setEnabled(m_modificationAutorisee);
        la->addWidget(btnVoir);
        la->addWidget(btnEdit);
        la->addStretch();
        const int id = l.id;
        connect(btnVoir, &QPushButton::clicked, this, [this, id]() { emit consulterJuryDemande(id); });
        connect(btnEdit, &QPushButton::clicked, this, [this, id]() { emit modifierJuryDemande(id); });
        m_table->setCellWidget(r, kColActions, cActions);
        m_table->setItem(r, kColActions, new QTableWidgetItem());
    }

    definirToutCoche(false);
    mettreAJourPagination(total, debut, fin, nbPages);
}

void ParametresJuryPage::mettreAJourPagination(int total, int debut, int fin, int nbPages)
{
    if (total == 0) m_lblAffichage->setText("Aucun juré ne correspond à la recherche");
    else m_lblAffichage->setText(QString("Affichage de %1 à %2 sur %3 jurys").arg(debut + 1).arg(fin).arg(total));

    // Boutons numérotés (reconstruits à chaque chargement)
    while (QLayoutItem *item = m_pagesLayout->takeAt(0)) {
        if (QWidget *w = item->widget()) w->deleteLater();
        delete item;
    }
    for (int p = 1; p <= nbPages; ++p) {
        QPushButton *b = new QPushButton(QString::number(p));
        b->setFixedSize(34, 34);
        b->setCursor(Qt::PointingHandCursor);
        const bool actuelle = (p == m_page);
        b->setStyleSheet(actuelle
            ? "QPushButton { background-color: #0391FC; color: #FFFFFF; border: none; border-radius: 6px; font-size: 13px; font-weight: bold; }"
            : "QPushButton { background-color: transparent; color: #FFFFFF; border: 1px solid #1F57A4; border-radius: 6px; font-size: 13px; font-weight: bold; } "
              "QPushButton:hover { background-color: #173266; border-color: #3B82F6; }");
        connect(b, &QPushButton::clicked, this, [this, p]() { allerPage(p); });
        m_pagesLayout->addWidget(b);
    }
    m_btnPrecedent->setEnabled(m_page > 1);
    m_btnSuivant->setEnabled(m_page < nbPages);
}

void ParametresJuryPage::reconstruireMenuFiltre()
{
    m_menuFiltre->clear();

    // Équipes disponibles (si l'équipe filtrée a disparu, on retire le filtre)
    const QStringList equipes = Jury::equipesDistinctes();
    if (!m_filtreEquipe.isEmpty() && !equipes.contains(m_filtreEquipe)) m_filtreEquipe.clear();

    m_menuFiltre->addSection("Statut");
    QActionGroup *gStatut = new QActionGroup(m_menuFiltre);
    gStatut->setExclusive(true);
    const QStringList statuts = {"", "Actif", "Inactif"};
    for (const QString &s : statuts) {
        QAction *a = m_menuFiltre->addAction(s.isEmpty() ? "Tous les statuts" : s);
        a->setCheckable(true);
        a->setActionGroup(gStatut);
        a->setChecked(s == m_filtreStatut);
        connect(a, &QAction::triggered, this, [this, s]() {
            m_filtreStatut = s;
            m_page = 1;
            reconstruireMenuFiltre();
            chargerListe();
        });
    }

    m_menuFiltre->addSection("Équipe");
    QActionGroup *gEquipe = new QActionGroup(m_menuFiltre);
    gEquipe->setExclusive(true);
    QStringList choixEquipes = {""};
    choixEquipes.append(equipes);
    for (const QString &e : choixEquipes) {
        QAction *a = m_menuFiltre->addAction(e.isEmpty() ? "Toutes les équipes" : e);
        a->setCheckable(true);
        a->setActionGroup(gEquipe);
        a->setChecked(e == m_filtreEquipe);
        connect(a, &QAction::triggered, this, [this, e]() {
            m_filtreEquipe = e;
            m_page = 1;
            reconstruireMenuFiltre();
            chargerListe();
        });
    }

    m_menuFiltre->addSeparator();
    QAction *reset = m_menuFiltre->addAction("Réinitialiser les filtres");
    connect(reset, &QAction::triggered, this, [this]() {
        m_filtreStatut.clear();
        m_filtreEquipe.clear();
        m_page = 1;
        reconstruireMenuFiltre();
        chargerListe();
    });

    // Bouton Filtrer : mis en évidence quand un filtre est actif
    const bool actif = !m_filtreStatut.isEmpty() || !m_filtreEquipe.isEmpty();
    m_btnFiltrer->setStyleSheet(QString(
        "QPushButton { background-color: %1; color: #FFFFFF; border: 1px solid %2; border-radius: 8px; font-size: 12px; font-weight: 600; } "
        "QPushButton:hover { background-color: #173266; border-color: #2F6FD8; }")
        .arg(actif ? "#0B4FB0" : "#011846", actif ? "#19C3F0" : "#1E5BC0"));
}

// ====================================================================
// ACTIONS
// ====================================================================

void ParametresJuryPage::enregistrerParametres()
{
    if (!m_modificationAutorisee) {
        QMessageBox::information(this, "Accès restreint",
                                 "Seul un administrateur / organisateur peut modifier les paramètres du jury.");
        return;
    }
    if (m_spinMembres->value() > m_spinJurys->value()) {
        QMessageBox::warning(this, "Paramètres invalides",
                             QString("Le nombre de membres par session (%1) ne peut pas dépasser le nombre de jurys (%2).")
                                 .arg(m_spinMembres->value()).arg(m_spinJurys->value()));
        m_spinMembres->setFocus();
        return;
    }

    ParametresJury p;
    p.nombreJurys = m_spinJurys->value();
    p.membresParSession = m_spinMembres->value();
    p.dureeEvaluationMin = m_spinDuree->value();
    p.noteTotaleParProjet = m_spinNote->value();

    QString erreurSql;
    if (!p.enregistrer(&erreurSql)) {
        QMessageBox::critical(this, "Erreur",
                              "L'enregistrement des paramètres dans la base de données a échoué :\n" +
                              (erreurSql.isEmpty() ? QStringLiteral("Erreur inconnue.") : erreurSql));
        return;
    }
    QMessageBox::information(this, "Paramètres enregistrés", "Les paramètres du jury ont été enregistrés avec succès.");
    emit parametresEnregistres();
}

void ParametresJuryPage::surRecherche(const QString &)
{
    m_page = 1;
    chargerListe();
}

void ParametresJuryPage::surTriDemande(int colonne)
{
    if (colonne == kColCoche) {          // case « tout sélectionner » de l'en-tête
        definirToutCoche(!toutCoche());
        surCaseTous(toutCoche());
        return;
    }
    if (colonne != kColNom && colonne != kColStatut) return;
    if (m_triColonne == colonne) m_triCroissant = !m_triCroissant;
    else { m_triColonne = colonne; m_triCroissant = true; }
    m_page = 1;
    chargerListe();
}

void ParametresJuryPage::allerPage(int page)
{
    m_page = page;
    chargerListe();
}

void ParametresJuryPage::surCaseTous(bool coche)
{
    for (int r = 0; r < m_table->rowCount(); ++r) {
        if (QWidget *w = m_table->cellWidget(r, kColCoche)) {
            if (QCheckBox *c = w->findChild<QCheckBox *>()) {
                QSignalBlocker b(c);
                c->setChecked(coche);
            }
        }
    }
}

void ParametresJuryPage::surCaseLigne()
{
    bool toutes = m_table->rowCount() > 0;
    for (int r = 0; r < m_table->rowCount() && toutes; ++r) {
        QWidget *w = m_table->cellWidget(r, kColCoche);
        QCheckBox *c = w ? w->findChild<QCheckBox *>() : nullptr;
        if (!c || !c->isChecked()) toutes = false;
    }
    definirToutCoche(toutes);
}

bool ParametresJuryPage::toutCoche() const
{
    const EnteteTri *e = dynamic_cast<const EnteteTri *>(m_table->horizontalHeader());
    return e && e->toutCoche;
}

void ParametresJuryPage::definirToutCoche(bool coche)
{
    if (EnteteTri *e = dynamic_cast<EnteteTri *>(m_table->horizontalHeader())) {
        e->toutCoche = coche;
        e->viewport()->update();
    }
}

// ====================================================================
// MISE EN PAGE DYNAMIQUE
// ====================================================================

bool ParametresJuryPage::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::Resize || event->type() == QEvent::Show) {
        // Unité « min » du champ durée
        if (QSpinBox *s = qobject_cast<QSpinBox *>(obj)) {
            if (QLabel *u = s->findChild<QLabel *>("lblUnite")) {
                u->adjustSize();
                u->move(s->width() - 30 - u->width() - 12, (s->height() - u->height()) / 2);
                u->raise();
            }
        }
        // Largeurs de colonnes proportionnelles à la largeur du tableau
        if (m_table && obj == m_table->viewport()) {
            const int total = m_table->viewport()->width();
            const int coche = 40, statut = 84, actions = 80;
            const int reste = qMax(200, total - coche - statut - actions);
            QHeaderView *h = m_table->horizontalHeader();
            h->resizeSection(kColCoche, coche);
            h->resizeSection(kColNom, reste * 29 / 100);
            h->resizeSection(kColPrenom, reste * 19 / 100);
            h->resizeSection(kColSpecialite, reste * 30 / 100);
            h->resizeSection(kColEquipe, reste - reste * 29 / 100 - reste * 19 / 100 - reste * 30 / 100);
            h->resizeSection(kColStatut, statut);
            h->resizeSection(kColActions, actions);
        }
    }
    return QWidget::eventFilter(obj, event);
}
