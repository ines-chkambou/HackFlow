#include "evaluationprojets.h"
#include "evaluation.h"
#include "uihelpers.h"

#include <QAction>
#include <QActionGroup>
#include <QCalendarWidget>
#include <QCheckBox>
#include <QComboBox>
#include <QDateEdit>
#include <QDateTime>
#include <QDebug>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSqlError>
#include <QSqlQuery>
#include <QTableWidget>
#include <QTextCursor>
#include <QVBoxLayout>
#include <algorithm>

using UiHelpers::EnteteTri;

namespace {

const int kColCoche = 0, kColProjet = 1, kColEquipe = 2, kColJury = 3, kColDate = 4,
          kColNote = 5, kColStatut = 6, kColActions = 7;

const int kNoteMax = 20;
const int kCommentaireMax = 500;

// Valeurs affichées par défaut dans la grille (celles de la maquette)
const double kNotesParDefaut[EvalProjet::NbCriteres] = {16, 18, 14, 17, 16, 15, 17};

const QString kCarteCss =
    "QFrame#carteEval { background-color: #011846; border: 1px solid #14449A; border-radius: 8px; }";

const QString kComboCss =
    "QComboBox { background-color: #01286C; border: 1px solid #1F57A4; border-radius: 6px; padding: 0px 12px; "
    "color: #FFFFFF; font-size: 12px; min-height: 34px; } "
    "QComboBox:focus { border: 1px solid #3B82F6; } "
    "QComboBox::drop-down { border: none; width: 30px; } "
    "QComboBox::down-arrow { image: url(:/img/chevron_down_white.png); width: 12px; height: 12px; } "
    "QComboBox QAbstractItemView { background-color: #021B4A; color: #E6EEFF; border: 1px solid #1F57A4; "
    "selection-background-color: #0B6BFF; outline: none; }";

const QString kComboFiltreCss =
    "QComboBox { background-color: #011846; border: 1px solid #1F57A4; border-radius: 6px; padding: 0px 8px; "
    "color: #E6EEFF; font-size: 11px; min-height: 30px; } "
    "QComboBox:focus { border: 1px solid #3B82F6; } "
    "QComboBox::drop-down { border: none; width: 24px; } "
    "QComboBox::down-arrow { image: url(:/img/chevron_down_white.png); width: 11px; height: 11px; } "
    "QComboBox QAbstractItemView { background-color: #021B4A; color: #E6EEFF; border: 1px solid #1F57A4; "
    "selection-background-color: #0B6BFF; outline: none; }";

const QString kDateCss =
    "QDateEdit { background-color: #01286C; border: 1px solid #1F57A4; border-radius: 6px; padding: 0px 8px 0px 36px; "
    "color: #FFFFFF; font-size: 12px; min-height: 34px; } "
    "QDateEdit:focus { border: 1px solid #3B82F6; } "
    "QDateEdit::drop-down { border: none; width: 30px; } "
    "QDateEdit::down-arrow { image: url(:/img/chevron_down_white.png); width: 12px; height: 12px; }";

const QString kCalendrierPopupCss =
    "QCalendarWidget QWidget { background-color: #021B4A; color: #E6EEFF; } "
    "QCalendarWidget QToolButton { color: #E6EEFF; background: transparent; border: none; padding: 4px 8px; } "
    "QCalendarWidget QToolButton:hover { background-color: #173266; } "
    "QCalendarWidget QMenu { background-color: #021B4A; color: #E6EEFF; } "
    "QCalendarWidget QSpinBox { background-color: #01286C; color: #E6EEFF; } "
    "QCalendarWidget QAbstractItemView:enabled { background-color: #021B4A; color: #E6EEFF; "
    "selection-background-color: #0B6BFF; selection-color: #FFFFFF; } "
    "QCalendarWidget QAbstractItemView:disabled { color: #5B6E98; }";

const QString kDialogCss =
    "QDialog { background-color: #01143C; } "
    "QLabel { color: #E6EEFF; font-size: 13px; background: transparent; } "
    "QPushButton { background-color: #11254F; color: #EAF2FF; border: 1px solid #1D4A8B; border-radius: 8px; padding: 0px 18px; "
    "min-height: 40px; font-size: 13px; font-weight: 600; } "
    "QPushButton:hover { background-color: #173266; border-color: #2F6FD8; } "
    "QPushButton[principal=\"true\"] { background-color: #0391FC; color: #FFFFFF; border: none; font-weight: bold; } "
    "QPushButton[principal=\"true\"]:hover { background-color: #1AA0FF; }";

const char *kFormatDate = "dd/MM/yyyy";

// Champ de note : « 16 » (et non « 16.0 »), « 16.5 » par pas de 0,5
class NoteSpin : public QDoubleSpinBox
{
public:
    explicit NoteSpin(QWidget *parent = nullptr) : QDoubleSpinBox(parent)
    {
        setRange(0, kNoteMax);
        setDecimals(1);
        setSingleStep(0.5);
        setKeyboardTracking(true);
        setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    }

    QString textFromValue(double v) const override
    {
        return qFuzzyCompare(v + 1, qRound(v) + 1.0) ? QString::number(qRound(v)) : QString::number(v, 'f', 1);
    }
};

// « GreenTeam » -> « GT », « EcoSolutions » -> « ES » (majuscules), sinon deux premières lettres
QString initialesEquipe(const QString &equipe)
{
    QString maj;
    for (const QChar &c : equipe) if (c.isUpper()) maj += c;
    return maj.size() >= 2 ? maj.left(2) : UiHelpers::initiales(equipe);
}

QString formatNote(double v)
{
    return QString::number(v, 'f', 2);
}

QString couleurNote(double note)
{
    return note >= 13.0 ? "#0DA582" : "#7C6E5E";
}

QString etiquetteObligatoire(const QString &texte)
{
    return texte + " <span style='color:#19C3F0;'>*</span>";
}

} // namespace

// ====================================================================
// GRILLE ET DONNÉES
// ====================================================================

QStringList EvaluationProjetsPage::nomsCriteres()
{
    return {"Qualité de la solution", "Innovation", "Faisabilité", "Présentation",
            "Impact / Valeur ajoutée", "Business Model", "Équipe"};
}

QStringList EvaluationProjetsPage::descriptionsCriteres()
{
    return {"Pertinence et innovation\nde la solution proposée", "Niveau d'innovation\net originalité",
            "Réalisation technique\net faisabilité du projet", "Qualité de la présentation\net clarté du discours",
            "Impact potentiel et valeur\npour l'écosystème", "Modèle économique\net viabilité",
            "Compétences et cohésion\nde l'équipe"};
}

QVector<int> EvaluationProjetsPage::poidsCriteres()
{
    return {20, 15, 15, 10, 15, 10, 10};
}

double EvaluationProjetsPage::calculerNoteFinale(const double notes[EvalProjet::NbCriteres])
{
    const QVector<int> poids = poidsCriteres();
    double total = 0;
    for (int i = 0; i < EvalProjet::NbCriteres; ++i) total += notes[i] * poids.at(i) / 100.0;
    return qRound(total * 100) / 100.0;
}

static const char *kSelectEval =
    "SELECT e.id_evaluation, e.id_projet, p.titre, COALESCE(p.description, ''), p.nom_equipe, "
    "e.id_jury, j.nom, j.prenom, e.date_evaluation, e.note, "
    "e.crit_qualite, e.critere_innovation, e.critere_technique, e.critere_presentation, "
    "e.crit_impact, e.crit_business, e.crit_equipe, COALESCE(e.commentaire, '') "
    "FROM EVALUATION e "
    "JOIN PROJET p ON p.id_projet = e.id_projet "
    "JOIN JURY j ON j.id_jury = e.id_jury ";

static EvalProjet depuisRequete(const QSqlQuery &q)
{
    EvalProjet e;
    e.id = q.value(0).toInt();
    e.idProjet = q.value(1).toInt();
    e.titreProjet = q.value(2).toString();
    e.descriptionProjet = q.value(3).toString();
    e.equipe = q.value(4).toString();
    e.idJury = q.value(5).toInt();
    e.nomJury = q.value(6).toString();
    e.prenomJury = q.value(7).toString();
    e.dateHeure = q.value(8).toString();
    e.note = q.value(9).toDouble();
    // Évaluations saisies avec l'ancienne grille : les critères absents prennent la note globale
    for (int i = 0; i < EvalProjet::NbCriteres; ++i) {
        const QVariant v = q.value(10 + i);
        e.criteres[i] = v.isNull() ? e.note : v.toDouble();
    }
    e.commentaire = q.value(17).toString();
    return e;
}

QVector<EvalProjet> EvaluationProjetsPage::lireEvaluations()
{
    QVector<EvalProjet> res;
    QSqlQuery q;
    if (!q.exec(QString(kSelectEval) + "ORDER BY e.date_evaluation DESC, e.id_evaluation DESC")) {
        qWarning() << "Erreur lecture évaluations:" << q.lastError().text();
        return res;
    }
    while (q.next()) res.append(depuisRequete(q));
    return res;
}

EvalProjet EvaluationProjetsPage::lireEvaluation(int id)
{
    QSqlQuery q;
    q.prepare(QString(kSelectEval) + "WHERE e.id_evaluation = ?");
    q.addBindValue(id);
    if (q.exec() && q.next()) return depuisRequete(q);
    return EvalProjet();
}

