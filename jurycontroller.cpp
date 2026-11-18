#include "jurycontroller.h"
#include "ui_mainwindow.h"
#include "scrollhelpers.h"

#include <QMainWindow>

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
// JuryController
// ---------------------------------------------------------------------------
JuryController::JuryController(Ui::MainWindow *ui, QMainWindow *host)
    : QObject(host), ui(ui), m_root(ui->pageJury)
{
    applyStyleSheet();
    setupIcons();
    setupTable();
    setupEvaluationForm();
    setupScrolling();
    setupSortAndExport();

    connect(ui->lineEditSearch, &QLineEdit::textChanged, this, &JuryController::onSearchChanged);
    connect(ui->btnAddMember, &QPushButton::clicked, this, &JuryController::onAddMember);
    connect(ui->btnPrevJury, &QPushButton::clicked, this, &JuryController::onPrevPage);
    connect(ui->btnNextJury, &QPushButton::clicked, this, &JuryController::onNextPage);
    connect(ui->tableJury, &QTableWidget::itemChanged, this, &JuryController::onItemChanged);

    loadSampleData();
    applyFilter();
}

// Le style est pose sur pageJury uniquement : il n'affecte aucune autre page.
// "padding: 0px" sur les petits boutons annule le style global des QPushButton de HackFlow.
void JuryController::applyStyleSheet()
{
    m_root->setStyleSheet(QString::fromUtf8(R"(
QWidget { font-family: "Segoe UI", "Roboto", "Helvetica Neue", sans-serif; font-size: 13px; }
QWidget#pageJury, QWidget#scrollContentJury { background-color: #101B31; }
QDialog, QMessageBox { background-color: #101B31; color: #FFFFFF; }
QLabel { background: transparent; color: #FFFFFF; }
QLabel#labelEvalSubtitle { color: #9FB0D0; font-size: 14px; }
QLabel#labelCountJury { color: #9FB0D0; }
QLabel#labelDialogTitle { font-size: 18px; font-weight: bold; }

/* ---------- Recherche + bouton ajouter ---------- */
QLineEdit {
    background-color: #1C2740;
    border: 1px solid #2F3F66;
    border-radius: 10px;
    padding: 0px 12px;
    color: #FFFFFF;
    selection-background-color: #4B26A0;
}
QLineEdit:focus { border: 1px solid #0BF8F6; }
QPushButton#btnAddMember, QPushButton#btnEvalSave {
    background-color: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #4B26A0, stop:1 #2F67E0);
    color: #FFFFFF;
    border: none;
    border-radius: 10px;
    padding: 0px 22px;
    font-weight: bold;
    font-size: 14px;
}
QPushButton#btnAddMember:hover, QPushButton#btnEvalSave:hover { background-color: #2F67E0; border: none; }
QPushButton#btnAddMember:pressed, QPushButton#btnEvalSave:pressed { background-color: #3A1D80; }

/* ---------- Carte + tableau ---------- */
QFrame#cardTableJury { background-color: #1C2740; border: 1px solid #2A3858; border-radius: 14px; }
QTableWidget#tableJury {
    background-color: transparent;
    border: none;
    color: #FFFFFF;
    font-size: 13px;
    outline: 0;
    gridline-color: transparent;
}
QTableWidget#tableJury::item {
    padding: 0px 0px 0px 8px;
    border-bottom: 1px solid #2A3858;
}
QTableWidget#tableJury::item:hover { background-color: #22304F; }
QTableWidget#tableJury::item:selected { background-color: #2A3A66; color: #FFFFFF; }
QTableWidget#tableJury QLineEdit {
    background-color: #101B31;
    color: #FFFFFF;
    border: 1px solid #0BF8F6;
    border-radius: 6px;
    padding: 0px 6px;
}
QHeaderView { background-color: transparent; }
QHeaderView::section {
    background-color: transparent;
    color: #C8D3EE;
    border: none;
    border-bottom: 1px solid #2F3F66;
    padding: 0px 0px 0px 8px;
    font-weight: bold;
}
QPushButton#btnEdit, QPushButton#btnDelete {
    background-color: transparent;
    border: none;
    border-radius: 8px;
    padding: 0px;
}
QPushButton#btnEdit:hover { background-color: #2F3F66; border: none; }
QPushButton#btnDelete:hover { background-color: #4A2338; border: none; }

QScrollBar:vertical { background: transparent; width: 12px; margin: 2px; }
QScrollBar::handle:vertical { background: #3A4C7A; border-radius: 4px; min-height: 36px; }
QScrollBar::handle:vertical:hover { background: #2F67E0; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }

/* ---------- Page Jury defilante ---------- */
QScrollArea#scrollAreaJury { background: transparent; border: none; }

/* ---------- Boutons Trier / Exporter ---------- */
QPushButton#btnSort, QPushButton#btnExport {
    background-color: #16213A;
    color: #FFFFFF;
    border: 1px solid #2F67E0;
    border-radius: 10px;
    padding: 0px 18px;
    font-size: 14px;
}
QPushButton#btnSort:hover, QPushButton#btnExport:hover { border: 1px solid #0BF8F6; background-color: #1C2A4A; }
QPushButton#btnSort:pressed, QPushButton#btnExport:pressed { background-color: #101B31; }

QMenu { background-color: #1C2740; color: #FFFFFF; border: 1px solid #2F3F66; border-radius: 8px; padding: 6px; }
QMenu::item { padding: 8px 26px; border-radius: 6px; }
QMenu::item:selected { background-color: #4B26A0; }

/* ---------- Fiche d'evaluation (liste verticale) ---------- */
QLabel#labelEvalTitle { font-size: 22px; font-weight: bold; }
QLabel#labelCardTitle { font-size: 15px; font-weight: bold; }
QLabel#labelCardHint { color: #9FB0D0; font-size: 12px; }
QLabel#labelFieldName { color: #C8D3EE; font-size: 12px; margin-top: 6px; }
QLabel#labelScoreValue { color: #0BF8F6; font-weight: bold; min-width: 58px; }
QLabel#labelTotalScore { color: #0BF8F6; font-size: 17px; font-weight: bold; }
QFrame#evalCard { background-color: #1C2740; border: 1px solid #2A3858; border-radius: 14px; }
QFrame#evalCard QLineEdit, QFrame#evalCard QComboBox, QFrame#evalCard QPlainTextEdit {
    background-color: #101B31;
    color: #FFFFFF;
    border: 1px solid #2F3F66;
    border-radius: 8px;
    padding: 4px 12px;
    selection-background-color: #4B26A0;
}
QFrame#evalCard QLineEdit, QFrame#evalCard QComboBox { min-height: 34px; }
QFrame#evalCard QLineEdit:focus, QFrame#evalCard QComboBox:focus, QFrame#evalCard QPlainTextEdit:focus { border: 1px solid #0BF8F6; }
QComboBox::drop-down { border: none; width: 30px; }
QComboBox::down-arrow { image: url(:/images/icons/chevron-down-small.png); width: 14px; height: 14px; }
QComboBox QAbstractItemView {
    background-color: #1C2740; color: #FFFFFF; border: 1px solid #2F3F66;
    selection-background-color: #4B26A0; outline: 0;
}
QSlider::groove:horizontal { height: 6px; background: #2F3F66; border-radius: 3px; }
QSlider::sub-page:horizontal {
    background-color: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #4B26A0, stop:1 #2F67E0);
    border-radius: 3px;
}
QSlider::handle:horizontal { background: #0BF8F6; width: 16px; height: 16px; margin: -5px 0px; border-radius: 8px; }
QPushButton#btnEvalSave, QPushButton#btnEvalReset { border-radius: 10px; padding: 0px 22px; font-weight: bold; font-size: 14px; }
QPushButton#btnEvalReset { background-color: transparent; color: #C8D3EE; border: 1px solid #2F3F66; }
QPushButton#btnEvalReset:hover { border: 1px solid #0BF8F6; color: #0BF8F6; background-color: transparent; }

/* ---------- Pagination ---------- */
QPushButton#btnPrevJury, QPushButton#btnNextJury, QPushButton#btnPageJury {
    background-color: #1C2740;
    color: #FFFFFF;
    border: 1px solid #2F3F66;
    border-radius: 8px;
    font-size: 15px;
    padding: 0px;
}
QPushButton#btnPrevJury:hover, QPushButton#btnNextJury:hover, QPushButton#btnPageJury:hover {
    border: 1px solid #0BF8F6;
    background-color: #1C2740;
}
QPushButton#btnPageJury:checked {
    background-color: #4B26A0;
    border: 1px solid #4B26A0;
    font-weight: bold;
}
QPushButton#btnPrevJury:disabled, QPushButton#btnNextJury:disabled { color: #4D5B7E; border: 1px solid #243253; }

/* ---------- Boites de dialogue ---------- */
QDialog QPushButton, QMessageBox QPushButton {
    background-color: #2F67E0;
    color: #FFFFFF;
    border: none;
    border-radius: 8px;
    padding: 8px 20px;
    min-width: 80px;
}
QDialog QPushButton:hover, QMessageBox QPushButton:hover { background-color: #4B26A0; border: none; }
QDialog QLineEdit { min-height: 34px; }
)"));
}

// Icones des champs et boutons (PNG blancs recolores par le code)
void JuryController::setupIcons()
{
    ui->lineEditSearch->addAction(tintedIcon(":/images/icons/search.png", QColor("#9FB0D0"), 18),
                                  QLineEdit::LeadingPosition);

    ui->btnPrevJury->setIcon(tintedIcon(":/images/icons/chevron-left.png", QColor("#FFFFFF"), 18));
    ui->btnNextJury->setIcon(tintedIcon(":/images/icons/chevron-right.png", QColor("#FFFFFF"), 18));
    ui->btnPrevJury->setIconSize(QSize(18, 18));
    ui->btnNextJury->setIconSize(QSize(18, 18));
}

// --- Tableau ---------------------------------------------------------------
void JuryController::setupTable()
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

void JuryController::loadSampleData()
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

int JuryController::pageCount() const
{
    return qMax(1, (m_filtered.size() + m_pageSize - 1) / m_pageSize);
}

void JuryController::applyFilter()
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

void JuryController::refreshTable()
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
    ui->btnPrevJury->setEnabled(m_page > 0);
    ui->btnNextJury->setEnabled(m_page < pages - 1);
    ui->labelCountJury->setText(total == 0
        ? QString("Aucun membre trouvé")
        : QString("Affichage de %1 à %2 sur %3 membres").arg(start + 1).arg(end).arg(total));

    refreshJurorCombo();
}

