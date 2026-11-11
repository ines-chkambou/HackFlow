#include "participantscontroller.h"
#include "ui_mainwindow.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QAction>
#include <QActionGroup>
#include <QCheckBox>
#include <QDateTime>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMainWindow>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTableWidget>
#include <QTextStream>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidgetAction>
#include <algorithm>

#ifdef HAVE_QTPDF            // defini par SmartHackathon.pro si le module Qt PDF est installe
#include <QPdfDocument>
#endif

static const QStringList kRoles   = { "Organisateur", "Participant" };
static const QStringList kEquipes = { "Pédagogie/Mentorat", "GreenTeam", "MedTech", "EcoSolutions",
                                      "SmartCity", "Clean Energy", "UrbanAI", "HealthCare", "TechVision" };
static const QStringList kDispos  = { "Disponible", "En cours", "En attente" };

// ---------------------------------------------------------------------------
//  Icônes (Lucide) : fichiers blancs dans icons/ (resources.qrc), teintés à la demande
// ---------------------------------------------------------------------------
static QPixmap iconPixmap(const QString &name, const QColor &color = Qt::white, int size = 20)
{
    const QPixmap src(QString(":/part/%1.png").arg(name));
    if (src.isNull())
        return QPixmap();
    const qreal dpr = 2.0;                                   // net sur écrans HiDPI
    QPixmap pm = src.scaled(int(size * dpr), int(size * dpr),
                            Qt::KeepAspectRatio, Qt::SmoothTransformation);
    pm.setDevicePixelRatio(dpr);
    QPainter p(&pm);
    p.setCompositionMode(QPainter::CompositionMode_SourceIn);
    p.fillRect(QRect(0, 0, size, size), color);
    p.end();
    return pm;
}

static QIcon iconOf(const QString &name, const QColor &color = Qt::white, int size = 20)
{
    return QIcon(iconPixmap(name, color, size));
}

static QString iconHtml(const QString &name, int size = 20)
{
    return QString("<img src=\":/part/%1.png\" width=\"%2\" height=\"%2\" style=\"vertical-align:middle\">")
            .arg(name).arg(size);
}

static QString formTitleHtml(bool editing)
{
    return editing ? iconHtml("pencil", 22) + "&nbsp;&nbsp;Modifier un participant"
                   : iconHtml("circle-plus", 24) + "&nbsp;&nbsp;Ajouter un participant";
}

// ---------------------------------------------------------------------------
//  Lecture du texte d'un PDF + détection de l'équipe à partir du CV
// ---------------------------------------------------------------------------

// Texte « brut » : décompresse les flux FlateDecode et récupère les chaînes (...) des opérateurs Tj/TJ.
// Solution de secours (PDF simples) si le module Qt PDF n'est pas disponible.
static QString extractPdfTextRaw(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return QString();
    const QByteArray data = f.readAll();

    static const QRegularExpression strRe(R"re(\(((?:\\.|[^\\()])*)\))re");
    QString out;
    int pos = 0;
    while (out.size() < 400000) {
        const int s = data.indexOf("stream", pos);
        if (s < 0) break;
        int start = s + 6;
        if (data.mid(start, 2) == "\r\n")                          start += 2;
        else if (data.mid(start, 1) == "\n" || data.mid(start, 1) == "\r") start += 1;
        const int e = data.indexOf("endstream", start);
        if (e < 0) break;
        pos = e + 9;

        QByteArray chunk = data.mid(start, e - start);
        const QByteArray dict = data.mid(qMax(0, s - 300), qMin(300, s));
        if (dict.contains("FlateDecode")) {
            const quint32 n = qMin<quint32>(quint32(chunk.size()) * 20u + 1024u, 64u * 1024u * 1024u);
            QByteArray pre(4, 0);
            pre[0] = char(n >> 24); pre[1] = char(n >> 16); pre[2] = char(n >> 8); pre[3] = char(n);
            chunk = qUncompress(pre + chunk);
        }
        if (chunk.isEmpty() || !chunk.contains("BT"))
            continue;

        const QString txt = QString::fromLatin1(chunk);
        int prevEnd = 0;
        auto it = strRe.globalMatch(txt);
        while (it.hasNext()) {
            const QRegularExpressionMatch m = it.next();
            const QString gap = txt.mid(prevEnd, m.capturedStart() - prevEnd);
            bool hasLetter = false;
            for (const QChar c : gap) if (c.isLetter()) { hasLetter = true; break; }
            if (hasLetter) out += ' ';                    // opérateur entre deux chaînes = nouveau mot
            const QString raw = m.captured(1);
            for (int i = 0; i < raw.size(); ++i) {        // décodage des échappements PDF
                QChar c = raw[i];
                if (c == '\\' && i + 1 < raw.size()) {
                    const QChar n2 = raw[++i];
                    if      (n2 == 'n' || n2 == 'r') out += ' ';
                    else if (n2 == 't')              out += ' ';
                    else if (n2 >= '0' && n2 <= '7') {
                        int v = n2.digitValue();
                        for (int k = 0; k < 2 && i + 1 < raw.size() && raw[i + 1] >= '0' && raw[i + 1] <= '7'; ++k)
                            v = v * 8 + raw[++i].digitValue();
                        out += QChar(v & 0xFF);
                    } else out += n2;
                } else out += c;
            }
            prevEnd = m.capturedEnd();
        }
        out += ' ';
    }
    return out;
}

static QString extractPdfText(const QString &path)
{
#if defined(HAVE_QTPDF) && QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
    {
        QPdfDocument doc;
        if (doc.load(path) == QPdfDocument::Error::None) {
            QString all;
            const int pages = qMin(doc.pageCount(), 6);              // un CV tient sur quelques pages
            for (int i = 0; i < pages; ++i)
                all += doc.getAllText(i).text() + "\n";
            if (!all.trimmed().isEmpty())
                return all;
        }
    }
#endif
    return extractPdfTextRaw(path);
}

// minuscules + sans accents (ligatures « ﬁ » dépliées)
static QString foldText(const QString &s)
{
    const QString d = s.normalized(QString::NormalizationForm_KD);
    QString out;
    out.reserve(d.size());
    for (const QChar c : d)
        if (c.category() != QChar::Mark_NonSpacing)
            out += c.toLower();
    return out;
}

struct TeamRule
{
    QString team;
    QRegularExpression alias;                 // nom de l'équipe écrit dans le CV (« Green Team »...)
    QRegularExpression label;                 // « Équipe : GreenTeam »
    QList<QRegularExpression> keywords;       // domaines / compétences typiques de l'équipe
};