bool EvaluationProjetsPage::enregistrer(EvalProjet &e)
{
    QSqlQuery q;
    if (e.id < 0) {
        q.prepare("INSERT INTO EVALUATION (id_jury, id_projet, note, critere_innovation, critere_technique, "
                  "critere_presentation, critere_pitch, crit_qualite, crit_impact, crit_business, crit_equipe, "
                  "commentaire, date_evaluation) VALUES (?, ?, ?, ?, ?, ?, 0, ?, ?, ?, ?, ?, ?)");
    } else {
        q.prepare("UPDATE EVALUATION SET id_jury = ?, id_projet = ?, note = ?, critere_innovation = ?, "
                  "critere_technique = ?, critere_presentation = ?, crit_qualite = ?, crit_impact = ?, "
                  "crit_business = ?, crit_equipe = ?, commentaire = ?, date_evaluation = ? WHERE id_evaluation = ?");
    }
    q.addBindValue(e.idJury);
    q.addBindValue(e.idProjet);
    q.addBindValue(e.note);
    q.addBindValue(e.criteres[1]);   // innovation
    q.addBindValue(e.criteres[2]);   // faisabilité (colonne « technique »)
    q.addBindValue(e.criteres[3]);   // présentation
    q.addBindValue(e.criteres[0]);   // qualité
    q.addBindValue(e.criteres[4]);   // impact
    q.addBindValue(e.criteres[5]);   // business model
    q.addBindValue(e.criteres[6]);   // équipe
    q.addBindValue(e.commentaire.trimmed());
    q.addBindValue(e.dateHeure);
    if (e.id >= 0) q.addBindValue(e.id);
    if (!q.exec()) {
        qWarning() << "Erreur enregistrement évaluation:" << q.lastError().text();
        return false;
    }
    QSqlQuery().exec("COMMIT");
    if (e.id < 0) e.id = q.lastInsertId().toInt();
    return true;
}

bool EvaluationProjetsPage::supprimer(int id)
{
    return Evaluation::supprimer(id);
}

// ====================================================================
// INTERFACE
// ====================================================================

EvaluationProjetsPage::EvaluationProjetsPage(QWidget *parent)
    : QWidget(parent),
      m_lblTitreForm(nullptr), m_cbProjet(nullptr), m_cbEquipe(nullptr), m_cbJury(nullptr), m_dateEval(nullptr),
      m_lblPoidsTotal(nullptr), m_lblNoteFinale(nullptr), m_txtCommentaire(nullptr), m_lblCompteur(nullptr),
      m_btnEnregistrer(nullptr), m_btnReinitialiser(nullptr),
      m_lblNombre(nullptr), m_inputRecherche(nullptr), m_cbFiltreJury(nullptr), m_cbFiltreEquipe(nullptr),
      m_cbFiltreDate(nullptr), m_btnFiltrer(nullptr), m_menuFiltre(nullptr), m_table(nullptr),
      m_lblAffichage(nullptr), m_btnPrecedent(nullptr), m_btnSuivant(nullptr), m_pagesLayout(nullptr),
      m_lblMoyenne(nullptr), m_lblNbEvals(nullptr), m_lblDerniere(nullptr),
      m_idEnEdition(-1), m_triColonne(kColDate), m_triCroissant(false), m_page(1), m_taillePage(8),
      m_modificationAutorisee(true), m_chargement(false)
{
    for (int i = 0; i < EvalProjet::NbCriteres; ++i) { m_spins[i] = nullptr; m_lblScores[i] = nullptr; }
    setObjectName("pageEvaluationProjets");
    construireInterface();
    notesParDefaut();
    rafraichir();
}

void EvaluationProjetsPage::construireInterface()
{
    QVBoxLayout *externe = new QVBoxLayout(this);
    externe->setContentsMargins(0, 0, 0, 0);

    QScrollArea *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("QScrollArea { background: transparent; border: none; } "
                          "QScrollArea > QWidget > QWidget { background: transparent; } "
                          "QScrollBar:vertical { background: transparent; width: 10px; } "
                          "QScrollBar::handle:vertical { background: #1F57A4; border-radius: 5px; min-height: 30px; } "
                          "QScrollBar:horizontal { background: transparent; height: 10px; } "
                          "QScrollBar::handle:horizontal { background: #1F57A4; border-radius: 5px; min-width: 30px; } "
                          "QScrollBar::add-line, QScrollBar::sub-line { height: 0px; width: 0px; }");
    externe->addWidget(scroll);

    QWidget *contenu = new QWidget();
    contenu->setObjectName("contenuEvaluationProjets");
    contenu->setMinimumWidth(1120);
    scroll->setWidget(contenu);

    QVBoxLayout *principal = new QVBoxLayout(contenu);
    principal->setContentsMargins(22, 22, 22, 22);
    principal->setSpacing(18);
    principal->addWidget(construireEnTete());

    QHBoxLayout *corps = new QHBoxLayout();
    corps->setSpacing(18);
    corps->addWidget(construireCarteFormulaire(), 0, Qt::AlignTop);

    QVBoxLayout *droite = new QVBoxLayout();
    droite->setSpacing(18);
    droite->addWidget(construireCarteListe(), 1);
    droite->addWidget(construireCartesStats(), 0);
    corps->addLayout(droite, 1);
    principal->addLayout(corps, 1);
}

QWidget *EvaluationProjetsPage::construireEnTete()
{
    QWidget *w = new QWidget();
    w->setStyleSheet("background: transparent;");
    QHBoxLayout *h = new QHBoxLayout(w);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(16);

    QLabel *icone = new QLabel();
    QPixmap pm(":/img/title_award.png");
    pm.setDevicePixelRatio(pm.width() / 50.0);
    icone->setPixmap(pm);
    icone->setFixedSize(50, 50);
    icone->setStyleSheet("background: transparent;");

    QVBoxLayout *textes = new QVBoxLayout();
    textes->setSpacing(2);
    QLabel *titre = new QLabel("Saisie et Grille d'Évaluation des Projets");
    titre->setStyleSheet("color: #FFFFFF; font-size: 21px; font-weight: bold; background: transparent;");
    QLabel *sousTitre = new QLabel("Évaluez les projets selon les critères définis. Les notes sont calculées automatiquement.");
    sousTitre->setStyleSheet("color: #19C3F0; font-size: 12px; background: transparent;");
    textes->addWidget(titre);
    textes->addWidget(sousTitre);

    h->addWidget(icone, 0, Qt::AlignVCenter);
    h->addLayout(textes);
    h->addStretch();

    QPushButton *btnRetour = new QPushButton(" Retour");
    btnRetour->setObjectName("btnRetourEvaluation");
    btnRetour->setIcon(QIcon(":/img/arrow_left.png"));
    btnRetour->setIconSize(QSize(18, 18));
    btnRetour->setFixedSize(130, 46);
    btnRetour->setCursor(Qt::PointingHandCursor);
    btnRetour->setStyleSheet("QPushButton { background-color: #011846; color: #FFFFFF; border: 1px solid #1E5BC0; "
                             "border-radius: 6px; font-size: 13px; font-weight: 600; } "
                             "QPushButton:hover { background-color: #06306F; border-color: #3B82F6; } "
                             "QPushButton:pressed { background-color: #031A48; }");
    connect(btnRetour, &QPushButton::clicked, this, &EvaluationProjetsPage::retourDemande);
    h->addWidget(btnRetour);

    // Encadré d'information (note finale calculée automatiquement)
    QFrame *info = new QFrame();
    info->setObjectName("encadreInfoEval");
    info->setStyleSheet("QFrame#encadreInfoEval { background-color: #011846; border: 2px solid #1E7BE0; border-radius: 6px; }");
    info->setFixedHeight(58);
    QHBoxLayout *hi = new QHBoxLayout(info);
    hi->setContentsMargins(14, 6, 18, 6);
    hi->setSpacing(12);
    QLabel *ic = new QLabel();
    QPixmap pc(":/img/calc_cyan.png");
    pc.setDevicePixelRatio(pc.width() / 34.0);
    ic->setPixmap(pc);
    ic->setFixedSize(34, 34);
    ic->setStyleSheet("background: transparent; border: none;");
    QLabel *txt = new QLabel("La note finale est calculée automatiquement\nà partir des critères suivants.");
    txt->setStyleSheet("color: #BDD6FF; font-size: 11px; background: transparent; border: none;");
    hi->addWidget(ic);
    hi->addWidget(txt);
    h->addWidget(info);
    return w;
}