void JuryController::rebuildPagination(int pages)
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
        b->setObjectName("btnPageJury");
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
void JuryController::onSearchChanged(const QString &)
{
    m_page = 0;
    applyFilter();
}

void JuryController::onPrevPage()
{
    --m_page;
    refreshTable();
}

void JuryController::onNextPage()
{
    ++m_page;
    refreshTable();
}

void JuryController::onItemChanged(QTableWidgetItem *item)
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
            QMessageBox::warning(m_root, "Valeur invalide", error);
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

void JuryController::onAddMember()
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

void JuryController::deleteMember(int sourceIndex)
{
    if (sourceIndex < 0 || sourceIndex >= m_all.size())
        return;

    const JuryMember &m = m_all.at(sourceIndex);

    QMessageBox box(QMessageBox::Question, "Supprimer un membre",
                    QString("Voulez-vous vraiment supprimer %1 %2 ?").arg(m.prenom, m.nom),
                    QMessageBox::NoButton, m_root);
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

bool JuryController::askNewMember(JuryMember &m)
{
    QDialog dlg(m_root);
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
void JuryController::setupScrolling()
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
void JuryController::setupEvaluationForm()
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

    connect(ui->btnEvalSave, &QPushButton::clicked, this, &JuryController::onEvalSave);
    connect(ui->btnEvalReset, &QPushButton::clicked, this, &JuryController::onEvalReset);
    updateTotalScore();
}

void JuryController::updateTotalScore()
{
    int sum = 0;
    for (const QSlider *s : std::as_const(m_scoreSliders))
        sum += s->value();
    ui->labelTotalScore->setText(
        QString("Note globale : %1 / %2").arg(sum).arg(m_scoreSliders.size() * kMaxScore));
}

void JuryController::refreshJurorCombo()
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

void JuryController::onEvalReset()
{
    for (QSlider *s : std::as_const(m_scoreSliders))
        s->setValue(0);
    m_cmbProject->setCurrentIndex(0);
    m_cmbJuror->setCurrentIndex(0);
    m_cmbReco->setCurrentIndex(0);
    m_editTeam->clear();
    m_editComment->clear();
}

void JuryController::onEvalSave()
{
    if (m_cmbProject->currentIndex() <= 0 || m_cmbJuror->currentIndex() <= 0) {
        QMessageBox::warning(m_root, "Champs obligatoires",
                             "Veuillez sélectionner le projet évalué et le membre du jury évaluateur.");
        return;
    }

    int sum = 0;
    for (const QSlider *s : std::as_const(m_scoreSliders))
        sum += s->value();

    QMessageBox::information(m_root, "Évaluation enregistrée",
        QString("Projet : %1\nÉvaluateur : %2\nNote globale : %3 / %4\nRecommandation : %5")
            .arg(m_cmbProject->currentText(), m_cmbJuror->currentText())
            .arg(sum).arg(m_scoreSliders.size() * kMaxScore)
            .arg(m_cmbReco->currentText()));

    onEvalReset();
}

// --- Trier / Exporter ------------------------------------------------------
void JuryController::setupSortAndExport()
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

    QMenu *menu = new QMenu(m_root);
    for (const SortOption &o : options) {
        QAction *a = menu->addAction(o.text);
        connect(a, &QAction::triggered, this, [this, o]() { sortMembers(o.column, o.ascending); });
    }

    connect(ui->btnSort, &QPushButton::clicked, this, [this, menu]() {
        menu->exec(ui->btnSort->mapToGlobal(QPoint(0, ui->btnSort->height() + 4)));
    });
    connect(ui->btnExport, &QPushButton::clicked, this, &JuryController::exportMembers);
}

void JuryController::sortMembers(Column column, bool ascending)
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

void JuryController::exportMembers()
{
    QString path = QFileDialog::getSaveFileName(m_root, "Exporter les membres du jury",
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
        QMessageBox::warning(m_root, "Export impossible",
                             "Impossible d'écrire le fichier :\n" + path);
        return;
    }
    file.write("\xEF\xBB\xBF");            // BOM UTF-8 : accents corrects dans Excel
    file.write(out.toUtf8());
    file.close();

    QMessageBox::information(m_root, "Export terminé",
        QString("%1 membre(s) exporté(s) vers :\n%2").arg(m_filtered.size()).arg(path));
}