static const QList<TeamRule> &teamRules()
{
    static const QList<TeamRule> rules = [] {
        QList<TeamRule> list;
        auto add = [&list](const QString &team, const QString &alias, const QStringList &kws) {
            TeamRule r;
            r.team  = team;
            r.alias = QRegularExpression(alias);
            r.label = QRegularExpression(
                QString("(?:equipe|team|groupe)\\s*(?:\\(s\\))?\\s*[:\\-\\x{2013}]?\\s*(?:%1)").arg(alias));
            for (const QString &k : kws)
                r.keywords.append(QRegularExpression(QString("(?<![a-z0-9])(?:%1)(?![a-z0-9])").arg(k)));
            list.append(r);
        };
        add("Pédagogie/Mentorat", R"re(pedagogie\s*/?\s*mentorat)re",
           { R"re(pedagog\w*)re",
            R"re(enseign\w*)re",
            R"re(formateur)re",
            R"re(formatrice)re",
            R"re(mentor\w*)re",
            R"re(coach\w*)re",
            R"re(educat\w*)re",
            R"re(professeur)re",
            R"re(tuteur)re",
            R"re(tutorat)re",
            R"re(tutor\w*)re",
            R"re(teach\w*)re",
            R"re(trainer)re",
            R"re(didactique)re" });
        add("GreenTeam", R"re(green\s*team)re",
           { R"re(environnement\w*)re",
            R"re(environment\w*)re",
            R"re(ecolog\w*)re",
            R"re(developpement\s+durable)re",
            R"re(durable)re",
            R"re(biodiversite)re",
            R"re(climat\w*)re",
            R"re(sustainab\w*)re",
            R"re(agricultur\w*)re",
            R"re(reforestation)re",
            R"re(carbone)re",
            R"re(carbon)re",
            R"re(vert)re",
            R"re(green)re" });
        add("MedTech", R"re(med\s*tech)re",
           { R"re(medtech)re",
            R"re(biomedical\w*)re",
            R"re(dispositifs?\s+medica\w*)re",
            R"re(medical\s+devices?)re",
            R"re(imagerie)re",
            R"re(diagnostic\w*)re",
            R"re(prothese\w*)re",
            R"re(telemedecine)re",
            R"re(genie\s+biomedical)re" });
        add("EcoSolutions", R"re(eco\s*-?\s*solutions?)re",
           { R"re(economie\s+circulaire)re",
            R"re(circular\s+economy)re",
            R"re(recycl\w*)re",
            R"re(dechet\w*)re",
            R"re(waste)re",
            R"re(assainissement)re",
            R"re(depollution)re",
            R"re(traitement\s+des\s+eaux)re",
            R"re(water\s+treatment)re",
            R"re(eco-?conception)re",
            R"re(eco-?responsable)re",
            R"re(eco-?friendly)re",
            R"re(valorisation)re" });
        add("SmartCity", R"re(smart\s*-?\s*city)re",
           { R"re(smart\s*-?\s*city)re",
            R"re(villes?\s+intelligentes?)re",
            R"re(urbanisme)re",
            R"re(urbain\w*)re",
            R"re(mobilite)re",
            R"re(transport\w*)re",
            R"re(traffic)re",
            R"re(trafic)re",
            R"re(iot)re",
            R"re(internet\s+des\s+objets)re",
            R"re(capteur\w*)re",
            R"re(infrastructure\w*)re",
            R"re(amenagement)re",
            R"re(smart\s*-?\s*grid)re" });
        add("Clean Energy", R"re(clean\s*energy)re",
           { R"re(energie\w*)re",
            R"re(energy)re",
            R"re(solaire)re",
            R"re(solar)re",
            R"re(photovoltaique)re",
            R"re(eolien\w*)re",
            R"re(wind)re",
            R"re(renouvelable\w*)re",
            R"re(renewable\w*)re",
            R"re(hydrogene)re",
            R"re(batterie\w*)re",
            R"re(electrique\w*)re",
            R"re(reseau\s+electrique)re" });
        add("UrbanAI", R"re(urban\s*-?\s*ai)re",
           { R"re(intelligence\s+artificielle)re",
            R"re(artificial\s+intelligence)re",
            R"re(machine\s+learning)re",
            R"re(deep\s+learning)re",
            R"re(apprentissage\s+automatique)re",
            R"re(ia)re",
            R"re(ai)re",
            R"re(data\s*-?\s*scien\w*)re",
            R"re(vision\s+par\s+ordinateur)re",
            R"re(computer\s+vision)re",
            R"re(neural\w*)re",
            R"re(nlp)re",
            R"re(tensorflow)re",
            R"re(pytorch)re",
            R"re(keras)re",
            R"re(scikit-?learn)re",
            R"re(opencv)re",
            R"re(llm)re" });
        add("HealthCare", R"re(health\s*-?\s*care)re",
           { R"re(sante)re",
            R"re(health\w*)re",
            R"re(medecin\w*)re",
            R"re(infirmi\w*)re",
            R"re(hopital)re",
            R"re(hopitaux)re",
            R"re(clinique\w*)re",
            R"re(patient\w*)re",
            R"re(pharmac\w*)re",
            R"re(soins)re",
            R"re(nursing)re",
            R"re(chirurg\w*)re",
            R"re(medical\w*)re",
            R"re(medicale\w*)re" });
        add("TechVision", R"re(tech\s*-?\s*vision)re",
           { R"re(developpeu\w*)re",
            R"re(developpement\s+(?:web|logiciel|mobile))re",
            R"re(software)re",
            R"re(full\s*-?\s*stack)re",
            R"re(front\s*-?\s*end)re",
            R"re(back\s*-?\s*end)re",
            R"re(react)re",
            R"re(angular)re",
            R"re(vue)re",
            R"re(javascript)re",
            R"re(typescript)re",
            R"re(java)re",
            R"re(c\+\+)re",
            R"re(php)re",
            R"re(devops)re",
            R"re(cloud)re",
            R"re(cybersecurite)re",
            R"re(informatique)re",
            R"re(programmation)re",
            R"re(qt)re",
            R"re(html)re",
            R"re(css)re",
            R"re(sql)re",
            R"re(genie\s+logiciel)re",
            R"re(web)re" });
        return list;
    }();
    return rules;
}

// Retourne l'équipe la plus probable (nom exact de kEquipes) ou "" si le CV n'est pas assez clair.
static QString guessTeamFromText(const QString &rawText)
{
    const QString text = foldText(rawText);
    QString bestTeam;
    int best = 0, second = 0;
    for (const TeamRule &r : teamRules()) {
        if (r.label.match(text).hasMatch())
            return r.team;                                        // « Équipe : X » écrit dans le CV
        int score = r.alias.match(text).hasMatch() ? 8 : 0;       // nom de l'équipe cité
        for (const QRegularExpression &re : r.keywords) {
            int n = 0;
            auto it = re.globalMatch(text);
            while (it.hasNext() && n < 4) { it.next(); ++n; }     // 4 occurrences max par mot-clé
            score += n;
        }
        if (score > best)        { second = best; best = score; bestTeam = r.team; }
        else if (score > second) { second = score; }
    }
    return (best >= 3 && best > second) ? bestTeam : QString();
}

// ---------------------------------------------------------------------------
//  Style (theme sombre bleu / neon)
//  - kPageStyle : applique UNIQUEMENT aux deux panneaux du module (formPanel, tablePanel)
//    pour ne pas modifier l'apparence des autres pages de HackFlow ;
//  - les widgets hors de ces panneaux (toast, panneau de notifications, menu de tri,
//    boites de dialogue) recoivent leur propre style ci-dessous.
// ---------------------------------------------------------------------------
static const char kPageStyle[] = R"(
QWidget { color: #e6ecff; font-family: "Segoe UI", "Inter", sans-serif; font-size: 13px; }
QLabel  { background: transparent; }

#formPanel, #tablePanel {
    background: rgba(6,18,58,0.85); border: 1px solid #1f46a8; border-radius: 14px;
}
#formPanel QLabel, #tablePanel QLabel { border: none; }
#formTitle { font-size: 18px; font-weight: 700; color: white; padding-bottom: 6px; }
#pdfDropZone { background: rgba(8,24,63,0.55); border: 1px dashed #654bff; border-radius: 9px; }
#pdfIcon { color: #9a72ff; font-size: 21px; font-weight: 700; }
#pdfPrompt { color: #dbe4ff; font-size: 11px; font-weight: 600; }
#pdfHint { color: #8298ce; font-size: 9px; }
#formPanel QLineEdit, #formPanel QComboBox { min-height: 25px; padding: 0 9px; border-radius: 7px; font-size: 11px; }
#formPanel QLabel#lblId, #formPanel QLabel#lblNom, #formPanel QLabel#lblPrenom,
#formPanel QLabel#lblEmail, #formPanel QLabel#lblTel, #formPanel QLabel#lblRole, #formPanel QLabel#lblEquipe,
#formPanel QLabel#lblDispo, #formPanel QLabel#lblPieceJointe { color: #dbe4ff; margin-top: 2px; font-size: 10px; }