QWidget *EvaluationProjetsPage::construireCarteFormulaire()
{
    QFrame *carte = new QFrame();
    carte->setObjectName("carteEval");
    carte->setStyleSheet(kCarteCss);
    carte->setFixedWidth(450);

    QVBoxLayout *v = new QVBoxLayout(carte);
    v->setContentsMargins(16, 14, 16, 16);
    v->setSpacing(10);

    // Titre
    QHBoxLayout *entete = new QHBoxLayout();
    entete->setSpacing(10);
    QLabel *ic = new QLabel();
    QPixmap pm(":/img/star_circle.png");
    pm.setDevicePixelRatio(pm.width() / 26.0);
    ic->setPixmap(pm);
    ic->setFixedSize(26, 26);
    ic->setStyleSheet("background: transparent; border: none;");
    m_lblTitreForm = new QLabel("Nouvelle évaluation");
    m_lblTitreForm->setObjectName("lblTitreFormulaireEval");
    m_lblTitreForm->setStyleSheet("color: #FFFFFF; font-size: 16px; font-weight: bold; background: transparent; border: none;");
    entete->addWidget(ic);
    entete->addWidget(m_lblTitreForm);
    entete->addStretch();
    v->addLayout(entete);

    // Projet / Équipe / Juré / Date
    const QString cssLabel = "color: #E6EEFF; font-size: 12px; background: transparent; border: none;";
    auto libelle = [&](const QString &t) {
        QLabel *l = new QLabel(t);
        l->setTextFormat(Qt::RichText);
        l->setStyleSheet(cssLabel);
        return l;
    };

    m_cbProjet = new QComboBox();
    m_cbEquipe = new QComboBox();
    m_cbJury = new QComboBox();
    m_cbProjet->setObjectName("cbEvalProjet");
    m_cbEquipe->setObjectName("cbEvalEquipe");
    m_cbJury->setObjectName("cbEvalJury");
    for (QComboBox *c : {m_cbProjet, m_cbEquipe, m_cbJury}) {
        c->setStyleSheet(kComboCss);
        c->setFixedHeight(36);
        c->setCursor(Qt::PointingHandCursor);
    }
    m_cbProjet->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_cbProjet->setMinimumContentsLength(8);
    m_cbProjet->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    m_cbJury->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    m_cbEquipe->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);

    m_dateEval = new QDateEdit(QDate::currentDate());
    m_dateEval->setObjectName("dateEval");
    m_dateEval->setDisplayFormat(kFormatDate);
    m_dateEval->setCalendarPopup(true);
    m_dateEval->setStyleSheet(kDateCss);
    m_dateEval->setFixedHeight(36);
    m_dateEval->setCursor(Qt::PointingHandCursor);
    m_dateEval->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    if (QCalendarWidget *cw = m_dateEval->calendarWidget()) {
        cw->setStyleSheet(kCalendrierPopupCss);
        cw->setVerticalHeaderFormat(QCalendarWidget::NoVerticalHeader);
        cw->setFirstDayOfWeek(Qt::Monday);
    }
    QLabel *icCal = new QLabel(m_dateEval);
    QPixmap pc(":/img/calendar_small.png");
    pc.setDevicePixelRatio(pc.width() / 16.0);
    icCal->setPixmap(pc);
    icCal->setGeometry(12, 10, 16, 16);
    icCal->setStyleSheet("background: transparent; border: none;");
    icCal->setAttribute(Qt::WA_TransparentForMouseEvents);

    QGridLayout *g = new QGridLayout();
    g->setHorizontalSpacing(14);
    g->setVerticalSpacing(4);
    g->addWidget(libelle(etiquetteObligatoire("Projet")), 0, 0);
    g->addWidget(libelle(etiquetteObligatoire("Équipe")), 0, 1);
    g->addWidget(m_cbProjet, 1, 0);
    g->addWidget(m_cbEquipe, 1, 1);
    g->addWidget(libelle(etiquetteObligatoire("Juré")), 2, 0);
    g->addWidget(libelle("Date d'évaluation"), 2, 1);
    g->addWidget(m_cbJury, 3, 0);
    g->addWidget(m_dateEval, 3, 1);
    g->setColumnStretch(0, 11);
    g->setColumnStretch(1, 9);
    g->setRowMinimumHeight(1, 36);
    v->addLayout(g);

    connect(m_cbProjet, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &EvaluationProjetsPage::surProjetChange);
    connect(m_cbEquipe, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &EvaluationProjetsPage::surEquipeChange);

    // ---- Grille d'évaluation ----
    v->addSpacing(2);
    QHBoxLayout *titreGrille = new QHBoxLayout();
    titreGrille->setSpacing(8);
    QLabel *icEtoile = new QLabel();
    QPixmap pe(":/img/star_fill.png");
    pe.setDevicePixelRatio(pe.width() / 22.0);
    icEtoile->setPixmap(pe);
    icEtoile->setFixedSize(22, 22);
    icEtoile->setStyleSheet("background: transparent; border: none;");
    QLabel *tg = new QLabel("Grille d'évaluation");
    tg->setStyleSheet("color: #FFFFFF; font-size: 14px; font-weight: bold; background: transparent; border: none;");
    m_lblPoidsTotal = new QLabel();
    m_lblPoidsTotal->setObjectName("lblPoidsTotal");
    m_lblPoidsTotal->setStyleSheet("color: #19C3F0; font-size: 11px; background: transparent; border: none;");
    titreGrille->addWidget(icEtoile);
    titreGrille->addWidget(tg);
    titreGrille->addStretch();
    titreGrille->addWidget(m_lblPoidsTotal);
    v->addLayout(titreGrille);

    // Colonnes : critère | description | poids | note | score
    const int lCrit = 130, lDesc = 116, lPoids = 32, lNote = 66, lScore = 36;
    auto celluleTexte = [](const QString &t, int largeur, Qt::Alignment al, const QString &css) {
        QLabel *l = new QLabel(t);
        l->setFixedWidth(largeur);
        l->setAlignment(al | Qt::AlignVCenter);
        l->setStyleSheet(css + " background: transparent; border: none;");
        return l;
    };

    QWidget *enteteGrille = new QWidget();
    enteteGrille->setObjectName("enteteGrille");
    enteteGrille->setFixedHeight(34);
    enteteGrille->setStyleSheet("QWidget#enteteGrille { background-color: #05215A; border-radius: 6px; }");
    QHBoxLayout *he = new QHBoxLayout(enteteGrille);
    he->setContentsMargins(8, 0, 4, 0);
    he->setSpacing(6);
    const QString cssEntete = "color: #E6EEFF; font-size: 11px; font-weight: bold;";
    he->addWidget(celluleTexte("Critère", lCrit, Qt::AlignLeft, cssEntete));
    he->addWidget(celluleTexte("Description", lDesc, Qt::AlignLeft, cssEntete));
    he->addWidget(celluleTexte("Poids", lPoids, Qt::AlignCenter, cssEntete));
    he->addWidget(celluleTexte("Note (/20)", lNote, Qt::AlignCenter, cssEntete));
    he->addWidget(celluleTexte("Score", lScore, Qt::AlignCenter, cssEntete));
    v->addWidget(enteteGrille);

    static const QStringList icones = {"crit_qualite.png", "crit_innovation.png", "crit_faisabilite.png",
                                       "crit_presentation.png", "crit_impact.png", "crit_business.png", "crit_equipe.png"};
    const QStringList noms = nomsCriteres();
    const QStringList descs = descriptionsCriteres();
    const QVector<int> poids = poidsCriteres();

    QVBoxLayout *lignes = new QVBoxLayout();
    lignes->setSpacing(0);
    for (int i = 0; i < EvalProjet::NbCriteres; ++i) {
        QWidget *ligne = new QWidget();
        ligne->setObjectName("ligneGrille");
        ligne->setFixedHeight(45);
        ligne->setStyleSheet(i % 2 == 0 ? "QWidget#ligneGrille { background-color: rgba(255,255,255,0.03); border-radius: 4px; }"
                                        : "QWidget#ligneGrille { background: transparent; }");
        QHBoxLayout *hl = new QHBoxLayout(ligne);
        hl->setContentsMargins(8, 0, 4, 0);
        hl->setSpacing(6);

        // Critère : icône + nom
        QWidget *cCrit = new QWidget();
        cCrit->setFixedWidth(lCrit);
        cCrit->setStyleSheet("background: transparent;");
        QHBoxLayout *hc = new QHBoxLayout(cCrit);
        hc->setContentsMargins(0, 0, 0, 0);
        hc->setSpacing(8);
        QLabel *icc = new QLabel();
        QPixmap pcr(":/img/" + icones.at(i));
        pcr.setDevicePixelRatio(pcr.width() / 22.0);
        icc->setPixmap(pcr);
        icc->setFixedSize(22, 22);
        QLabel *nom = new QLabel(noms.at(i));
        nom->setStyleSheet("color: #E6EEFF; font-size: 10px; font-weight: 600;");
        nom->setWordWrap(true);
        hc->addWidget(icc);
        hc->addWidget(nom, 1);
        hl->addWidget(cCrit);

        hl->addWidget(celluleTexte(descs.at(i), lDesc, Qt::AlignLeft, "color: #9FB6E0; font-size: 9px;"));
        hl->addWidget(celluleTexte(QString("%1%").arg(poids.at(i)), lPoids, Qt::AlignCenter, "color: #E6EEFF; font-size: 11px;"));

        NoteSpin *sp = new NoteSpin();
        sp->setObjectName(QString("spinNote%1").arg(i));
        sp->setFixedSize(lNote, 30);
        sp->setCursor(Qt::PointingHandCursor);
        sp->setStyleSheet(
            "QDoubleSpinBox { background-color: #01286C; border: 1px solid #1F57A4; border-radius: 5px; "
            "padding: 0px 18px 0px 8px; color: #FFFFFF; font-size: 12px; selection-background-color: #0B6BFF; } "
            "QDoubleSpinBox:focus { border: 1px solid #3B82F6; } "
            "QDoubleSpinBox:disabled { color: #7F93B8; } "
            "QDoubleSpinBox::up-button { subcontrol-origin: border; subcontrol-position: top right; width: 18px; "
            "border: none; background: transparent; } "
            "QDoubleSpinBox::down-button { subcontrol-origin: border; subcontrol-position: bottom right; width: 18px; "
            "border: none; background: transparent; } "
            "QDoubleSpinBox::up-arrow { image: url(:/img/chevron_up.png); width: 9px; height: 9px; } "
            "QDoubleSpinBox::down-arrow { image: url(:/img/chevron_down.png); width: 9px; height: 9px; }");
        m_spins[i] = sp;
        connect(sp, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double) { surNoteChange(); });
        hl->addWidget(sp);

        m_lblScores[i] = celluleTexte("0.00", lScore, Qt::AlignCenter, "color: #E6EEFF; font-size: 11px; font-weight: 600;");
        hl->addWidget(m_lblScores[i]);
        lignes->addWidget(ligne);
    }
    v->addLayout(lignes);

    // Note finale
    QFrame *barre = new QFrame();
    barre->setObjectName("barreNoteFinale");
    barre->setFixedHeight(50);
    barre->setStyleSheet("QFrame#barreNoteFinale { background-color: #012B6B; border: 1px solid #0F5FB8; border-radius: 8px; }");
    QHBoxLayout *hb = new QHBoxLayout(barre);
    hb->setContentsMargins(10, 0, 12, 0);
    hb->setSpacing(10);
    QLabel *icTrophee = new QLabel();
    icTrophee->setFixedSize(26, 30);
    icTrophee->setScaledContents(true);
    icTrophee->setPixmap(QPixmap(":/img/trophy.png"));
    icTrophee->setStyleSheet("background: transparent; border: none;");
    QLabel *lblNF = new QLabel("<span style='color:#19E6F0; font-weight:bold;'>Note finale</span> "
                               "<span style='color:#9FB6E0; font-size:11px;'>(calculée automatiquement)</span>");
    lblNF->setTextFormat(Qt::RichText);
    lblNF->setStyleSheet("font-size: 12px; background: transparent; border: none;");
    m_lblNoteFinale = new QLabel();
    m_lblNoteFinale->setObjectName("lblNoteFinale");
    m_lblNoteFinale->setTextFormat(Qt::RichText);
    m_lblNoteFinale->setAlignment(Qt::AlignCenter);
    m_lblNoteFinale->setFixedSize(98, 34);
    m_lblNoteFinale->setStyleSheet("background-color: #014A8F; border-radius: 17px; border: none;");
    hb->addWidget(icTrophee);
    hb->addWidget(lblNF);
    hb->addStretch();
    hb->addWidget(m_lblNoteFinale);
    v->addWidget(barre);

    // Commentaire
    QLabel *lblCom = new QLabel("Commentaire (optionnel)");
    lblCom->setStyleSheet(cssLabel);
    v->addWidget(lblCom);

    m_txtCommentaire = new QPlainTextEdit();
    m_txtCommentaire->setObjectName("txtCommentaireEval");
    m_txtCommentaire->setPlaceholderText("Votre commentaire sur ce projet...");
    m_txtCommentaire->setFixedHeight(76);
    m_txtCommentaire->setStyleSheet("QPlainTextEdit { background-color: #01286C; border: 1px solid #1F57A4; border-radius: 6px; "
                                    "padding: 6px 8px 16px 8px; color: #EAF2FF; font-size: 11px; } "
                                    "QPlainTextEdit:focus { border: 1px solid #3B82F6; }");
    QPalette pal = m_txtCommentaire->palette();
    pal.setColor(QPalette::PlaceholderText, QColor("#7F9BD0"));
    m_txtCommentaire->setPalette(pal);
    m_lblCompteur = new QLabel("0/500", m_txtCommentaire);
    m_lblCompteur->setObjectName("lblCompteurCommentaire");
    m_lblCompteur->setStyleSheet("color: #7F9BD0; font-size: 9px; background: transparent; border: none;");
    m_lblCompteur->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_txtCommentaire->installEventFilter(this);
    connect(m_txtCommentaire, &QPlainTextEdit::textChanged, this, &EvaluationProjetsPage::surCommentaireChange);
    v->addWidget(m_txtCommentaire);

    // Boutons
    v->addSpacing(4);
    QHBoxLayout *boutons = new QHBoxLayout();
    boutons->setSpacing(12);
    m_btnReinitialiser = new QPushButton("  Réinitialiser");
    m_btnReinitialiser->setObjectName("btnReinitialiserEvaluation");
    m_btnReinitialiser->setIcon(QIcon(":/img/refresh.png"));
    m_btnReinitialiser->setIconSize(QSize(18, 18));
    m_btnReinitialiser->setFixedHeight(42);
    m_btnReinitialiser->setCursor(Qt::PointingHandCursor);
    m_btnReinitialiser->setStyleSheet("QPushButton { background-color: #011846; color: #FFFFFF; border: 1px solid #1E5BC0; "
                                      "border-radius: 6px; font-size: 12px; font-weight: 600; } "
                                      "QPushButton:hover { background-color: #06306F; border-color: #3B82F6; }");
    connect(m_btnReinitialiser, &QPushButton::clicked, this, &EvaluationProjetsPage::reinitialiserFormulaire);

    m_btnEnregistrer = new QPushButton("  Enregistrer l'évaluation");
    m_btnEnregistrer->setObjectName("btnEnregistrerEvaluation");
    m_btnEnregistrer->setIcon(QIcon(":/img/save.png"));
    m_btnEnregistrer->setIconSize(QSize(18, 18));
    m_btnEnregistrer->setFixedHeight(42);
    m_btnEnregistrer->setCursor(Qt::PointingHandCursor);
    m_btnEnregistrer->setStyleSheet("QPushButton { background-color: #0391FC; color: #FFFFFF; border: none; border-radius: 6px; "
                                    "font-size: 12px; font-weight: 600; } "
                                    "QPushButton:hover { background-color: #1AA0FF; } "
                                    "QPushButton:pressed { background-color: #0279D6; }");
    connect(m_btnEnregistrer, &QPushButton::clicked, this, &EvaluationProjetsPage::enregistrerEvaluation);
    boutons->addWidget(m_btnReinitialiser, 4);
    boutons->addWidget(m_btnEnregistrer, 6);
    v->addLayout(boutons);

    return carte;
}