QLineEdit, QComboBox {
    background: #08183f; border: 1px solid #1f46a8; border-radius: 8px;
    padding: 0 12px; min-height: 32px; color: #e6ecff; selection-background-color: #5b4bff;
}
QLineEdit:focus, QComboBox:focus { border: 1px solid #19d3ff; }
QLineEdit[readOnly="true"] { color: #8fa3d6; }
QComboBox::drop-down { border: none; width: 28px; }
QComboBox::down-arrow { image: url(:/part/chevron-down-small.png); width: 14px; height: 14px; }
QComboBox QAbstractItemView {
    background: #08183f; border: 1px solid #1f46a8; selection-background-color: #5b4bff; color: #e6ecff;
}

/* ---------- Boutons ---------- */
#btnAjouterPart {
    border: none; border-radius: 8px; font-size: 12px; font-weight: 600; color: white;
    background: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #3b6bff, stop:1 #6a4bff);
}
#btnAjouterPart:hover { background: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #5b82ff, stop:1 #8266ff); }
#btnViderPart, #btnTrierPart, #btnExporterPart, #btnPrev, #btnNext {
    background: #08183f; border: 1px solid #1f46a8; border-radius: 8px; color: #e6ecff;
}
#btnViderPart:hover, #btnTrierPart:hover, #btnExporterPart:hover,
#btnPrev:hover, #btnNext:hover { border-color: #19d3ff; }
#btnPage { background: #5b4bff; border: none; border-radius: 8px; color: white; font-weight: 700; }