QWidget *EvaluationProjetsPage::construireCarteListe()
{
    QFrame *carte = new QFrame();
    carte->setObjectName("carteEval");
    carte->setStyleSheet(kCarteCss);

    QVBoxLayout *v = new QVBoxLayout(carte);
    v->setContentsMargins(14, 14, 14, 14);
    v->setSpacing(14);

    // Titre + compteur
    QHBoxLayout *entete = new QHBoxLayout();
    entete->setSpacing(12);
    QLabel *ic = new QLabel();
    QPixmap pm(":/img/doc_blue.png");
    pm.setDevicePixelRatio(pm.width() / 26.0);
    ic->setPixmap(pm);
    ic->setFixedSize(26, 26);
    ic->setStyleSheet("background: transparent; border: none;");
    QLabel *titre = new QLabel("Liste des projets évalués");
    titre->setStyleSheet("color: #FFFFFF; font-size: 16px; font-weight: bold; background: transparent; border: none;");
    entete->addWidget(ic);
    entete->addWidget(titre);
    entete->addStretch();
    m_lblNombre = new QLabel();
    m_lblNombre->setObjectName("lblNombreEvaluations");
    m_lblNombre->setStyleSheet("QLabel#lblNombreEvaluations { color: #FFFFFF; font-size: 12px; background-color: #0B5FD0; "
                               "border-radius: 8px; padding: 7px 14px; }");
    entete->addWidget(m_lblNombre);
    v->addLayout(entete);

    // Recherche + filtres
    QHBoxLayout *filtres = new QHBoxLayout();
    filtres->setSpacing(10);
    m_inputRecherche = new QLineEdit();
    m_inputRecherche->setObjectName("inputRechercheEvaluations");
    m_inputRecherche->setPlaceholderText("Rechercher un projet...");
    m_inputRecherche->setClearButtonEnabled(true);
    m_inputRecherche->setFixedHeight(40);
    m_inputRecherche->setStyleSheet("QLineEdit { background-color: #01286C; border: 1px solid #1F57A4; border-radius: 8px; "
                                    "padding-left: 40px; padding-right: 8px; color: #EAF2FF; font-size: 12px; } "
                                    "QLineEdit:focus { border: 1px solid #3B82F6; }");
    QPalette pal = m_inputRecherche->palette();
    pal.setColor(QPalette::PlaceholderText, QColor("#BDD6FF"));
    m_inputRecherche->setPalette(pal);
    QLabel *loupe = new QLabel(m_inputRecherche);
    QPixmap pl(":/img/search.png");
    pl.setDevicePixelRatio(pl.width() / 18.0);
    loupe->setPixmap(pl);
    loupe->setGeometry(14, 11, 18, 18);
    loupe->setStyleSheet("background: transparent; border: none;");
    loupe->setAttribute(Qt::WA_TransparentForMouseEvents);
    connect(m_inputRecherche, &QLineEdit::textChanged, this, [this](const QString &) { surFiltreChange(); });
    filtres->addWidget(m_inputRecherche, 1);

    m_cbFiltreJury = new QComboBox();
    m_cbFiltreEquipe = new QComboBox();
    m_cbFiltreDate = new QComboBox();
    m_cbFiltreJury->setObjectName("cbFiltreJuryEval");
    m_cbFiltreEquipe->setObjectName("cbFiltreEquipeEval");
    m_cbFiltreDate->setObjectName("cbFiltreDateEval");
    m_cbFiltreDate->addItems({"Toutes les dates", "Aujourd'hui", "7 derniers jours", "30 derniers jours", "Ce mois-ci"});
    const int largeurs[3] = {122, 138, 128};
    int k = 0;
    for (QComboBox *c : {m_cbFiltreJury, m_cbFiltreEquipe, m_cbFiltreDate}) {
        c->setStyleSheet(kComboFiltreCss);
        c->setFixedSize(largeurs[k++], 40);
        c->setCursor(Qt::PointingHandCursor);
        connect(c, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) { surFiltreChange(); });
        filtres->addWidget(c);
    }

    m_btnFiltrer = new QPushButton(" Filtrer");
    m_btnFiltrer->setObjectName("btnFiltrerEval");
    m_btnFiltrer->setIcon(QIcon(":/img/filter.png"));
    m_btnFiltrer->setIconSize(QSize(16, 16));
    m_btnFiltrer->setFixedSize(86, 40);
    m_btnFiltrer->setCursor(Qt::PointingHandCursor);
    m_menuFiltre = new QMenu(this);
    m_menuFiltre->setStyleSheet(UiHelpers::cssMenu());
    connect(m_btnFiltrer, &QPushButton::clicked, this, [this]() {
        m_menuFiltre->exec(m_btnFiltrer->mapToGlobal(QPoint(0, m_btnFiltrer->height() + 4)));
    });
    filtres->addWidget(m_btnFiltrer);
    v->addLayout(filtres);

    // Tableau
    m_table = new QTableWidget();
    m_table->setObjectName("tableEvaluations");
    EnteteTri *entetes = new EnteteTri(m_table);
    entetes->triables = {kColDate, kColNote};
    entetes->margesGauche[kColProjet] = 0;
    m_table->setHorizontalHeader(entetes);
    m_table->setColumnCount(8);
    m_table->setHorizontalHeaderLabels({"", "Projet", "Équipe", "Juré", "Date", "Note finale", "Statut", "Actions"});
    m_table->verticalHeader()->setVisible(false);
    m_table->verticalHeader()->setDefaultSectionSize(53);
    m_table->setShowGrid(false);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setFocusPolicy(Qt::NoFocus);
    m_table->setWordWrap(false);
    m_table->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_table->setFrameShape(QFrame::NoFrame);
    m_table->setStyleSheet("QTableWidget { background: transparent; border: none; color: #E6EEFF; font-size: 12px; } "
                           "QTableWidget::item { border-bottom: 1px solid #0F3070; padding-left: 4px; }");
    entetes->setSectionResizeMode(QHeaderView::Fixed);
    entetes->setMinimumSectionSize(20);
    entetes->setStretchLastSection(false);
    entetes->setFixedHeight(44);
    entetes->setSectionsClickable(true);
    entetes->setHighlightSections(false);
    entetes->setStyleSheet("QHeaderView { background: transparent; border: none; }");
    entetes->viewport()->setAutoFillBackground(false);
    connect(entetes, &QHeaderView::sectionClicked, this, &EvaluationProjetsPage::surTriDemande);
    m_table->viewport()->installEventFilter(this);
    v->addWidget(m_table, 1);

    // Pied : compteur + pagination
    QHBoxLayout *pied = new QHBoxLayout();
    pied->setSpacing(8);
    m_lblAffichage = new QLabel();
    m_lblAffichage->setStyleSheet("color: #B8C7E8; font-size: 11px; background: transparent; border: none;");
    pied->addWidget(m_lblAffichage);
    pied->addStretch();

    const QString cssNav = "QPushButton { background-color: transparent; border: 1px solid #1F57A4; border-radius: 6px; } "
                           "QPushButton:hover { background-color: #173266; border-color: #3B82F6; } "
                           "QPushButton:disabled { border-color: #12326B; }";
    m_btnPrecedent = new QPushButton();
    m_btnPrecedent->setObjectName("btnPagePrecedenteEval");
    m_btnPrecedent->setIcon(QIcon(":/img/page_prev.png"));
    m_btnPrecedent->setIconSize(QSize(14, 14));
    m_btnPrecedent->setFixedSize(34, 34);
    m_btnPrecedent->setCursor(Qt::PointingHandCursor);
    m_btnPrecedent->setStyleSheet(cssNav);
    m_btnPrecedent->setToolTip("Page précédente");
    connect(m_btnPrecedent, &QPushButton::clicked, this, [this]() { allerPage(m_page - 1); });
    m_btnSuivant = new QPushButton();
    m_btnSuivant->setObjectName("btnPageSuivanteEval");
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

QWidget *EvaluationProjetsPage::construireCartesStats()
{
    QWidget *w = new QWidget();
    w->setStyleSheet("background: transparent;");
    QHBoxLayout *h = new QHBoxLayout(w);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(18);

    auto carte = [&](const QString &icone, int tailleIcone, const QString &titre, QLabel *&valeur) {
        QFrame *f = new QFrame();
        f->setObjectName("carteEval");
        f->setStyleSheet(kCarteCss);
        f->setFixedHeight(88);
        QHBoxLayout *hl = new QHBoxLayout(f);
        hl->setContentsMargins(18, 10, 14, 10);
        hl->setSpacing(18);
        QLabel *ic = new QLabel();
        QPixmap pm(":/img/" + icone);
        pm.setDevicePixelRatio(pm.width() / double(tailleIcone));
        ic->setPixmap(pm);
        ic->setFixedSize(tailleIcone, tailleIcone);
        ic->setStyleSheet("background: transparent; border: none;");
        QVBoxLayout *vl = new QVBoxLayout();
        vl->setSpacing(6);
        QLabel *t = new QLabel(titre);
        t->setStyleSheet("color: #BDD6FF; font-size: 11px; background: transparent; border: none;");
        valeur = new QLabel();
        valeur->setTextFormat(Qt::RichText);
        valeur->setStyleSheet("color: #FFFFFF; font-size: 13px; background: transparent; border: none;");
        vl->addStretch();
        vl->addWidget(t);
        vl->addWidget(valeur);
        vl->addStretch();
        hl->addWidget(ic, 0, Qt::AlignVCenter);
        hl->addLayout(vl, 1);
        h->addWidget(f, 1);
    };
    carte("trophy.png", 44, "Note moyenne", m_lblMoyenne);
    carte("doc_blue.png", 40, "Nombre d'évaluations", m_lblNbEvals);
    carte("calendar_cyan.png", 44, "Dernière mise à jour", m_lblDerniere);
    m_lblMoyenne->setObjectName("lblStatMoyenne");
    m_lblNbEvals->setObjectName("lblStatNombre");
    m_lblDerniere->setObjectName("lblStatDerniere");
    return w;
}

// ====================================================================
// CHARGEMENT
// ====================================================================

void EvaluationProjetsPage::rafraichir()
{
    rechargerListesFormulaire();
    rechargerFiltres();
    reconstruireMenuFiltre();
    surNoteChange();
    chargerListe();
    chargerStatistiques();
    // Une évaluation en cours de modification qui n'existe plus : retour au mode « nouvelle évaluation »
    if (m_idEnEdition >= 0 && lireEvaluation(m_idEnEdition).id < 0) definirModeEdition(-1);
}

void EvaluationProjetsPage::definirModificationAutorisee(bool autorisee)
{
    m_modificationAutorisee = autorisee;
    if (!autorisee && m_idEnEdition >= 0) reinitialiserFormulaire();
    if (m_table) chargerListe();
}

void EvaluationProjetsPage::rechargerListesFormulaire()
{
    const int projetCourant = m_cbProjet->currentData().toInt();
    const int juryCourant = m_cbJury->currentData().toInt();
    const QString equipeCourante = m_cbEquipe->currentText();

    m_chargement = true;
    {
        QSignalBlocker b1(m_cbProjet), b2(m_cbEquipe), b3(m_cbJury);
        m_cbProjet->clear();
        m_cbEquipe->clear();
        m_cbJury->clear();

        QSqlQuery q("SELECT id_projet, titre, nom_equipe, COALESCE(description, '') FROM PROJET ORDER BY id_projet");
        QStringList equipes;
        while (q.next()) {
            const QString desc = q.value(3).toString();
            m_cbProjet->addItem(desc.isEmpty() ? q.value(1).toString() : q.value(1).toString() + " - " + desc, q.value(0).toInt());
            m_cbProjet->setItemData(m_cbProjet->count() - 1, q.value(2).toString(), Qt::UserRole + 1);
            if (!equipes.contains(q.value(2).toString())) equipes << q.value(2).toString();
        }
        std::sort(equipes.begin(), equipes.end());
        for (const QString &e : equipes) m_cbEquipe->addItem(e, e);

        QSqlQuery qj("SELECT id_jury, nom, prenom FROM JURY ORDER BY nom, prenom");
        while (qj.next()) m_cbJury->addItem(qj.value(1).toString() + " " + qj.value(2).toString(), qj.value(0).toInt());

        // Retour aux sélections précédentes si elles existent encore
        int i = m_cbProjet->findData(projetCourant);
        m_cbProjet->setCurrentIndex(i >= 0 ? i : 0);
        i = m_cbJury->findData(juryCourant);
        m_cbJury->setCurrentIndex(i >= 0 ? i : 0);
        const QString eq = m_cbProjet->currentData(Qt::UserRole + 1).toString();
        i = m_cbEquipe->findText(eq.isEmpty() ? equipeCourante : eq);
        m_cbEquipe->setCurrentIndex(i >= 0 ? i : 0);
    }
    m_chargement = false;
}

void EvaluationProjetsPage::rechargerFiltres()
{
    const QString jury = m_cbFiltreJury->currentText();
    const QString equipe = m_cbFiltreEquipe->currentText();
    QSignalBlocker b1(m_cbFiltreJury), b2(m_cbFiltreEquipe);
    m_cbFiltreJury->clear();
    m_cbFiltreEquipe->clear();
    m_cbFiltreJury->addItem("Tous les jurés", -1);
    m_cbFiltreEquipe->addItem("Toutes les équipes", QString());

    QSqlQuery qj("SELECT id_jury, nom, prenom FROM JURY ORDER BY nom, prenom");
    while (qj.next()) m_cbFiltreJury->addItem(qj.value(1).toString() + " " + qj.value(2).toString(), qj.value(0).toInt());
    QSqlQuery qe("SELECT DISTINCT nom_equipe FROM PROJET ORDER BY nom_equipe");
    while (qe.next()) m_cbFiltreEquipe->addItem(qe.value(0).toString(), qe.value(0).toString());

    int i = m_cbFiltreJury->findText(jury);
    m_cbFiltreJury->setCurrentIndex(i >= 0 ? i : 0);
    i = m_cbFiltreEquipe->findText(equipe);
    m_cbFiltreEquipe->setCurrentIndex(i >= 0 ? i : 0);
}

void EvaluationProjetsPage::reconstruireMenuFiltre()
{
    m_menuFiltre->clear();
    m_menuFiltre->addSection("Note finale");
    QActionGroup *groupe = new QActionGroup(m_menuFiltre);
    groupe->setExclusive(true);
    const QList<QPair<QString, QString>> choix = {{"", "Toutes les notes"}, {"haute", "16 / 20 et plus"},
                                                  {"moyenne", "De 13 à 16 / 20"}, {"basse", "Moins de 13 / 20"}};
    for (const auto &c : choix) {
        QAction *a = m_menuFiltre->addAction(c.second);
        a->setCheckable(true);
        a->setActionGroup(groupe);
        a->setChecked(c.first == m_filtreNote);
        const QString cle = c.first;
        connect(a, &QAction::triggered, this, [this, cle]() {
            m_filtreNote = cle;
            m_page = 1;
            reconstruireMenuFiltre();
            chargerListe();
        });
    }
    m_menuFiltre->addSeparator();
    QAction *reset = m_menuFiltre->addAction("Réinitialiser les filtres");
    connect(reset, &QAction::triggered, this, [this]() {
        m_filtreNote.clear();
        {
            QSignalBlocker b1(m_cbFiltreJury), b2(m_cbFiltreEquipe), b3(m_cbFiltreDate), b4(m_inputRecherche);
            m_cbFiltreJury->setCurrentIndex(0);
            m_cbFiltreEquipe->setCurrentIndex(0);
            m_cbFiltreDate->setCurrentIndex(0);
            m_inputRecherche->clear();
        }
        m_page = 1;
        reconstruireMenuFiltre();
        chargerListe();
    });

    const bool actif = !m_filtreNote.isEmpty();
    m_btnFiltrer->setStyleSheet(QString(
        "QPushButton { background-color: %1; color: #FFFFFF; border: 1px solid %2; border-radius: 8px; font-size: 12px; font-weight: 600; } "
        "QPushButton:hover { background-color: #173266; border-color: #2F6FD8; }")
        .arg(actif ? "#0B4FB0" : "#011846", actif ? "#19C3F0" : "#1E5BC0"));
}