/* ---------- Tableau ---------- */
QTableWidget {
    background: transparent; border: 1px solid #1f46a8; border-radius: 10px;
    gridline-color: transparent; outline: 0;
}
QTableWidget::item { border-bottom: 1px solid #0f2a66; padding-left: 4px; }
QHeaderView { background: transparent; }
QHeaderView::section {
    background: rgba(8,24,70,0.8); color: #6ea8ff; font-weight: 600;
    border: none; border-bottom: 1px solid #1f46a8; padding: 12px 6px;
}
QCheckBox::indicator {
    width: 16px; height: 16px; border: 1.5px solid #3b6bff; border-radius: 4px; background: transparent;
}
QCheckBox::indicator:checked { background: #5b4bff; border-color: #5b4bff; }

QLabel[badge="organisateur"] { background: #3a2fd8; color: white;   border-radius: 4px; padding: 6px 4px; font-size: 12px; }
QLabel[badge="participant"]  { background: #2563eb; color: white;   border-radius: 4px; padding: 6px 4px; font-size: 12px; }
QLabel[badge="disponible"]   { background: #0c6b5c; color: #6dffd0; border-radius: 4px; padding: 6px 4px; font-size: 12px; }
QLabel[badge="en cours"]     { background: #7a4512; color: #ffb36b; border-radius: 4px; padding: 6px 4px; font-size: 12px; }
QLabel[badge="en attente"]   { background: #70401a; color: #ffad70; border-radius: 4px; padding: 6px 4px; font-size: 12px; }

QToolButton[act="view"]   { color: #19d3ff; }
QToolButton[act="edit"]   { color: #4aa8ff; }
QToolButton[act="delete"] { color: #ff4d6a; }
QToolButton[act] { border: none; background: transparent; font-size: 17px; padding: 4px; }
QToolButton[act]:hover { background: rgba(91,75,255,0.25); border-radius: 6px; }

#countLabel { color: #9fb3e8; }
)";

static const char kBadgeStyle[] =
    "background: #ff4d6a; color: white; border-radius: 9px; font-size: 10px; font-weight: 700;";

static const char kToastStyle[] = R"(
QPushButton { text-align: left; padding: 8px 14px; background: #0b2260; color: #e6ecff;
              border: 1px solid #19d3ff; border-radius: 10px; font-size: 12px; }
QPushButton:hover { background: #12308a; }
)";

static const char kNotifStyle[] = R"(
#notifPanel { background: #071445; border: 1px solid #1f46a8; border-radius: 12px; }
#notifPanel QLabel { background: transparent; border: none; color: #e6ecff; }
#notifInfo { background: rgba(8,24,70,0.8); border: 1px solid #1f46a8; border-radius: 8px; }
#notifInfoText { color: #8fb4ff; font-size: 11px; }
#notifHeader { font-size: 15px; font-weight: 700; color: white; }
#notifEmpty  { color: #8fa3d6; padding: 14px 0; }
#notifClear  { background: transparent; border: none; color: #19d3ff; padding: 4px 6px; }
#notifClear:hover { color: white; }
#notifItem { background: rgba(8,24,70,0.9); border: 1px solid #1f46a8; border-radius: 8px; }
#notifItem[unread="true"] { border: 1px solid #19d3ff; }
#notifItemTitle { font-weight: 700; color: #6ea8ff; }
#notifItemTime  { color: #8fa3d6; font-size: 11px; }
#notifItemText  { color: #dbe4ff; }
)";

static const char kMenuStyle[] = R"(
QMenu { background: #08183f; border: 1px solid #1f46a8; border-radius: 8px; padding: 6px; color: #e6ecff; }
QMenu::item { padding: 8px 28px 8px 26px; border-radius: 6px; color: #e6ecff; }
QMenu::item:selected { background: #5b4bff; color: white; }
QMenu::separator { height: 1px; background: #1f46a8; margin: 6px 8px; }
QMenu::indicator { width: 14px; height: 14px; left: 6px; }
)";

static const char kDialogStyle[] = R"(
QMessageBox, QDialog { background: #071445; }
QMessageBox QLabel { color: #e6ecff; }
QMessageBox QPushButton {
    background: #5b4bff; border: none; border-radius: 6px; padding: 6px 18px; color: white;
}
)";

void ParticipantsController::applyStyleSheet()
{
    ui->formPanel->setStyleSheet(kPageStyle);
    ui->tablePanel->setStyleSheet(kPageStyle);
}

// ---------------------------------------------------------------------------
//  Construction : les widgets sont ceux de la page « pageParticipants » du
//  fichier mainwindow.ui (un seul .ui, convention du document « Bonnes pratiques »).
//  Les noms qui existaient deja dans la page Employes ont le suffixe « Part »
//  (btnAjouterPart, btnViderPart, btnTrierPart, btnExporterPart, comboRolePart, comboDispoPart).
// ---------------------------------------------------------------------------
ParticipantsController::ParticipantsController(Ui::MainWindow *ui, QMainWindow *host)
    : QObject(host), ui(ui), m_host(host)
{
    applyStyleSheet();
    setupCombos();
    setupTable();
    if (!loadData()) {          // premier lancement : donnees d'exemple, puis sauvegarde
        loadSampleData();
        saveData();
    }

    ui->pdfDropZone->setAcceptDrops(true);
    ui->pdfDropZone->setCursor(Qt::PointingHandCursor);
    ui->pdfDropZone->installEventFilter(this);
    for (QLabel *label : ui->pdfDropZone->findChildren<QLabel *>()) {
        label->setAttribute(Qt::WA_TransparentForMouseEvents);
        label->setCursor(Qt::PointingHandCursor);
    }
    setupNotifications();
    loadNotifs();
    setupSortMenu();
    setupIcons();
    setupTopSearch();

    // Connexions (CRUD désactivé pour Participant : btnAjouterPart n'est pas connecté)
    ui->btnAjouterPart->setEnabled(false);
    ui->btnAjouterPart->setToolTip("Création désactivée");
    // connect(ui->btnAjouterPart,   &QPushButton::clicked, this, &ParticipantsController::onAjouter);
    connect(ui->btnViderPart,     &QPushButton::clicked, this, &ParticipantsController::onVider);
    connect(ui->btnTrierPart,     &QPushButton::clicked, this, &ParticipantsController::onSortClicked);
    connect(ui->btnExporterPart,  &QPushButton::clicked, this, &ParticipantsController::onExporter);
    connect(ui->btnPrev,          &QPushButton::clicked, this, &ParticipantsController::onPrevPage);
    connect(ui->btnNext,          &QPushButton::clicked, this, &ParticipantsController::onNextPage);

    connect(ui->comboFiltreRole,   &QComboBox::currentIndexChanged, this, &ParticipantsController::onFiltersChanged);
    connect(ui->comboFiltreEquipe, &QComboBox::currentIndexChanged, this, &ParticipantsController::onFiltersChanged);
    connect(ui->comboFiltreDispo,  &QComboBox::currentIndexChanged, this, &ParticipantsController::onFiltersChanged);

    connect(ui->participantsTable->horizontalHeader(), &QHeaderView::sectionClicked,
            this, &ParticipantsController::onHeaderClicked);

    refreshTable();
}

ParticipantsController::~ParticipantsController() = default;

// Dialogues : meme theme sombre que le module, quel que soit le style de la fenetre
int ParticipantsController::showMsg(QMessageBox::Icon icon, const QString &title, const QString &text)
{
    QMessageBox box(icon, title, text,
                    icon == QMessageBox::Question ? (QMessageBox::Yes | QMessageBox::No) : QMessageBox::Ok,
                    m_host);
    box.setStyleSheet(kDialogStyle);
    return box.exec();
}

// La barre de recherche du haut (lineTopSearch) est celle de HackFlow : elle filtre les
// participants uniquement quand la page « Participants » est affichee.
void ParticipantsController::setupTopSearch()
{
    m_searchPlaceholder = ui->lineTopSearch->placeholderText();
    connect(ui->lineTopSearch, &QLineEdit::textChanged, this, [this] {
        if (isParticipantsPageVisible()) onFiltersChanged();
    });
    connect(ui->stackedPages, &QStackedWidget::currentChanged, this, [this](int) {
        const bool onPart = isParticipantsPageVisible();
        ui->lineTopSearch->blockSignals(true);
        ui->lineTopSearch->clear();                 // chaque page repart d'une recherche vide
        ui->lineTopSearch->blockSignals(false);
        ui->lineTopSearch->setPlaceholderText(onPart ? QStringLiteral("Rechercher un participant...")
                                                     : m_searchPlaceholder);
        if (onPart) { m_page = 1; refreshTable(); }
    });
}

bool ParticipantsController::isParticipantsPageVisible() const
{
    return ui->stackedPages->currentWidget() == ui->pageParticipants;
}

// ---------------------------------------------------------------------------
//  Icones du formulaire, des filtres et de la pagination
// ---------------------------------------------------------------------------
void ParticipantsController::setupIcons()
{
    // Formulaire
    const QColor field("#9fb3e8");
    ui->formTitle->setText(formTitleHtml(false));
    ui->editId->addAction(iconOf("user", field, 16),     QLineEdit::LeadingPosition);
    ui->editNom->addAction(iconOf("user", field, 16),    QLineEdit::LeadingPosition);
    ui->editPrenom->addAction(iconOf("user", field, 16), QLineEdit::LeadingPosition);
    ui->editEmail->addAction(iconOf("mail", field, 16),  QLineEdit::LeadingPosition);
    ui->btnAjouterPart->setIcon(iconOf("plus", Qt::white, 18));
    ui->btnViderPart->setIcon(iconOf("rotate-cw", Qt::white, 18));

    // Filtres, export, pagination
    ui->btnTrierPart->setIcon(iconOf("arrow-up-down", Qt::white, 18));
    ui->btnExporterPart->setIcon(iconOf("download", Qt::white, 18));
    ui->btnPrev->setIcon(iconOf("chevron-left", Qt::white, 18));
    ui->btnNext->setIcon(iconOf("chevron-right", Qt::white, 18));
    for (QPushButton *b : { ui->btnAjouterPart, ui->btnViderPart, ui->btnTrierPart, ui->btnExporterPart,
                            ui->btnPrev, ui->btnNext })
        b->setIconSize(QSize(18, 18));
}

// ---------------------------------------------------------------------------
//  Notifications : cloche de HackFlow (btnBell) -> pastille rouge + liste deroulante
// ---------------------------------------------------------------------------
void ParticipantsController::setupNotifications()
{
    ui->btnBell->setCursor(Qt::PointingHandCursor);

    m_notifBadge = new QLabel(ui->btnBell);
    m_notifBadge->setObjectName("notifBadge");
    m_notifBadge->setStyleSheet(kBadgeStyle);
    m_notifBadge->setAlignment(Qt::AlignCenter);
    m_notifBadge->setFixedSize(18, 18);
    m_notifBadge->move(24, 1);                 // coin superieur droit de la cloche (40 x 38)
    m_notifBadge->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_notifBadge->hide();

    connect(ui->btnBell, &QToolButton::clicked, this, &ParticipantsController::showNotifications);
    qApp->installEventFilter(this);            // pour refermer le panneau au clic a l'exterieur
}


void ParticipantsController::addNotification(const QString &titre, const QString &texte, int participantId)
{
    Notification n;
    n.participantId = participantId;
    n.titre = titre;
    n.texte = texte;
    n.date  = QDateTime::currentDateTime();
    m_notifs.append(n);
    if (m_notifs.size() > 50)
        m_notifs.removeFirst();
    updateNotifBadge();
    saveNotifs();
    showToast(titre, texte);
}

QString ParticipantsController::notifFilePath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + "/notifications.json";
}

void ParticipantsController::saveNotifs() const
{
    QJsonArray arr;
    for (const Notification &n : m_notifs) {
        QJsonObject o;
        o["titre"] = n.titre;
        o["texte"] = n.texte;
        o["date"]  = n.date.toString(Qt::ISODate);
        o["lue"]   = n.lue;
        o["pid"]   = n.participantId;
        arr.append(o);
    }
    QSaveFile f(notifFilePath());
    if (!f.open(QIODevice::WriteOnly))
        return;
    f.write(QJsonDocument(arr).toJson(QJsonDocument::Compact));
    f.commit();
}

void ParticipantsController::loadNotifs()
{
    QFile f(notifFilePath());
    if (!f.open(QIODevice::ReadOnly))
        return;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isArray())
        return;
    m_notifs.clear();
    for (const QJsonValue &v : doc.array()) {
        const QJsonObject o = v.toObject();
        Notification n;
        n.titre = o["titre"].toString();
        n.texte = o["texte"].toString();
        n.date  = QDateTime::fromString(o["date"].toString(), Qt::ISODate);
        n.lue   = o["lue"].toBool();
        n.participantId = o["pid"].toInt(-1);
        m_notifs.append(n);
    }
    updateNotifBadge();
}

// Bulle qui apparaît sous la cloche quand une notification arrive (cliquable)
void ParticipantsController::showToast(const QString &titre, const QString &texte)
{
    if (!m_toast) {
        m_toast = new QPushButton(m_host);
        m_toast->setStyleSheet(kToastStyle);
        m_toast->setObjectName("toast");
        m_toast->setCursor(Qt::PointingHandCursor);
        m_toast->setFixedWidth(380);
        m_toastTimer = new QTimer(this);
        m_toastTimer->setSingleShot(true);
        connect(m_toastTimer, &QTimer::timeout, m_toast, &QWidget::hide);
        connect(m_toast, &QPushButton::clicked, this, [this] {
            m_toast->hide();
            showNotifications();
        });
    }
    QString court = texte;
    court.replace('\n', ' ');
    if (court.size() > 110)
        court = court.left(107) + "...";
    m_toast->setIcon(iconOf("bell", QColor("#19d3ff"), 18));
    m_toast->setText("  " + titre + "\n" + court);
    m_toast->setFixedHeight(64);
    m_toast->move(m_host->width() - m_toast->width() - 20, 74);
    m_toast->show();
    m_toast->raise();
    m_toastTimer->start(6000);
}

void ParticipantsController::updateNotifBadge()
{
    int unread = 0;
    for (const Notification &n : m_notifs)
        if (!n.lue) ++unread;

    m_notifBadge->setVisible(unread > 0);
    m_notifBadge->setText(unread > 9 ? "9+" : QString::number(unread));
    m_notifBadge->raise();
}

void ParticipantsController::showNotifications()
{
    // 2e clic sur la cloche : on referme
    if (m_notifPanel && m_notifPanel->isVisible()) {
        m_notifPanel->hide();
        return;
    }
    if (m_toast)
        m_toast->hide();
    if (m_notifPanel) {
        m_notifPanel->hide();
        m_notifPanel->deleteLater();
        m_notifPanel = nullptr;
    }

    // Panneau = simple widget enfant de la fenêtre (aucune fenêtre système, donc fiable)
    auto *panel = new QFrame(m_host);
    panel->setStyleSheet(kNotifStyle);
    panel->setObjectName("notifPanel");
    panel->setFixedWidth(380);
    m_notifPanel = panel;

    auto *lay = new QVBoxLayout(panel);
    lay->setContentsMargins(14, 12, 14, 12);
    lay->setSpacing(8);

    // En-tête
    auto *head = new QHBoxLayout;
    auto *title = new QLabel("Notifications");
    title->setObjectName("notifHeader");
    head->addWidget(title);
    head->addStretch();
    auto *clear = new QPushButton("Tout effacer");
    clear->setObjectName("notifClear");
    clear->setCursor(Qt::PointingHandCursor);
    clear->setVisible(!m_notifs.isEmpty());
    connect(clear, &QPushButton::clicked, this, [this] {
        m_notifs.clear();
        updateNotifBadge();
        saveNotifs();
        if (m_notifPanel) m_notifPanel->hide();
    });
    head->addWidget(clear);
    lay->addLayout(head);

    // Information permanente : toujours affichée, jamais effacée par « Tout effacer »
    auto *info = new QFrame;
    info->setObjectName("notifInfo");
    auto *il = new QVBoxLayout(info);
    il->setContentsMargins(12, 10, 12, 10);
    auto *infoTxt = new QLabel("ⓘ  Le participant recevra un SMS de confirmation avec ses identifiants "
                               "et les informations de l'événement.");
    infoTxt->setObjectName("notifInfoText");
    infoTxt->setWordWrap(true);
    il->addWidget(infoTxt);
    lay->addWidget(info);

    if (m_notifs.isEmpty()) {
        auto *empty = new QLabel("Aucune notification");
        empty->setObjectName("notifEmpty");
        empty->setAlignment(Qt::AlignCenter);
        lay->addWidget(empty);
    } else {
        int shown = 0;
        for (int i = m_notifs.size() - 1; i >= 0 && shown < 6; --i, ++shown) {   // 6 plus récentes
            const Notification &n = m_notifs[i];
            auto *item = new QFrame;
            item->setObjectName("notifItem");
            item->setProperty("unread", !n.lue);
            auto *vl = new QVBoxLayout(item);
            vl->setContentsMargins(12, 10, 12, 10);
            vl->setSpacing(4);

            auto *row = new QHBoxLayout;
            auto *t = new QLabel(n.titre);
            t->setObjectName("notifItemTitle");
            auto *tm = new QLabel(n.date.toString("HH:mm"));
            tm->setObjectName("notifItemTime");
            row->addWidget(t);
            row->addStretch();
            row->addWidget(tm);
            vl->addLayout(row);

            auto *txt = new QLabel(n.texte);
            txt->setObjectName("notifItemText");
            txt->setWordWrap(true);
            vl->addWidget(txt);

            for (QLabel *l : { t, tm, txt })
                l->setAttribute(Qt::WA_TransparentForMouseEvents);

            if (n.participantId > 0) {                       // clic -> fiche du participant
                item->setProperty("pid", n.participantId);
                item->setCursor(Qt::PointingHandCursor);
                item->setToolTip("Cliquer pour voir le participant");
                item->installEventFilter(this);
            }
            lay->addWidget(item);
        }
        if (m_notifs.size() > shown) {
            auto *more = new QLabel(QString("+ %1 autre(s) notification(s) plus ancienne(s)")
                                        .arg(m_notifs.size() - shown));
            more->setObjectName("notifItemTime");
            more->setAlignment(Qt::AlignCenter);
            lay->addWidget(more);
        }
    }

    // Ouverture = notifications lues -> la pastille disparaît
    for (Notification &n : m_notifs)
        n.lue = true;
    updateNotifBadge();
    saveNotifs();

    // Placé sous la cloche, bord droit aligné
    panel->adjustSize();
    const QPoint p = ui->btnBell->mapTo(m_host, QPoint(ui->btnBell->width() - panel->width(),
                                                      ui->btnBell->height() + 6));
    panel->move(qMax(8, p.x()), p.y());
    panel->show();
    panel->raise();
}
bool ParticipantsController::eventFilter(QObject *obj, QEvent *ev)
{
    if (obj == ui->pdfDropZone) {
        if (ev->type() == QEvent::MouseButtonRelease &&
            static_cast<QMouseEvent *>(ev)->button() == Qt::LeftButton) {
            const QString path = QFileDialog::getOpenFileName(m_host, "Choisir un fichier PDF", QString(),
                                                               "Fichiers PDF (*.pdf)");
            if (!path.isEmpty()) setPdfAttachment(path);
            return true;
        }
        if (ev->type() == QEvent::DragEnter) {
            auto *drag = static_cast<QDragEnterEvent *>(ev);
            if (drag->mimeData()->hasUrls()) drag->acceptProposedAction();
            return true;
        }
        if (ev->type() == QEvent::Drop) {
            auto *drop = static_cast<QDropEvent *>(ev);
            const QList<QUrl> urls = drop->mimeData()->urls();
            if (!urls.isEmpty() && urls.first().isLocalFile()) setPdfAttachment(urls.first().toLocalFile());
            drop->acceptProposedAction();
            return true;
        }
    }
    // Clic sur une notification liée à un participant -> ouvre sa fiche
    if (ev->type() == QEvent::MouseButtonRelease && obj->property("pid").isValid()) {
        const int pid = obj->property("pid").toInt();
        if (m_notifPanel)
            m_notifPanel->hide();
        QMetaObject::invokeMethod(this, [this, pid] { onView(pid); }, Qt::QueuedConnection);
        return true;
    }

    // Clic n'importe où en dehors du panneau (et de la cloche) -> referme le panneau
    if (ev->type() == QEvent::MouseButtonPress && m_notifPanel && m_notifPanel->isVisible()) {
        if (auto *w = qobject_cast<QWidget *>(obj)) {
            const bool inPanel = (w == m_notifPanel) || m_notifPanel->isAncestorOf(w);
            const bool inBell  = (w == ui->btnBell) || ui->btnBell->isAncestorOf(w);
            if (!inPanel && !inBell && w->window() == m_host->window())
                m_notifPanel->hide();
        }
    }
    return QObject::eventFilter(obj, ev);
}

bool ParticipantsController::setPdfAttachment(const QString &path, bool detectTeam)
{
    const QFileInfo info(path);
    if (!info.exists() || !info.isFile() || info.suffix().compare("pdf", Qt::CaseInsensitive) != 0) {
        showMsg(QMessageBox::Warning, "Fichier invalide", "Veuillez sélectionner un fichier PDF.");
        return false;
    }
    if (info.size() > 10 * 1024 * 1024) {
        showMsg(QMessageBox::Warning, "Fichier trop volumineux", "La taille maximale autorisée est de 10 Mo.");
        return false;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || !file.read(1024).contains("%PDF-")) {
        showMsg(QMessageBox::Warning, "Fichier invalide", "Le fichier sélectionné n'est pas un PDF valide.");
        return false;
    }
    m_pdfPath = info.absoluteFilePath();
    ui->pdfIcon->setText("✓");
    ui->pdfPrompt->setText(info.fileName());
    ui->pdfHint->setText("PDF prêt · Cliquez pour changer de fichier");
    if (detectTeam)
        detectTeamFromPdf(m_pdfPath);           // le CV vient d'être déposé -> choisit l'équipe automatiquement
    return true;
}

// Lit le CV (PDF), devine l'équipe et la sélectionne dans la liste « Équipe(s) ».
void ParticipantsController::detectTeamFromPdf(const QString &path)
{
    const QString text = extractPdfText(path);
    if (text.trimmed().isEmpty()) {
        ui->pdfHint->setText("Aucun texte lisible dans ce PDF · choisissez l'équipe");
        return;
    }
    const QString team = guessTeamFromText(text);
    const int idx = team.isEmpty() ? -1 : ui->comboEquipe->findText(team);
    if (idx < 0) {
        ui->pdfHint->setText("Équipe non détectée dans le CV · choisissez-la");
        return;
    }
    ui->comboEquipe->setCurrentIndex(idx);
    ui->pdfHint->setText(QString("Équipe détectée : %1 · Cliquez pour changer de fichier").arg(team));
    showToast("Équipe détectée",
              QString("L'équipe « %1 » a été sélectionnée automatiquement d'après le CV.").arg(team));
}

void ParticipantsController::clearPdfAttachment()
{
    m_pdfPath.clear();
    ui->pdfIcon->setText("⇧");
    ui->pdfPrompt->setText("Déposez votre fichier PDF ici");
    ui->pdfHint->setText("ou cliquez pour parcourir · PDF uniquement · Taille max. 10 Mo");
}

void ParticipantsController::setupCombos()
{
    ui->comboRolePart->addItem("Sélectionner un rôle");
    ui->comboRolePart->addItems(kRoles);

    ui->comboEquipe->addItem("Sélectionner une équipe");
    ui->comboEquipe->addItems(kEquipes);

    ui->comboDispoPart->addItems(kDispos);

    ui->comboFiltreRole->addItem("Tous les rôles");
    ui->comboFiltreRole->addItems(kRoles);

    ui->comboFiltreEquipe->addItem("Toutes les équipes");
    ui->comboFiltreEquipe->addItems(kEquipes);

    ui->comboFiltreDispo->addItem("Disponibilité");
    ui->comboFiltreDispo->addItems(kDispos);
}

void ParticipantsController::setupTable()
{
    QTableWidget *t = ui->participantsTable;
    t->setColumnCount(9);
    t->setHorizontalHeaderLabels({ "☐", "ID", "Nom", "Prénom", "Email", "Rôle",
                                   "Équipe(s)", "Disponibilité", "Actions" });
    t->verticalHeader()->setVisible(false);
    t->verticalHeader()->setDefaultSectionSize(52);
    t->setShowGrid(false);
    t->setSelectionMode(QAbstractItemView::NoSelection);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setFocusPolicy(Qt::NoFocus);
    t->setIconSize(QSize(34, 34));
    t->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    QHeaderView *h = t->horizontalHeader();
    h->setHighlightSections(false);
    h->setSectionsClickable(true);
    h->setSectionResizeMode(QHeaderView::Stretch);
    h->setSectionResizeMode(0, QHeaderView::Fixed);  // Sélection
    h->setSectionResizeMode(1, QHeaderView::Fixed);  // ID
    h->resizeSection(0, 38);
    h->resizeSection(1, 48);
}

void ParticipantsController::loadSampleData()
{
    auto add = [this](const QString &nom, const QString &prenom, const QString &email,
                      const QString &role, const QString &equipe, const QString &dispo) {
        Participant p;
        p.id = m_nextId++;
        p.nom = nom; p.prenom = prenom; p.email = email;
        p.telephone = QString("+216 20 000 0%1").arg(p.id, 2, 10, QChar('0'));
        p.role = role; p.equipe = equipe; p.dispo = dispo;
        m_data.append(p);
    };
    add("Ben Ali",  "Ahmed",   "ahmed.benali@email.com",   "Organisateur", "Pédagogie/Mentorat", "Disponible");
    add("Trabelsi", "Sara",    "sara.trabelsi@email.com",  "Participant",  "GreenTeam",          "Disponible");
    add("Khaled",   "Omar",    "omar.khaled@email.com",    "Participant",  "MedTech",            "En cours");
    add("Ayari",    "Meriem",  "meriem.ayari@email.com",   "Participant",  "EcoSolutions",       "Disponible");
    add("Jabri",    "Yassine", "yassine.jabri@email.com",  "Participant",  "SmartCity",          "En attente");
    add("Zribi",    "Ines",    "ines.zribi@email.com",     "Participant",  "Clean Energy",       "Disponible");
    add("Gharbi",   "Amine",   "amine.gharbi@email.com",   "Participant",  "UrbanAI",            "En cours");
    add("Mansouri", "Leila",   "leila.mansouri@email.com", "Participant",  "GreenTeam",          "Disponible");
    add("Sassi",    "Laila",   "laila.sassi@email.com",    "Participant",  "HealthCare",         "En attente");
    add("Slimani",  "Rami",    "rami.slimani@email.com",   "Participant",  "TechVision",         "Disponible");
}

// ---------------------------------------------------------------------------
//  SMS de confirmation
//  NB : l'application n'est reliée à aucun opérateur / API SMS. Cette fonction construit
//  le message ; pour un envoi réel, appelez ici votre service (Twilio, Infobip, passerelle
//  de l'opérateur, etc.) avec p.telephone et le texte ci-dessous.
// ---------------------------------------------------------------------------
void ParticipantsController::sendConfirmationSms(const Participant &p)
{
    const QString texte = QString(
        "HackFlow : Bonjour %1, votre inscription est confirmee. "
        "Identifiant : %2. Equipe : %3. Les informations de l'evenement vous seront communiquees.")
        .arg(p.prenom).arg(p.id).arg(p.equipe);
    qInfo("SMS -> %s : %s", qPrintable(p.telephone), qPrintable(texte));
}

// ---------------------------------------------------------------------------
//  Sauvegarde / chargement (fichier JSON dans le dossier de données de l'application)
// ---------------------------------------------------------------------------
QString ParticipantsController::dataFilePath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + "/participants.json";
}

void ParticipantsController::saveData() const
{
    QJsonArray arr;
    for (const Participant &p : m_data) {
        QJsonObject o;
        o["id"] = p.id;       o["nom"] = p.nom;       o["prenom"] = p.prenom;
        o["email"] = p.email; o["telephone"] = p.telephone; o["role"] = p.role;     o["equipe"] = p.equipe;
        o["dispo"] = p.dispo;
        o["pieceJointe"] = p.pieceJointe;
        arr.append(o);
    }
    QJsonObject root;
    root["nextId"] = m_nextId;
    root["participants"] = arr;

    QSaveFile f(dataFilePath());                       // écriture atomique
    if (!f.open(QIODevice::WriteOnly)) {
        qWarning("Impossible d'enregistrer %s", qPrintable(dataFilePath()));
        return;
    }
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    f.commit();
}

bool ParticipantsController::loadData()
{
    QFile f(dataFilePath());
    if (!f.open(QIODevice::ReadOnly))
        return false;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject())
        return false;

    m_data.clear();
    int maxId = 0;
    const QJsonArray arr = doc.object().value("participants").toArray();
    for (const QJsonValue &v : arr) {
        const QJsonObject o = v.toObject();
        Participant p;
        p.id     = o["id"].toInt();
        p.nom    = o["nom"].toString();
        p.prenom = o["prenom"].toString();
        p.email  = o["email"].toString();
        p.telephone = o["telephone"].toString();
        p.role   = o["role"].toString();
        p.equipe = o["equipe"].toString();
        p.dispo  = o["dispo"].toString();
        p.pieceJointe = o["pieceJointe"].toString();
        m_data.append(p);
        maxId = std::max(maxId, p.id);
    }
    m_nextId = std::max(doc.object().value("nextId").toInt(1), maxId + 1);
    return true;
}

// ---------------------------------------------------------------------------
//  Données / filtres
// ---------------------------------------------------------------------------
QList<Participant> ParticipantsController::filteredData() const
{
    const QString q = ui->lineTopSearch->text().trimmed();
    const int ri = ui->comboFiltreRole->currentIndex();
    const int ei = ui->comboFiltreEquipe->currentIndex();
    const int di = ui->comboFiltreDispo->currentIndex();

    QList<Participant> out;
    for (const Participant &p : m_data) {
        if (!q.isEmpty() &&
            !p.nom.contains(q, Qt::CaseInsensitive) &&
            !p.prenom.contains(q, Qt::CaseInsensitive) &&
            !p.email.contains(q, Qt::CaseInsensitive))
            continue;
        if (ri > 0 && p.role   != ui->comboFiltreRole->currentText())   continue;
        if (ei > 0 && p.equipe != ui->comboFiltreEquipe->currentText()) continue;
        if (di > 0 && p.dispo  != ui->comboFiltreDispo->currentText())  continue;
        out.append(p);
    }
    return out;
}

int ParticipantsController::indexOfId(int id) const
{
    for (int i = 0; i < m_data.size(); ++i)
        if (m_data[i].id == id) return i;
    return -1;
}

void ParticipantsController::refreshTable()
{
    QList<Participant> list = filteredData();

    std::sort(list.begin(), list.end(), [this](const Participant &x, const Participant &y) {
        int c = 0;
        switch (m_sortKey) {
        case SortKey::Id:     c = x.id - y.id; break;
        case SortKey::Nom:
            c = QString::compare(x.nom, y.nom, Qt::CaseInsensitive);
            if (c == 0) c = QString::compare(x.prenom, y.prenom, Qt::CaseInsensitive);
            break;
        case SortKey::Role:   c = kRoles.indexOf(x.role)  - kRoles.indexOf(y.role);  break;
        case SortKey::Statut: c = kDispos.indexOf(x.dispo) - kDispos.indexOf(y.dispo); break;
        case SortKey::None:   break;
        }
        if (c == 0) return x.id < y.id;          // départage stable par ID
        return m_sortAsc ? c < 0 : c > 0;
    });

    const int total = list.size();
    const int pages = std::max(1, (total + m_pageSize - 1) / m_pageSize);
    m_page = std::clamp(m_page, 1, pages);
    const int from = (m_page - 1) * m_pageSize;
    const int to   = std::min(total, from + m_pageSize);

    QTableWidget *t = ui->participantsTable;
    t->setRowCount(0);
    t->setRowCount(to - from);

    for (int r = 0; r < to - from; ++r) {
        const Participant &p = list[from + r];

        t->setCellWidget(r, 0, makeCheckCell());

        auto *idItem = new QTableWidgetItem(QString::number(p.id));
        idItem->setTextAlignment(Qt::AlignCenter);
        t->setItem(r, 1, idItem);

        auto *nomItem = new QTableWidgetItem(p.nom);
        nomItem->setIcon(QIcon(makeAvatar(QString(p.prenom.left(1)) + p.nom.left(1), p.id)));
        t->setItem(r, 2, nomItem);

        t->setItem(r, 3, new QTableWidgetItem(p.prenom));
        t->setItem(r, 4, new QTableWidgetItem(p.email));
        t->setCellWidget(r, 5, makeBadge(p.role, p.role.toLower()));
        t->setItem(r, 6, new QTableWidgetItem(p.equipe));
        t->setCellWidget(r, 7, makeBadge(p.dispo, p.dispo.toLower()));
        t->setCellWidget(r, 8, makeActionsCell(p.id));
        t->setRowHeight(r, 52);
    }

    ui->countLabel->setText(QString("Affichage de %1 sur %2 participants").arg(to - from).arg(total));
    ui->btnPage->setText(QString::number(m_page));
    ui->btnPrev->setEnabled(m_page > 1);
    ui->btnNext->setEnabled(m_page < pages);
}

// ---------------------------------------------------------------------------
//  Widgets de cellule
// ---------------------------------------------------------------------------
QWidget *ParticipantsController::makeBadge(const QString &text, const QString &kind)
{
    auto *w   = new QWidget;
    auto *lay = new QHBoxLayout(w);
    lay->setContentsMargins(4, 0, 4, 0);
    auto *lbl = new QLabel(text);
    lbl->setProperty("badge", kind);
    lbl->setAlignment(Qt::AlignCenter);
    lay->addWidget(lbl);
    lay->addStretch();
    return w;
}

QWidget *ParticipantsController::makeCheckCell()
{
    auto *w   = new QWidget;
    auto *lay = new QHBoxLayout(w);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setAlignment(Qt::AlignCenter);
    lay->addWidget(new QCheckBox);
    return w;
}

QWidget *ParticipantsController::makeActionsCell(int id)
{
    auto *w   = new QWidget;
    auto *lay = new QHBoxLayout(w);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(6);

    auto mk = [&](const QString &iconName, const QColor &color, const QString &act, const QString &tip) {
        auto *b = new QToolButton;
        b->setIcon(iconOf(iconName, color, 20));
        b->setIconSize(QSize(20, 20));
        b->setToolTip(tip);
        b->setProperty("act", act);
        b->setCursor(Qt::ArrowCursor);
        b->setEnabled(false); // Désactivé (CRUD désactivé pour Participant)
        lay->addWidget(b);
        return b;
    };
    mk("eye",     QColor("#19d3ff"), "view",   "Voir (désactivé)");
    mk("pencil",  QColor("#4aa8ff"), "edit",   "Modifier (désactivé)");
    mk("trash-2", QColor("#ff4d6a"), "delete", "Supprimer (désactivé)");
    return w;
}

QPixmap ParticipantsController::makeAvatar(const QString &initials, int colorIndex, int size) const
{
    static const QColor palette[] = { QColor("#5b4bff"), QColor("#e0689a"), QColor("#2fb6a8"),
                                      QColor("#f08a3c"), QColor("#3b82f6"), QColor("#9b6bff") };
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(palette[qAbs(colorIndex) % 6]);
    p.setPen(QPen(QColor("#8fb4ff"), 1.5));
    p.drawEllipse(1, 1, size - 2, size - 2);
    p.setPen(Qt::white);
    QFont f = m_host->font();
    f.setBold(true);
    f.setPixelSize(size / 3);
    p.setFont(f);
    p.drawText(pm.rect(), Qt::AlignCenter, initials.toUpper());
    return pm;
}

// ---------------------------------------------------------------------------
//  Slots
// ---------------------------------------------------------------------------
void ParticipantsController::onAjouter()
{
    // Fonctionnement du bouton désactivé (CRUD désactivé pour Participant)
    return;
    const QString nom    = ui->editNom->text().trimmed();
    const QString prenom = ui->editPrenom->text().trimmed();
    const QString email  = ui->editEmail->text().trimmed();
    const QString tel    = ui->editTel->text().trimmed();

    if (nom.isEmpty() || prenom.isEmpty() || email.isEmpty() || tel.isEmpty() ||
        ui->comboRolePart->currentIndex() == 0 || ui->comboEquipe->currentIndex() == 0) {
        showMsg(QMessageBox::Warning, "Champs requis", "Veuillez remplir tous les champs obligatoires (*).");
        return;
    }
    static const QRegularExpression re(R"(^[\w.+-]+@[\w-]+\.[\w.-]+$)");
    if (!re.match(email).hasMatch()) {
        showMsg(QMessageBox::Warning, "Email invalide", "Veuillez saisir une adresse email valide.");
        return;
    }

    // Numéro : +216 12 345 678, 12345678, 0021612345678... (8 à 15 chiffres, espaces tolérés)
    static const QRegularExpression reTel(R"(^\+?[0-9][0-9 ]{6,18}[0-9]$)");
    QString digits = tel;
    digits.remove(' ');
    if (!reTel.match(tel).hasMatch() || digits.remove('+').size() < 8 || digits.size() > 15) {
        showMsg(QMessageBox::Warning, "Téléphone invalide",
                             "Veuillez saisir un numéro de téléphone valide (ex. +216 20 123 456).");
        return;
    }

    Participant saved;
    const int editedId = m_editingId;
    if (m_editingId >= 0) {                              // mode modification
        int i = indexOfId(m_editingId);
        if (i >= 0) {
            m_data[i].nom = nom;       m_data[i].prenom = prenom;
            m_data[i].email = email;   m_data[i].telephone = tel;
            m_data[i].role = ui->comboRolePart->currentText();
            m_data[i].equipe = ui->comboEquipe->currentText();
            m_data[i].dispo  = ui->comboDispoPart->currentText();
            m_data[i].pieceJointe = m_pdfPath;
        }
    } else {
        Participant p;
        p.id = m_nextId++;
        p.nom = nom; p.prenom = prenom; p.email = email;
        p.role   = ui->comboRolePart->currentText();
        p.equipe = ui->comboEquipe->currentText();
        p.dispo  = ui->comboDispoPart->currentText();
        p.telephone = tel;
        p.pieceJointe = m_pdfPath;
        m_data.append(p);
        saved = p;
    }
    saveData();
    onVider();
    refreshTable();

    if (saved.id > 0) {                                  // nouveau participant -> SMS de confirmation
        sendConfirmationSms(saved);
        addNotification("SMS de confirmation",
            QString("%1 %2 (ID %3) recevra un SMS de confirmation avec ses identifiants et les "
                    "informations de l'événement.\nNuméro : %4")
                .arg(saved.prenom, saved.nom).arg(saved.id).arg(saved.telephone),
            saved.id);
    } else {
        addNotification("Participant modifié",
            QString("%1 %2 a été mis à jour.").arg(prenom, nom), editedId);
    }
}

void ParticipantsController::onVider()
{
    m_editingId = -1;
    ui->editId->clear();
    ui->editNom->clear();
    ui->editPrenom->clear();
    ui->editEmail->clear();
    ui->editTel->clear();
    ui->comboRolePart->setCurrentIndex(0);
    ui->comboEquipe->setCurrentIndex(0);
    ui->comboDispoPart->setCurrentIndex(0);
    clearPdfAttachment();
    ui->btnAjouterPart->setIcon(iconOf("plus", Qt::white, 18));
    ui->btnAjouterPart->setText("  Ajouter");
    ui->formTitle->setText(formTitleHtml(false));
}

void ParticipantsController::onFiltersChanged()
{
    m_page = 1;
    refreshTable();
}

// ---------------------------------------------------------------------------
//  Tri : un seul bouton « Trier » qui ouvre le menu des critères
// ---------------------------------------------------------------------------
void ParticipantsController::setupSortMenu()
{
    m_sortMenu = new QMenu(m_host);
    m_sortMenu->setStyleSheet(kMenuStyle);
    m_sortMenu->addSection("Trier par");

    auto *keys = new QActionGroup(this);
    keys->setExclusive(true);
    const QList<QPair<QString, SortKey>> items = {
        { "ID", SortKey::Id }, { "Nom", SortKey::Nom },
        { "Rôle", SortKey::Role }, { "Statut (disponibilité)", SortKey::Statut } };
    for (const auto &it : items) {
        QAction *a = m_sortMenu->addAction(it.first);
        a->setCheckable(true);
        keys->addAction(a);
        m_sortKeyActions.append(a);
        const SortKey k = it.second;
        connect(a, &QAction::triggered, this, [this, k] {
            m_sortKey = k;
            updateSortUi();
            refreshTable();
        });
    }

    m_sortMenu->addSeparator();
    auto *order = new QActionGroup(this);
    order->setExclusive(true);
    m_actAsc  = m_sortMenu->addAction("Ordre croissant  (A → Z, 1 → 9)");
    m_actDesc = m_sortMenu->addAction("Ordre décroissant  (Z → A, 9 → 1)");
    for (QAction *a : { m_actAsc, m_actDesc }) { a->setCheckable(true); order->addAction(a); }
    connect(m_actAsc,  &QAction::triggered, this, [this] { m_sortAsc = true;  updateSortUi(); refreshTable(); });
    connect(m_actDesc, &QAction::triggered, this, [this] { m_sortAsc = false; updateSortUi(); refreshTable(); });

    m_sortMenu->addSeparator();
    QAction *reset = m_sortMenu->addAction("Réinitialiser le tri");
    connect(reset, &QAction::triggered, this, [this] {
        m_sortKey = SortKey::None;
        m_sortAsc = true;
        updateSortUi();
        refreshTable();
    });

    updateSortUi();
}

void ParticipantsController::updateSortUi()
{
    static const QStringList names = { "ID", "Nom", "Rôle", "Statut" };
    for (int i = 0; i < m_sortKeyActions.size(); ++i)
        m_sortKeyActions[i]->setChecked(m_sortKey != SortKey::None && int(m_sortKey) - 1 == i);
    m_actAsc->setChecked(m_sortAsc);
    m_actDesc->setChecked(!m_sortAsc);

    if (m_sortKey == SortKey::None)
        ui->btnTrierPart->setText("  Trier");
    else
        ui->btnTrierPart->setText(QString("  %1 %2").arg(names[int(m_sortKey) - 1], m_sortAsc ? "▲" : "▼"));
}

void ParticipantsController::onSortClicked()
{
    m_sortMenu->exec(ui->btnTrierPart->mapToGlobal(QPoint(0, ui->btnTrierPart->height() + 4)));
}

void ParticipantsController::onHeaderClicked(int section)
{
    if (section == 0) {     // case "tout sélectionner"
        QTableWidget *t = ui->participantsTable;
        bool anyUnchecked = false;
        for (int r = 0; r < t->rowCount(); ++r)
            if (auto *cb = t->cellWidget(r, 0)->findChild<QCheckBox *>())
                if (!cb->isChecked()) { anyUnchecked = true; break; }
        for (int r = 0; r < t->rowCount(); ++r)
            if (auto *cb = t->cellWidget(r, 0)->findChild<QCheckBox *>())
                cb->setChecked(anyUnchecked);
    } else {
        SortKey k = SortKey::None;
        switch (section) {
        case 1: k = SortKey::Id;     break;
        case 2: k = SortKey::Nom;    break;
        case 5: k = SortKey::Role;   break;
        case 7: k = SortKey::Statut; break;
        default: return;
        }
        if (m_sortKey == k) m_sortAsc = !m_sortAsc;   // 2e clic : inverse l'ordre
        else { m_sortKey = k; m_sortAsc = true; }
        updateSortUi();
        refreshTable();
    }
}

void ParticipantsController::onPrevPage() { --m_page; refreshTable(); }
void ParticipantsController::onNextPage() { ++m_page; refreshTable(); }

void ParticipantsController::onExporter()
{
    const QString path = QFileDialog::getSaveFileName(m_host, "Exporter les participants",
                                                      "participants.csv", "Fichiers CSV (*.csv)");
    if (path.isEmpty()) return;

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        showMsg(QMessageBox::Critical, "Erreur", "Impossible d'écrire le fichier.");
        return;
    }
    QTextStream out(&f);
    out.setEncoding(QStringConverter::Utf8);
    out << "ID;Nom;Prénom;Email;Téléphone;Rôle;Équipe;Disponibilité\n";
    for (const Participant &p : filteredData())
        out << p.id << ';' << p.nom << ';' << p.prenom << ';' << p.email << ';' << p.telephone << ';'
            << p.role << ';' << p.equipe << ';' << p.dispo << '\n';
    showMsg(QMessageBox::Information, "Export", "Export terminé avec succès.");
}

void ParticipantsController::onView(int id)
{
    Q_UNUSED(id);
    // Fonctionnement du bouton désactivé (CRUD désactivé pour Participant)
    return;
    int i = indexOfId(id);
    if (i < 0) return;
    const Participant &p = m_data[i];
    showMsg(QMessageBox::Information, "Détails du participant",
        QString("ID : %1\nNom : %2\nPrénom : %3\nEmail : %4\nTéléphone : %5\nRôle : %6\nÉquipe : %7\nDisponibilité : %8")
            .arg(p.id).arg(p.nom, p.prenom, p.email, p.telephone, p.role, p.equipe, p.dispo));
}

void ParticipantsController::onEdit(int id)
{
    Q_UNUSED(id);
    // Fonctionnement du bouton désactivé (CRUD désactivé pour Participant)
    return;
    int i = indexOfId(id);
    if (i < 0) return;
    const Participant &p = m_data[i];
    m_editingId = id;
    ui->editId->setText(QString::number(p.id));
    ui->editNom->setText(p.nom);
    ui->editPrenom->setText(p.prenom);
    ui->editEmail->setText(p.email);
    ui->editTel->setText(p.telephone);
    ui->comboRolePart->setCurrentText(p.role);
    ui->comboEquipe->setCurrentText(p.equipe);
    ui->comboDispoPart->setCurrentText(p.dispo);
    if (p.pieceJointe.isEmpty() || !QFileInfo::exists(p.pieceJointe)) clearPdfAttachment();
    else setPdfAttachment(p.pieceJointe, false);   // modification : on garde l'équipe enregistrée
    ui->btnAjouterPart->setIcon(iconOf("pencil", Qt::white, 18));
    ui->btnAjouterPart->setText("  Mettre à jour");
    ui->formTitle->setText(formTitleHtml(true));
}

void ParticipantsController::onDelete(int id)
{
    Q_UNUSED(id);
    // Fonctionnement du bouton désactivé (CRUD désactivé pour Participant)
    return;
    int i = indexOfId(id);
    if (i < 0) return;
    if (showMsg(QMessageBox::Question, "Supprimer",
            QString("Supprimer %1 %2 ?").arg(m_data[i].prenom, m_data[i].nom)) != QMessageBox::Yes)
        return;
    const QString who = m_data[i].prenom + " " + m_data[i].nom;
    m_data.removeAt(i);
    saveData();
    addNotification("Participant supprimé", QString("%1 a été supprimé de la liste.").arg(who));
    refreshTable();
}