QVector<EvalProjet> EvaluationProjetsPage::evaluationsFiltrees() const
{
    QVector<EvalProjet> res;
    const QString recherche = m_inputRecherche->text().trimmed();
    const int idJury = m_cbFiltreJury->currentData().toInt();
    const QString equipe = m_cbFiltreEquipe->currentData().toString();
    const int periode = m_cbFiltreDate->currentIndex();
    const QDate aujourdhui = QDate::currentDate();

    for (const EvalProjet &e : lireEvaluations()) {
        if (m_cbFiltreJury->currentIndex() > 0 && e.idJury != idJury) continue;
        if (m_cbFiltreEquipe->currentIndex() > 0 && e.equipe != equipe) continue;
        const QDate d = e.date();
        if (periode == 1 && d != aujourdhui) continue;
        if (periode == 2 && !(d <= aujourdhui && d >= aujourdhui.addDays(-6))) continue;
        if (periode == 3 && !(d <= aujourdhui && d >= aujourdhui.addDays(-29))) continue;
        if (periode == 4 && !(d.year() == aujourdhui.year() && d.month() == aujourdhui.month())) continue;
        if (m_filtreNote == "haute" && e.note < 16.0) continue;
        if (m_filtreNote == "moyenne" && !(e.note >= 13.0 && e.note < 16.0)) continue;
        if (m_filtreNote == "basse" && e.note >= 13.0) continue;
        if (!recherche.isEmpty()) {
            const QString bloc = e.titreProjet + ' ' + e.descriptionProjet + ' ' + e.equipe + ' ' + e.nomJury + ' ' +
                                 e.prenomJury + ' ' + e.prenomJury + ' ' + e.nomJury;
            if (!bloc.contains(recherche, Qt::CaseInsensitive)) continue;
        }
        res.append(e);
    }

    if (m_triColonne == kColDate) {
        std::stable_sort(res.begin(), res.end(), [this](const EvalProjet &a, const EvalProjet &b) {
            return m_triCroissant ? a.dateHeure < b.dateHeure : a.dateHeure > b.dateHeure;
        });
    } else if (m_triColonne == kColNote) {
        std::stable_sort(res.begin(), res.end(), [this](const EvalProjet &a, const EvalProjet &b) {
            return m_triCroissant ? a.note < b.note : a.note > b.note;
        });
    }
    return res;
}

void EvaluationProjetsPage::chargerListe()
{
    if (!m_table) return;
    const QVector<EvalProjet> liste = evaluationsFiltrees();

    if (EnteteTri *e = dynamic_cast<EnteteTri *>(m_table->horizontalHeader())) {
        e->colonneActive = m_triColonne;
        e->viewport()->update();
    }

    const int total = liste.size();
    const int nbPages = qMax(1, (total + m_taillePage - 1) / m_taillePage);
    m_page = qBound(1, m_page, nbPages);
    const int debut = (m_page - 1) * m_taillePage;
    const int fin = qMin(total, debut + m_taillePage);

    m_table->setRowCount(0);
    m_table->setRowCount(fin - debut);
    for (int i = debut; i < fin; ++i) {
        const int r = i - debut;
        const EvalProjet &e = liste.at(i);
        const int id = e.id;

        auto cellule = [&](int col, QWidget *w) {
            if (w->styleSheet().isEmpty()) w->setStyleSheet("background: transparent;");
            m_table->setCellWidget(r, col, w);
            QTableWidgetItem *it = new QTableWidgetItem();
            it->setData(Qt::UserRole, id);
            m_table->setItem(r, col, it);
        };
        auto avatar = [](const QString &texte, const QString &couleur) {
            QLabel *a = new QLabel(texte);
            a->setFixedSize(26, 26);
            a->setAlignment(Qt::AlignCenter);
            a->setStyleSheet(QString("background-color: %1; color: #FFFFFF; border-radius: 13px; font-size: 9px; font-weight: bold;").arg(couleur));
            return a;
        };
        auto avecAvatar = [&](const QString &initiales, const QString &couleur, const QString &texte) {
            QWidget *w = new QWidget();
            QHBoxLayout *hl = new QHBoxLayout(w);
            hl->setContentsMargins(0, 0, 0, 0);
            hl->setSpacing(10);
            QLabel *lbl = new QLabel(texte);
            lbl->setStyleSheet("color: #E6EEFF; font-size: 12px; background: transparent;");
            lbl->setToolTip(texte);
            lbl->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
            hl->addWidget(avatar(initiales, couleur));
            hl->addWidget(lbl, 1);
            return w;
        };

        // Case à cocher
        QWidget *cCoche = new QWidget();
        QHBoxLayout *lc = new QHBoxLayout(cCoche);
        lc->setContentsMargins(0, 0, 0, 0);
        QCheckBox *chk = new QCheckBox();
        chk->setStyleSheet(UiHelpers::cssCheckbox());
        chk->setCursor(Qt::PointingHandCursor);
        connect(chk, &QCheckBox::toggled, this, &EvaluationProjetsPage::surCaseLigne);
        lc->addWidget(chk, 0, Qt::AlignCenter);
        cellule(kColCoche, cCoche);

        // Projet
        QLabel *lblProjet = new QLabel(e.titreProjet);
        lblProjet->setToolTip(e.descriptionProjet.isEmpty() ? e.titreProjet : e.titreProjet + " - " + e.descriptionProjet);
        lblProjet->setStyleSheet("color: #E6EEFF; font-size: 12px; background: transparent;");
        lblProjet->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        cellule(kColProjet, lblProjet);

        // Équipe / Juré avec avatars
        cellule(kColEquipe, avecAvatar(initialesEquipe(e.equipe),
                                       UiHelpers::couleurAvatar(UiHelpers::sommeCaracteres(e.equipe)), e.equipe));
        cellule(kColJury, avecAvatar(UiHelpers::initiales(e.nomJury), UiHelpers::couleurAvatar(e.idJury - 1), e.nomCompletJury()));

        // Date
        QLabel *lblDate = new QLabel(e.date().toString(kFormatDate));
        lblDate->setStyleSheet("color: #E6EEFF; font-size: 12px; background: transparent;");
        cellule(kColDate, lblDate);

        // Note finale
        QWidget *cNote = new QWidget();
        QHBoxLayout *ln = new QHBoxLayout(cNote);
        ln->setContentsMargins(0, 0, 0, 0);
        QLabel *pill = new QLabel(QString("%1 / %2").arg(formatNote(e.note)).arg(kNoteMax));
        pill->setAlignment(Qt::AlignCenter);
        pill->setFixedSize(70, 26);
        pill->setStyleSheet(QString("background-color: %1; color: #FFFFFF; border-radius: 5px; font-size: 11px; font-weight: bold;")
                                .arg(couleurNote(e.note)));
        ln->addWidget(pill);
        ln->addStretch();
        cellule(kColNote, cNote);

        // Statut
        QWidget *cStatut = new QWidget();
        QHBoxLayout *ls = new QHBoxLayout(cStatut);
        ls->setContentsMargins(0, 0, 0, 0);
        QLabel *badge = new QLabel("Validé");
        badge->setAlignment(Qt::AlignCenter);
        badge->setFixedSize(52, 26);
        badge->setStyleSheet("background-color: #0A8C73; color: #FFFFFF; border-radius: 5px; font-size: 11px; font-weight: bold;");
        ls->addWidget(badge);
        ls->addStretch();
        cellule(kColStatut, cStatut);

        // Actions : consulter / modifier / supprimer
        QWidget *cActions = new QWidget();
        QHBoxLayout *la = new QHBoxLayout(cActions);
        la->setContentsMargins(0, 0, 0, 0);
        la->setSpacing(8);
        QPushButton *bVoir = UiHelpers::boutonAction("act_eye.png", "Consulter l'évaluation");
        QPushButton *bEdit = UiHelpers::boutonAction("act_edit.png", "Modifier l'évaluation");
        QPushButton *bSupp = UiHelpers::boutonAction("act_trash.png", "Supprimer l'évaluation");
        bVoir->setObjectName("btnVoirEval");
        bEdit->setObjectName("btnModifierEval");
        bSupp->setObjectName("btnSupprimerEval");
        bEdit->setEnabled(m_modificationAutorisee);
        bSupp->setEnabled(m_modificationAutorisee);
        la->addWidget(bVoir);
        la->addWidget(bEdit);
        la->addWidget(bSupp);
        la->addStretch();
        connect(bVoir, &QPushButton::clicked, this, [this, id]() { detailEvaluation(id); });
        connect(bEdit, &QPushButton::clicked, this, [this, id]() { editer(id); });
        connect(bSupp, &QPushButton::clicked, this, [this, id]() { confirmerSuppression(id); });
        cellule(kColActions, cActions);
    }

    definirToutCoche(false);
    m_lblNombre->setText(QString("%1 projet%2").arg(total).arg(total > 1 ? "s" : ""));
    mettreAJourPagination(total, debut, fin, nbPages);
}

void EvaluationProjetsPage::mettreAJourPagination(int total, int debut, int fin, int nbPages)
{
    if (total == 0) m_lblAffichage->setText("Aucune évaluation ne correspond à la recherche");
    else m_lblAffichage->setText(QString("Affichage de %1 à %2 sur %3 projet%4").arg(debut + 1).arg(fin).arg(total).arg(total > 1 ? "s" : ""));

    while (QLayoutItem *item = m_pagesLayout->takeAt(0)) {
        if (QWidget *w = item->widget()) w->deleteLater();
        delete item;
    }
    for (int p = 1; p <= nbPages; ++p) {
        QPushButton *b = new QPushButton(QString::number(p));
        b->setObjectName(QString("btnPageEval%1").arg(p));
        b->setFixedSize(34, 34);
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet(p == m_page
            ? "QPushButton { background-color: #0391FC; color: #FFFFFF; border: none; border-radius: 6px; font-size: 13px; font-weight: bold; }"
            : "QPushButton { background-color: transparent; color: #FFFFFF; border: 1px solid #1F57A4; border-radius: 6px; font-size: 13px; font-weight: bold; } "
              "QPushButton:hover { background-color: #173266; border-color: #3B82F6; }");
        connect(b, &QPushButton::clicked, this, [this, p]() { allerPage(p); });
        m_pagesLayout->addWidget(b);
    }
    m_btnPrecedent->setEnabled(m_page > 1);
    m_btnSuivant->setEnabled(m_page < nbPages);
}

void EvaluationProjetsPage::chargerStatistiques()
{
    const QVector<EvalProjet> toutes = lireEvaluations();
    double somme = 0;
    QString derniere;
    for (const EvalProjet &e : toutes) {
        somme += e.note;
        if (e.dateHeure > derniere) derniere = e.dateHeure;
    }
    const double moyenne = toutes.isEmpty() ? 0.0 : somme / toutes.size();
    m_lblMoyenne->setText(QString("<span style='color:#19E6A0; font-size:22px; font-weight:bold;'>%1</span> "
                                  "<span style='color:#E6EEFF; font-size:13px;'>/ %2</span>")
                              .arg(QString::number(moyenne, 'f', 2)).arg(kNoteMax));
    m_lblNbEvals->setText(QString("<span style='font-size:15px; font-weight:bold;'>%1</span>").arg(toutes.size()));
    const QDateTime dt = QDateTime::fromString(derniere, "yyyy-MM-dd hh:mm");
    m_lblDerniere->setText(dt.isValid() ? dt.toString("dd/MM/yyyy 'à' HH:mm") : QString("—"));
}

// ====================================================================
// FORMULAIRE
// ====================================================================

void EvaluationProjetsPage::surProjetChange(int)
{
    if (m_chargement) return;
    const QString equipe = m_cbProjet->currentData(Qt::UserRole + 1).toString();
    const int i = m_cbEquipe->findText(equipe);
    if (i >= 0) {
        QSignalBlocker b(m_cbEquipe);
        m_cbEquipe->setCurrentIndex(i);
    }
}

void EvaluationProjetsPage::surEquipeChange(int)
{
    if (m_chargement) return;
    const QString equipe = m_cbEquipe->currentText();
    if (m_cbProjet->currentData(Qt::UserRole + 1).toString() == equipe) return;
    // Sélectionne le premier projet de l'équipe choisie
    for (int i = 0; i < m_cbProjet->count(); ++i) {
        if (m_cbProjet->itemData(i, Qt::UserRole + 1).toString() == equipe) {
            QSignalBlocker b(m_cbProjet);
            m_cbProjet->setCurrentIndex(i);
            return;
        }
    }
}

void EvaluationProjetsPage::surNoteChange()
{
    double notes[EvalProjet::NbCriteres];
    const QVector<int> poids = poidsCriteres();
    int totalPoids = 0;
    for (int i = 0; i < EvalProjet::NbCriteres; ++i) {
        notes[i] = m_spins[i]->value();
        m_lblScores[i]->setText(QString::number(notes[i] * poids.at(i) / 100.0, 'f', 2));
        totalPoids += poids.at(i);
    }
    const double finale = calculerNoteFinale(notes);
    m_lblNoteFinale->setText(QString("<span style='color:#19E6F0; font-size:19px; font-weight:bold;'>%1</span>"
                                     "<span style='color:#E6EEFF; font-size:11px;'> /%2</span>")
                                 .arg(formatNote(finale)).arg(kNoteMax));
    m_lblPoidsTotal->setText(QString("Poids total : %1%").arg(totalPoids));
}

void EvaluationProjetsPage::surCommentaireChange()
{
    QString t = m_txtCommentaire->toPlainText();
    if (t.size() > kCommentaireMax) {
        QSignalBlocker b(m_txtCommentaire);
        t.truncate(kCommentaireMax);
        m_txtCommentaire->setPlainText(t);
        QTextCursor c = m_txtCommentaire->textCursor();
        c.movePosition(QTextCursor::End);
        m_txtCommentaire->setTextCursor(c);
    }
    m_lblCompteur->setText(QString("%1/%2").arg(t.size()).arg(kCommentaireMax));
    m_lblCompteur->adjustSize();
    m_lblCompteur->move(m_txtCommentaire->width() - m_lblCompteur->width() - 8,
                        m_txtCommentaire->height() - m_lblCompteur->height() - 4);
}

void EvaluationProjetsPage::notesParDefaut()
{
    for (int i = 0; i < EvalProjet::NbCriteres; ++i) {
        QSignalBlocker b(m_spins[i]);
        m_spins[i]->setValue(kNotesParDefaut[i]);
    }
    surNoteChange();
}

void EvaluationProjetsPage::definirModeEdition(int id)
{
    m_idEnEdition = id;
    m_lblTitreForm->setText(id >= 0 ? "Modifier l'évaluation" : "Nouvelle évaluation");
    m_btnEnregistrer->setText(id >= 0 ? "  Mettre à jour l'évaluation" : "  Enregistrer l'évaluation");
}

void EvaluationProjetsPage::reinitialiserFormulaire()
{
    m_chargement = true;
    m_cbProjet->setCurrentIndex(m_cbProjet->count() > 0 ? 0 : -1);
    m_cbJury->setCurrentIndex(m_cbJury->count() > 0 ? 0 : -1);
    const int i = m_cbEquipe->findText(m_cbProjet->currentData(Qt::UserRole + 1).toString());
    m_cbEquipe->setCurrentIndex(i >= 0 ? i : 0);
    m_chargement = false;
    m_dateEval->setDate(QDate::currentDate());
    m_txtCommentaire->clear();
    notesParDefaut();
    definirModeEdition(-1);
}

void EvaluationProjetsPage::chargerDansFormulaire(const EvalProjet &e)
{
    m_chargement = true;
    int i = m_cbProjet->findData(e.idProjet);
    if (i >= 0) m_cbProjet->setCurrentIndex(i);
    i = m_cbEquipe->findText(e.equipe);
    if (i >= 0) m_cbEquipe->setCurrentIndex(i);
    i = m_cbJury->findData(e.idJury);
    if (i >= 0) m_cbJury->setCurrentIndex(i);
    m_chargement = false;
    m_dateEval->setDate(e.date().isValid() ? e.date() : QDate::currentDate());
    for (int k = 0; k < EvalProjet::NbCriteres; ++k) {
        QSignalBlocker b(m_spins[k]);
        m_spins[k]->setValue(e.criteres[k]);
    }
    surNoteChange();
    m_txtCommentaire->setPlainText(e.commentaire);
}

void EvaluationProjetsPage::editer(int id)
{
    if (!m_modificationAutorisee) {
        QMessageBox::information(this, "Accès restreint", "Seul un administrateur / organisateur peut modifier une évaluation.");
        return;
    }
    const EvalProjet e = lireEvaluation(id);
    if (e.id < 0) {
        QMessageBox::warning(this, "Évaluation introuvable", "Cette évaluation n'existe plus.");
        rafraichir();
        return;
    }
    chargerDansFormulaire(e);
    definirModeEdition(id);
    m_txtCommentaire->setFocus();
}

void EvaluationProjetsPage::enregistrerEvaluation()
{
    if (m_cbProjet->currentIndex() < 0 || m_cbEquipe->currentIndex() < 0) {
        QMessageBox::warning(this, "Projet manquant", "Veuillez sélectionner un projet et son équipe.");
        m_cbProjet->setFocus();
        return;
    }
    if (m_cbJury->currentIndex() < 0) {
        QMessageBox::warning(this, "Juré manquant", "Veuillez sélectionner le juré qui évalue le projet.");
        m_cbJury->setFocus();
        return;
    }
    if (m_cbProjet->currentData(Qt::UserRole + 1).toString() != m_cbEquipe->currentText()) {
        QMessageBox::warning(this, "Équipe incohérente",
                             QString("Le projet « %1 » appartient à l'équipe « %2 ».")
                                 .arg(m_cbProjet->currentText(), m_cbProjet->currentData(Qt::UserRole + 1).toString()));
        m_cbEquipe->setFocus();
        return;
    }
    if (m_dateEval->date() > QDate::currentDate()) {
        QMessageBox::warning(this, "Date invalide", "La date d'évaluation ne peut pas être dans le futur.");
        m_dateEval->setFocus();
        return;
    }
    if (m_idEnEdition >= 0 && !m_modificationAutorisee) {
        QMessageBox::information(this, "Accès restreint", "Seul un administrateur / organisateur peut modifier une évaluation.");
        return;
    }

    EvalProjet e;
    e.id = m_idEnEdition;
    e.idProjet = m_cbProjet->currentData().toInt();
    e.idJury = m_cbJury->currentData().toInt();

    // Règle de gestion : un juré n'évalue un projet qu'une seule fois
    if (Evaluation::evaluationExiste(e.idJury, e.idProjet, m_idEnEdition)) {
        QMessageBox::warning(this, "Évaluation existante",
                             QString("%1 a déjà évalué le projet « %2 ».\nModifiez l'évaluation existante depuis la liste.")
                                 .arg(m_cbJury->currentText(), m_cbProjet->currentText()));
        return;
    }

    double notes[EvalProjet::NbCriteres];
    for (int i = 0; i < EvalProjet::NbCriteres; ++i) {
        notes[i] = m_spins[i]->value();
        e.criteres[i] = notes[i];
    }
    e.note = calculerNoteFinale(notes);
    e.commentaire = m_txtCommentaire->toPlainText().left(kCommentaireMax);

    // Date saisie + heure (conservée en modification, heure courante à l'ajout)
    QTime heure = QTime::currentTime();
    if (m_idEnEdition >= 0) {
        const EvalProjet ancienne = lireEvaluation(m_idEnEdition);
        const QTime t = QTime::fromString(ancienne.dateHeure.mid(11, 5), "hh:mm");
        if (t.isValid()) heure = t;
    }
    e.dateHeure = m_dateEval->date().toString("yyyy-MM-dd") + " " + heure.toString("hh:mm");

    const bool modification = m_idEnEdition >= 0;
    if (!enregistrer(e)) {
        QMessageBox::critical(this, "Erreur", "L'enregistrement de l'évaluation dans la base de données a échoué :\n" + QSqlDatabase::database().lastError().text());
        return;
    }
    QMessageBox::information(this, "Succès",
                             modification ? "L'évaluation a été modifiée avec succès."
                                          : QString("L'évaluation a été enregistrée avec succès.\nNote finale : %1 / %2").arg(formatNote(e.note)).arg(kNoteMax));
    reinitialiserFormulaire();
    m_page = 1;
    chargerListe();
    chargerStatistiques();
    emit evaluationsModifiees();
}

// ====================================================================
// LISTE : TRI, FILTRES, PAGINATION, SÉLECTION
// ====================================================================

void EvaluationProjetsPage::surFiltreChange()
{
    m_page = 1;
    chargerListe();
}

void EvaluationProjetsPage::surTriDemande(int colonne)
{
    if (colonne == kColCoche) {
        definirToutCoche(!toutCoche());
        for (int r = 0; r < m_table->rowCount(); ++r) {
            if (QWidget *w = m_table->cellWidget(r, kColCoche)) {
                if (QCheckBox *c = w->findChild<QCheckBox *>()) {
                    QSignalBlocker b(c);
                    c->setChecked(toutCoche());
                }
            }
        }
        return;
    }
    if (colonne != kColDate && colonne != kColNote) return;
    if (m_triColonne == colonne) m_triCroissant = !m_triCroissant;
    else { m_triColonne = colonne; m_triCroissant = (colonne == kColNote ? false : true); }
    m_page = 1;
    chargerListe();
}

void EvaluationProjetsPage::surCaseLigne()
{
    bool toutes = m_table->rowCount() > 0;
    for (int r = 0; r < m_table->rowCount() && toutes; ++r) {
        QWidget *w = m_table->cellWidget(r, kColCoche);
        QCheckBox *c = w ? w->findChild<QCheckBox *>() : nullptr;
        if (!c || !c->isChecked()) toutes = false;
    }
    definirToutCoche(toutes);
}

bool EvaluationProjetsPage::toutCoche() const
{
    const EnteteTri *e = dynamic_cast<const EnteteTri *>(m_table->horizontalHeader());
    return e && e->toutCoche;
}

void EvaluationProjetsPage::definirToutCoche(bool coche)
{
    if (EnteteTri *e = dynamic_cast<EnteteTri *>(m_table->horizontalHeader())) {
        e->toutCoche = coche;
        e->viewport()->update();
    }
}

void EvaluationProjetsPage::allerPage(int page)
{
    m_page = page;
    chargerListe();
}

// ====================================================================
// CONSULTATION / SUPPRESSION
// ====================================================================

void EvaluationProjetsPage::detailEvaluation(int id)
{
    const EvalProjet e = lireEvaluation(id);
    if (e.id < 0) {
        QMessageBox::warning(this, "Évaluation introuvable", "Cette évaluation n'existe plus.");
        rafraichir();
        return;
    }

    QDialog dlg(this);
    dlg.setObjectName("dialogueDetailEvaluation");
    dlg.setWindowTitle(QString("Évaluation #%1 — %2").arg(e.id).arg(e.titreProjet));
    dlg.setStyleSheet(kDialogCss);
    dlg.setMinimumWidth(520);
    QVBoxLayout *v = new QVBoxLayout(&dlg);
    v->setContentsMargins(24, 22, 24, 22);
    v->setSpacing(12);

    QLabel *titre = new QLabel("Détail de l'évaluation");
    titre->setStyleSheet("color: #F5FAFF; font-size: 20px; font-weight: bold;");
    v->addWidget(titre);

    QGridLayout *g = new QGridLayout();
    g->setHorizontalSpacing(16);
    g->setVerticalSpacing(6);
    int l = 0;
    auto ligne = [&](const QString &k, const QString &val) {
        QLabel *a = new QLabel(k);
        a->setStyleSheet("color: #B8C7E8; font-size: 12px;");
        QLabel *b = new QLabel(val);
        b->setStyleSheet("color: #FFFFFF; font-size: 13px; font-weight: 600;");
        b->setWordWrap(true);
        g->addWidget(a, l, 0, Qt::AlignTop);
        g->addWidget(b, l, 1);
        ++l;
    };
    ligne("Projet", e.titreProjet + (e.descriptionProjet.isEmpty() ? QString() : " — " + e.descriptionProjet));
    ligne("Équipe", e.equipe);
    ligne("Juré", e.nomCompletJury());
    ligne("Date", e.date().toString(kFormatDate));
    g->setColumnStretch(1, 1);
    v->addLayout(g);

    // Grille des notes
    QGridLayout *grille = new QGridLayout();
    grille->setHorizontalSpacing(18);
    grille->setVerticalSpacing(5);
    const QStringList titres = {"Critère", "Poids", "Note", "Score"};
    for (int c = 0; c < titres.size(); ++c) {
        QLabel *t = new QLabel(titres.at(c));
        t->setStyleSheet("color: #19C3F0; font-size: 12px; font-weight: bold;");
        grille->addWidget(t, 0, c, c == 0 ? Qt::AlignLeft : Qt::AlignRight);
    }
    const QStringList noms = nomsCriteres();
    const QVector<int> poids = poidsCriteres();
    for (int i = 0; i < EvalProjet::NbCriteres; ++i) {
        const QStringList valeurs = {noms.at(i), QString("%1%").arg(poids.at(i)), QString::number(e.criteres[i], 'f', 1),
                                     QString::number(e.criteres[i] * poids.at(i) / 100.0, 'f', 2)};
        for (int c = 0; c < valeurs.size(); ++c) {
            QLabel *t = new QLabel(valeurs.at(c));
            t->setStyleSheet("color: #E6EEFF; font-size: 12px;");
            grille->addWidget(t, i + 1, c, c == 0 ? Qt::AlignLeft : Qt::AlignRight);
        }
    }
    grille->setColumnStretch(0, 1);
    v->addLayout(grille);

    QLabel *finale = new QLabel(QString("Note finale : <span style='color:#19E6F0; font-size:18px; font-weight:bold;'>%1 / %2</span>")
                                    .arg(formatNote(e.note)).arg(kNoteMax));
    finale->setTextFormat(Qt::RichText);
    v->addWidget(finale);

    if (!e.commentaire.trimmed().isEmpty()) {
        QLabel *c = new QLabel("Commentaire : " + e.commentaire);
        c->setWordWrap(true);
        c->setStyleSheet("color: #B8C7E8; font-size: 12px;");
        v->addWidget(c);
    }

    QHBoxLayout *boutons = new QHBoxLayout();
    boutons->addStretch();
    QPushButton *fermer = new QPushButton("Fermer");
    fermer->setObjectName("btnFermerDetailEvaluation");
    fermer->setProperty("principal", true);
    fermer->setCursor(Qt::PointingHandCursor);
    boutons->addWidget(fermer);
    v->addLayout(boutons);
    connect(fermer, &QPushButton::clicked, &dlg, &QDialog::accept);
    dlg.exec();
}

void EvaluationProjetsPage::confirmerSuppression(int id)
{
    if (!m_modificationAutorisee) {
        QMessageBox::information(this, "Accès restreint", "Seul un administrateur / organisateur peut supprimer une évaluation.");
        return;
    }
    const EvalProjet e = lireEvaluation(id);
    if (e.id < 0) {
        rafraichir();
        return;
    }
    const auto reponse = QMessageBox::question(
        this, "Confirmation de suppression",
        QString("Êtes-vous certain de vouloir supprimer l'évaluation du projet « %1 » par %2 ?")
            .arg(e.titreProjet, e.nomCompletJury()),
        QMessageBox::Yes | QMessageBox::No);
    if (reponse != QMessageBox::Yes) return;

    if (!supprimer(id)) {
        QMessageBox::critical(this, "Erreur", "Impossible de supprimer cette évaluation.");
        return;
    }
    if (m_idEnEdition == id) reinitialiserFormulaire();
    QMessageBox::information(this, "Suppression effectuée", "L'évaluation a été supprimée.");
    chargerListe();
    chargerStatistiques();
    emit evaluationsModifiees();
}

// ====================================================================
// MISE EN PAGE DYNAMIQUE
// ====================================================================

bool EvaluationProjetsPage::eventFilter(QObject *obj, QEvent *event)
{
    if (m_table && obj == m_table->viewport() && (event->type() == QEvent::Resize || event->type() == QEvent::Show)) {
        const int total = m_table->viewport()->width();
        const int coche = 36, date = 82, note = 88, statut = 62, actions = 94;
        const int reste = qMax(300, total - coche - date - note - statut - actions);
        QHeaderView *h = m_table->horizontalHeader();
        const int projet = reste * 29 / 100, equipe = reste * 31 / 100;
        h->resizeSection(kColCoche, coche);
        h->resizeSection(kColProjet, projet);
        h->resizeSection(kColEquipe, equipe);
        h->resizeSection(kColJury, reste - projet - equipe);
        h->resizeSection(kColDate, date);
        h->resizeSection(kColNote, note);
        h->resizeSection(kColStatut, statut);
        h->resizeSection(kColActions, actions);
    }
    if (m_txtCommentaire && obj == m_txtCommentaire && (event->type() == QEvent::Resize || event->type() == QEvent::Show)) {
        m_lblCompteur->adjustSize();
        m_lblCompteur->move(m_txtCommentaire->width() - m_lblCompteur->width() - 8,
                            m_txtCommentaire->height() - m_lblCompteur->height() - 4);
    }
    return QWidget::eventFilter(obj, event);
}
