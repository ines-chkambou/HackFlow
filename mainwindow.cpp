#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QApplication>
#include <QCheckBox>
#include <QDateTime>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QEvent>
#include <QMenu>
#include <QTimer>
#include <QMouseEvent>
#include <QWidgetAction>
#include <QFrame>
#include <QPushButton>
#include <QScrollArea>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QFileDialog>
#include <QFileInfo>
#include <QMimeData>
#include <QUrl>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QAction>
#include <QActionGroup>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QRegularExpression>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QTextStream>
#include <QToolButton>
#include <algorithm>

#ifdef HAVE_QTPDF            // défini par CMakeLists.txt si le module Qt PDF est installé
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
    const QPixmap src(QString(":/icons/%1.png").arg(name));
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
    return QString("<img src=\":/icons/%1.png\" width=\"%2\" height=\"%2\" style=\"vertical-align:middle\">")
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

// Icône du logo (ampoule, PNG encodé en base64) : aucun fichier externe ni .qrc nécessaire
static const char kLogoIconBase64[] =
    "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"
    "gg==";

// ---------------------------------------------------------------------------
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    applyStyleSheet();
    setupCombos();
    setupTable();
    if (!loadData()) {          // premier lancement : données d'exemple, puis sauvegarde
        loadSampleData();
        saveData();
    }

    setupLogo();
    ui->pdfDropZone->setAcceptDrops(true);
    ui->pdfDropZone->setCursor(Qt::PointingHandCursor);
    ui->pdfDropZone->installEventFilter(this);
    for (QLabel *label : ui->pdfDropZone->findChildren<QLabel *>()) {
        label->setAttribute(Qt::WA_TransparentForMouseEvents);
        label->setCursor(Qt::PointingHandCursor);
    }
    setupNavigation();
    setupNotifications();
    loadNotifs();
    setupSortMenu();
    setupIcons();

    // Avatars (en-tête + carte utilisateur)
    ui->userAvatar->setPixmap(makeAvatar("A", 0, 44));
    ui->adminAvatar->setPixmap(makeAvatar("A", 0, 40));

    // Connexions
    connect(ui->btnAjouter, &QPushButton::clicked, this, &MainWindow::onAjouter);
    connect(ui->btnVider,   &QPushButton::clicked, this, &MainWindow::onVider);
    connect(ui->btnTrier,   &QPushButton::clicked, this, &MainWindow::onSortClicked);
    connect(ui->btnExporter,&QPushButton::clicked, this, &MainWindow::onExporter);
    connect(ui->btnPrev,    &QPushButton::clicked, this, &MainWindow::onPrevPage);
    connect(ui->btnNext,    &QPushButton::clicked, this, &MainWindow::onNextPage);

    connect(ui->globalSearch,      &QLineEdit::textChanged, this, &MainWindow::onFiltersChanged);
    connect(ui->comboFiltreRole,   &QComboBox::currentIndexChanged, this, &MainWindow::onFiltersChanged);
    connect(ui->comboFiltreEquipe, &QComboBox::currentIndexChanged, this, &MainWindow::onFiltersChanged);
    connect(ui->comboFiltreDispo,  &QComboBox::currentIndexChanged, this, &MainWindow::onFiltersChanged);

    connect(ui->participantsTable->horizontalHeader(), &QHeaderView::sectionClicked,
            this, &MainWindow::onHeaderClicked);

    refreshTable();
}

MainWindow::~MainWindow()
{
    delete ui;
}

// ---------------------------------------------------------------------------
//  Navigation (barre latérale) : chaque bouton ouvre sa page
// ---------------------------------------------------------------------------
QWidget *MainWindow::makePage(const QString &icon, const QString &title,
                              const QString &accent, const QString &subtitle)
{
    auto *page = new QWidget;
    auto *lay  = new QVBoxLayout(page);
    lay->setContentsMargins(24, 18, 18, 18);
    lay->setSpacing(14);

    auto *head = new QHBoxLayout;
    auto *ico = new QLabel;
    ico->setPixmap(iconPixmap(icon, QColor("#6ea8ff"), 38));
    ico->setObjectName("titleIcon");
    ico->setAlignment(Qt::AlignTop);
    head->addWidget(ico);

    auto *col = new QVBoxLayout;
    col->setSpacing(0);
    auto *t = new QLabel(QString("%1 <span style=\"color:#6ea8ff\">%2</span>").arg(title, accent));
    t->setObjectName("titleLabel");
    auto *st = new QLabel(subtitle);
    st->setObjectName("subtitleLabel");
    col->addWidget(t);
    col->addWidget(st);
    head->addLayout(col);
    head->addStretch();
    lay->addLayout(head);

    auto *card = new QFrame;
    card->setObjectName("placeholderCard");
    auto *cl = new QVBoxLayout(card);
    auto *msg = new QLabel(QString("Page « %1 %2 » : contenu à venir.").arg(title, accent));
    msg->setObjectName("placeholderText");
    msg->setAlignment(Qt::AlignCenter);
    cl->addWidget(msg);
    lay->addWidget(card, 1);
    return page;
}

void MainWindow::setupNavigation()
{
    m_stack = new QStackedWidget(ui->mainArea);
    ui->mainLayout->replaceWidget(ui->pageContent, m_stack);

    // Ordre = ordre des boutons de la barre latérale
    m_stack->addWidget(makePage("house",          "Tableau de", "bord",       "Vue d'ensemble de l'événement HackFlow."));
    m_stack->addWidget(ui->pageContent);                       // Participants (page existante)
    m_stack->addWidget(makePage("network",        "Gestion des", "Équipes",   "Consultez et gérez les équipes de l'événement."));
    m_stack->addWidget(makePage("clipboard-list", "Gestion des", "Projets",   "Consultez et gérez les projets des équipes."));
    m_stack->addWidget(makePage("users-round",    "Gestion des", "Employés",  "Consultez et gérez les employés."));
    m_stack->addWidget(makePage("shield-check",   "Gestion du",  "Jury",      "Consultez et gérez les membres du jury."));
    m_stack->addWidget(makePage("settings",       "",            "Paramètres", "Configurez l'application."));

    const QList<QPushButton *> buttons = { ui->btnAccueil, ui->btnParticipants, ui->btnEquipes,
                                           ui->btnProjets, ui->btnEmployes, ui->btnJury,
                                           ui->btnParametres };
    for (int i = 0; i < buttons.size(); ++i) {
        buttons[i]->setCursor(Qt::PointingHandCursor);
        connect(buttons[i], &QPushButton::clicked, this, [this, i] { m_stack->setCurrentIndex(i); });
    }
    m_stack->setCurrentIndex(1);
    ui->btnParticipants->setChecked(true);

}

// ---------------------------------------------------------------------------
//  Icônes de l'interface (barre latérale, en-tête, formulaire, filtres, pagination)
// ---------------------------------------------------------------------------
void MainWindow::setupIcons()
{
    // Barre latérale
    struct NavIcon { QPushButton *btn; const char *name; };
    const QList<NavIcon> nav = {
        { ui->btnAccueil,      "house" },
        { ui->btnParticipants, "users" },
        { ui->btnEquipes,      "network" },
        { ui->btnProjets,      "clipboard-list" },
        { ui->btnEmployes,     "users-round" },
        { ui->btnJury,         "shield-check" },
        { ui->btnParametres,   "settings" },
    };
    for (const NavIcon &n : nav) {
        n.btn->setIcon(iconOf(n.name, Qt::white, 22));
        n.btn->setIconSize(QSize(22, 22));
    }

    // En-tête : recherche, cloche, titre, fil d'Ariane
    ui->globalSearch->addAction(iconOf("search", QColor("#9fb3e8"), 18), QLineEdit::LeadingPosition);
    ui->btnNotif->setIcon(iconOf("bell", Qt::white, 22));
    ui->btnNotif->setIconSize(QSize(22, 22));
    ui->titleIcon->setPixmap(iconPixmap("users-round", QColor("#6ea8ff"), 38));
    ui->breadcrumb->setText(iconHtml("house", 16) + "&nbsp;&nbsp;" + iconHtml("chevron-right", 14)
                            + "&nbsp;&nbsp;Participants&nbsp;&nbsp;" + iconHtml("chevron-right", 14));

    // Formulaire
    const QColor field("#9fb3e8");
    ui->formTitle->setText(formTitleHtml(false));
    ui->editId->addAction(iconOf("user", field, 16),     QLineEdit::LeadingPosition);
    ui->editNom->addAction(iconOf("user", field, 16),    QLineEdit::LeadingPosition);
    ui->editPrenom->addAction(iconOf("user", field, 16), QLineEdit::LeadingPosition);
    ui->editEmail->addAction(iconOf("mail", field, 16),  QLineEdit::LeadingPosition);
    ui->btnAjouter->setIcon(iconOf("plus", Qt::white, 18));
    ui->btnVider->setIcon(iconOf("rotate-cw", Qt::white, 18));

    // Filtres, export, pagination
    ui->btnTrier->setIcon(iconOf("arrow-up-down", Qt::white, 18));
    ui->btnExporter->setIcon(iconOf("download", Qt::white, 18));
    ui->btnPrev->setIcon(iconOf("chevron-left", Qt::white, 18));
    ui->btnNext->setIcon(iconOf("chevron-right", Qt::white, 18));
    for (QPushButton *b : { ui->btnAjouter, ui->btnVider, ui->btnTrier, ui->btnExporter,
                            ui->btnPrev, ui->btnNext })
        b->setIconSize(QSize(18, 18));
}

// ---------------------------------------------------------------------------
//  Notifications : cloche de l'en-tête (pastille rouge + liste déroulante)
// ---------------------------------------------------------------------------
void MainWindow::setupNotifications()
{
    ui->btnNotif->setCursor(Qt::PointingHandCursor);

    m_notifBadge = new QLabel(ui->btnNotif);
    m_notifBadge->setObjectName("notifBadge");
    m_notifBadge->setAlignment(Qt::AlignCenter);
    m_notifBadge->setFixedSize(18, 18);
    m_notifBadge->move(20, 2);                 // coin supérieur droit de la cloche (40 x 40)
    m_notifBadge->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_notifBadge->hide();

    connect(ui->btnNotif, &QToolButton::clicked, this, &MainWindow::showNotifications);
    qApp->installEventFilter(this);            // pour refermer le panneau au clic à l'extérieur
}

void MainWindow::addNotification(const QString &titre, const QString &texte, int participantId)
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

QString MainWindow::notifFilePath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + "/notifications.json";
}

void MainWindow::saveNotifs() const
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

void MainWindow::loadNotifs()
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
void MainWindow::showToast(const QString &titre, const QString &texte)
{
    if (!m_toast) {
        m_toast = new QPushButton(this);
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
    m_toast->move(width() - m_toast->width() - 20, 74);
    m_toast->show();
    m_toast->raise();
    m_toastTimer->start(6000);
}

void MainWindow::updateNotifBadge()
{
    int unread = 0;
    for (const Notification &n : m_notifs)
        if (!n.lue) ++unread;

    m_notifBadge->setVisible(unread > 0);
    m_notifBadge->setText(unread > 9 ? "9+" : QString::number(unread));
    m_notifBadge->raise();
}

void MainWindow::showNotifications()
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
    auto *panel = new QFrame(this);
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
    const QPoint p = ui->btnNotif->mapTo(this, QPoint(ui->btnNotif->width() - panel->width(),
                                                      ui->btnNotif->height() + 6));
    panel->move(qMax(8, p.x()), p.y());
    panel->show();
    panel->raise();
}

bool MainWindow::eventFilter(QObject *obj, QEvent *ev)
{
    if (obj == ui->pdfDropZone) {
        if (ev->type() == QEvent::MouseButtonRelease &&
            static_cast<QMouseEvent *>(ev)->button() == Qt::LeftButton) {
            const QString path = QFileDialog::getOpenFileName(this, "Choisir un fichier PDF", QString(),
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
            const bool inBell  = (w == ui->btnNotif) || ui->btnNotif->isAncestorOf(w);
            if (!inPanel && !inBell && w->window() == window())
                m_notifPanel->hide();
        }
    }
    return QMainWindow::eventFilter(obj, ev);
}

bool MainWindow::setPdfAttachment(const QString &path, bool detectTeam)
{
    const QFileInfo info(path);
    if (!info.exists() || !info.isFile() || info.suffix().compare("pdf", Qt::CaseInsensitive) != 0) {
        QMessageBox::warning(this, "Fichier invalide", "Veuillez sélectionner un fichier PDF.");
        return false;
    }
    if (info.size() > 10 * 1024 * 1024) {
        QMessageBox::warning(this, "Fichier trop volumineux", "La taille maximale autorisée est de 10 Mo.");
        return false;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || !file.read(1024).contains("%PDF-")) {
        QMessageBox::warning(this, "Fichier invalide", "Le fichier sélectionné n'est pas un PDF valide.");
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
void MainWindow::detectTeamFromPdf(const QString &path)
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

void MainWindow::clearPdfAttachment()
{
    m_pdfPath.clear();
    ui->pdfIcon->setText("⇧");
    ui->pdfPrompt->setText("Déposez votre fichier PDF ici");
    ui->pdfHint->setText("ou cliquez pour parcourir · PDF uniquement · Taille max. 10 Mo");
}

// ---------------------------------------------------------------------------
//  Logo (icône + texte + slogan), image intégrée
// ---------------------------------------------------------------------------
void MainWindow::setupLogo()
{
    QPixmap src;
    src.loadFromData(QByteArray::fromBase64(kLogoIconBase64), "PNG");
    if (src.isNull())
        return;

    // Seule l'icône est une image : tuile arrondie à bordure bleue.
    // Le nom « HackFlow » et le slogan sont de vrais textes (QLabel).
    const int size = 72;
    const qreal dpr = devicePixelRatioF();
    const int px = qRound(size * dpr);
    QPixmap tile(px, px);
    tile.fill(Qt::transparent);
    {
        QPainter p(&tile);
        p.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
        const QRectF r(1.5 * dpr, 1.5 * dpr, px - 3 * dpr, px - 3 * dpr);
        QPainterPath path;
        path.addRoundedRect(r, 16 * dpr, 16 * dpr);
        p.setClipPath(path);
        p.drawPixmap(r.toRect(), src);
        p.setClipping(false);
        p.setPen(QPen(QColor("#2f7bff"), 2.5 * dpr));
        p.setBrush(Qt::NoBrush);
        p.drawPath(path);
    }
    tile.setDevicePixelRatio(dpr);
    ui->logoIcon->setPixmap(tile);

    setWindowIcon(QIcon(src));
}

// ---------------------------------------------------------------------------
//  Style (thème sombre bleu / néon)
// ---------------------------------------------------------------------------
void MainWindow::applyStyleSheet()
{
    setStyleSheet(R"(
QMainWindow, #centralwidget {
    background: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #040a1f, stop:0.5 #071445, stop:1 #040a1f);
}
QWidget { color: #e6ecff; font-family: "Segoe UI", "Inter", sans-serif; font-size: 13px; }
QLabel  { background: transparent; }

/* ---------- Sidebar ---------- */
#sidebar { background: rgba(5,12,38,0.95); border-right: 1px solid #12306e; }
#sidebar QPushButton {
    text-align: left; padding: 11px 16px; border: none; border-radius: 10px;
    font-size: 15px; background: transparent; color: #dbe4ff;
}
#sidebar QPushButton:hover   { background: rgba(91,75,255,0.18); }
#sidebar QPushButton:checked {
    background: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #5b4bff, stop:1 #6d6bff); color: white;
}
#logoTitle   { font-size: 30px; font-weight: 700; color: white; }
#logoTagline { font-size: 9px; font-weight: 700; color: #19d3ff; }
#placeholderCard { background: rgba(6,18,58,0.85); border: 1px solid #1f46a8; border-radius: 14px; }
#placeholderText { color: #9fb3e8; font-size: 16px; }
QMenu { background: #08183f; border: 1px solid #1f46a8; border-radius: 8px; padding: 6px; }
QMenu::item { padding: 8px 28px 8px 26px; border-radius: 6px; color: #e6ecff; }
QMenu::item:selected { background: #5b4bff; color: white; }
QMenu::separator { height: 1px; background: #1f46a8; margin: 6px 8px; }
QMenu::indicator { width: 14px; height: 14px; left: 6px; }
#userCard { background: rgba(10,25,70,0.7); border: 1px solid #1b3a8a; border-radius: 10px; }

/* ---------- Top bar ---------- */
#topBar { background: rgba(5,14,44,0.9); border-bottom: 1px solid #12306e; }
#globalSearch {
    background: #08183f; border: 1px solid #1f46a8; border-radius: 8px; padding: 0 12px; color: #9fb3e8;
}
#btnNotif { border: none; background: transparent; font-size: 20px; }

/* ---------- Titres ---------- */
#titleIcon     { font-size: 34px; }
#titleLabel    { font-size: 32px; font-weight: 700; color: white; }
#subtitleLabel { color: #9fb3e8; font-size: 14px; }
#breadcrumb    { color: #9fb3e8; }

/* ---------- Panneaux ---------- */
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
#formPanel QLabel#lblId, #formPanel QLabel#lblNom, #formPanel QLabel#lblPrenom,
#formPanel QLabel#lblEmail, #formPanel QLabel#lblTel, #formPanel QLabel#lblRole, #formPanel QLabel#lblEquipe,
#formPanel QLabel#lblDispo { color: #dbe4ff; margin-top: 2px; }

QLineEdit, QComboBox {
    background: #08183f; border: 1px solid #1f46a8; border-radius: 8px;
    padding: 0 12px; min-height: 32px; color: #e6ecff; selection-background-color: #5b4bff;
}
QLineEdit:focus, QComboBox:focus { border: 1px solid #19d3ff; }
QLineEdit[readOnly="true"] { color: #8fa3d6; }
QComboBox::drop-down { border: none; width: 28px; }
QComboBox::down-arrow { image: url(:/icons/chevron-down-small.png); width: 14px; height: 14px; }
QComboBox QAbstractItemView {
    background: #08183f; border: 1px solid #1f46a8; selection-background-color: #5b4bff; color: #e6ecff;
}

/* ---------- Boutons ---------- */
#btnAjouter {
    border: none; border-radius: 8px; font-size: 12px; font-weight: 600; color: white;
    background: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #3b6bff, stop:1 #6a4bff);
}
#btnAjouter:hover { background: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #5b82ff, stop:1 #8266ff); }
#btnVider, #btnTrier, #btnExporter, #btnPrev, #btnNext {
    background: #08183f; border: 1px solid #1f46a8; border-radius: 8px; color: #e6ecff;
}
#btnVider:hover, #btnTrier:hover, #btnExporter:hover,
#btnPrev:hover, #btnNext:hover { border-color: #19d3ff; }
#btnPage { background: #5b4bff; border: none; border-radius: 8px; color: white; font-weight: 700; }
#notifBadge { background: #ff4d6a; color: white; border-radius: 9px; font-size: 10px; font-weight: 700; }
#toast { text-align: left; padding: 8px 14px; background: #0b2260; color: #e6ecff;
         border: 1px solid #19d3ff; border-radius: 10px; font-size: 12px; }
#toast:hover { background: #12308a; }
#notifPanel { background: #071445; border: 1px solid #1f46a8; border-radius: 12px; }
#notifPanel QLabel { background: transparent; border: none; }
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

QLabel[badge="organisateur"] { background: #3a2fd8; color: white;   border-radius: 4px; padding: 6px 10px; }
QLabel[badge="participant"]  { background: #2563eb; color: white;   border-radius: 4px; padding: 6px 10px; }
QLabel[badge="disponible"]   { background: #0c6b5c; color: #6dffd0; border-radius: 4px; padding: 6px 10px; }
QLabel[badge="en cours"]     { background: #7a4512; color: #ffb36b; border-radius: 4px; padding: 6px 10px; }
QLabel[badge="en attente"]   { background: #70401a; color: #ffad70; border-radius: 4px; padding: 6px 10px; }

QToolButton[act="view"]   { color: #19d3ff; }
QToolButton[act="edit"]   { color: #4aa8ff; }
QToolButton[act="delete"] { color: #ff4d6a; }
QToolButton[act] { border: none; background: transparent; font-size: 17px; padding: 4px; }
QToolButton[act]:hover { background: rgba(91,75,255,0.25); border-radius: 6px; }

#countLabel { color: #9fb3e8; }

QMessageBox, QDialog { background: #071445; }
QMessageBox QPushButton {
    background: #5b4bff; border: none; border-radius: 6px; padding: 6px 18px; color: white;
}
)");
}

// ---------------------------------------------------------------------------
void MainWindow::setupCombos()
{
    ui->comboRole->addItem("Sélectionner un rôle");
    ui->comboRole->addItems(kRoles);

    ui->comboEquipe->addItem("Sélectionner une équipe");
    ui->comboEquipe->addItems(kEquipes);

    ui->comboDispo->addItems(kDispos);

    ui->comboFiltreRole->addItem("Tous les rôles");
    ui->comboFiltreRole->addItems(kRoles);

    ui->comboFiltreEquipe->addItem("Toutes les équipes");
    ui->comboFiltreEquipe->addItems(kEquipes);

    ui->comboFiltreDispo->addItem("Disponibilité");
    ui->comboFiltreDispo->addItems(kDispos);
}

void MainWindow::setupTable()
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

void MainWindow::loadSampleData()
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
void MainWindow::sendConfirmationSms(const Participant &p)
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
QString MainWindow::dataFilePath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + "/participants.json";
}

void MainWindow::saveData() const
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

bool MainWindow::loadData()
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
QList<Participant> MainWindow::filteredData() const
{
    const QString q = ui->globalSearch->text().trimmed();
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

int MainWindow::indexOfId(int id) const
{
    for (int i = 0; i < m_data.size(); ++i)
        if (m_data[i].id == id) return i;
    return -1;
}

void MainWindow::refreshTable()
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
QWidget *MainWindow::makeBadge(const QString &text, const QString &kind)
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

QWidget *MainWindow::makeCheckCell()
{
    auto *w   = new QWidget;
    auto *lay = new QHBoxLayout(w);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setAlignment(Qt::AlignCenter);
    lay->addWidget(new QCheckBox);
    return w;
}

QWidget *MainWindow::makeActionsCell(int id)
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
        b->setCursor(Qt::PointingHandCursor);
        lay->addWidget(b);
        return b;
    };
    connect(mk("eye",     QColor("#19d3ff"), "view",   "Voir"),       &QToolButton::clicked, this, [this, id] { onView(id); });
    connect(mk("pencil",  QColor("#4aa8ff"), "edit",   "Modifier"),   &QToolButton::clicked, this, [this, id] { onEdit(id); });
    connect(mk("trash-2", QColor("#ff4d6a"), "delete", "Supprimer"),  &QToolButton::clicked, this, [this, id] { onDelete(id); });
    return w;
}

QPixmap MainWindow::makeAvatar(const QString &initials, int colorIndex, int size) const
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
    QFont f = font();
    f.setBold(true);
    f.setPixelSize(size / 3);
    p.setFont(f);
    p.drawText(pm.rect(), Qt::AlignCenter, initials.toUpper());
    return pm;
}

// ---------------------------------------------------------------------------
//  Slots
// ---------------------------------------------------------------------------
void MainWindow::onAjouter()
{
    const QString nom    = ui->editNom->text().trimmed();
    const QString prenom = ui->editPrenom->text().trimmed();
    const QString email  = ui->editEmail->text().trimmed();
    const QString tel    = ui->editTel->text().trimmed();

    if (nom.isEmpty() || prenom.isEmpty() || email.isEmpty() || tel.isEmpty() ||
        ui->comboRole->currentIndex() == 0 || ui->comboEquipe->currentIndex() == 0) {
        QMessageBox::warning(this, "Champs requis", "Veuillez remplir tous les champs obligatoires (*).");
        return;
    }
    static const QRegularExpression re(R"(^[\w.+-]+@[\w-]+\.[\w.-]+$)");
    if (!re.match(email).hasMatch()) {
        QMessageBox::warning(this, "Email invalide", "Veuillez saisir une adresse email valide.");
        return;
    }

    // Numéro : +216 12 345 678, 12345678, 0021612345678... (8 à 15 chiffres, espaces tolérés)
    static const QRegularExpression reTel(R"(^\+?[0-9][0-9 ]{6,18}[0-9]$)");
    QString digits = tel;
    digits.remove(' ');
    if (!reTel.match(tel).hasMatch() || digits.remove('+').size() < 8 || digits.size() > 15) {
        QMessageBox::warning(this, "Téléphone invalide",
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
            m_data[i].role = ui->comboRole->currentText();
            m_data[i].equipe = ui->comboEquipe->currentText();
            m_data[i].dispo  = ui->comboDispo->currentText();
            m_data[i].pieceJointe = m_pdfPath;
        }
    } else {
        Participant p;
        p.id = m_nextId++;
        p.nom = nom; p.prenom = prenom; p.email = email;
        p.role   = ui->comboRole->currentText();
        p.equipe = ui->comboEquipe->currentText();
        p.dispo  = ui->comboDispo->currentText();
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

void MainWindow::onVider()
{
    m_editingId = -1;
    ui->editId->clear();
    ui->editNom->clear();
    ui->editPrenom->clear();
    ui->editEmail->clear();
    ui->editTel->clear();
    ui->comboRole->setCurrentIndex(0);
    ui->comboEquipe->setCurrentIndex(0);
    ui->comboDispo->setCurrentIndex(0);
    clearPdfAttachment();
    ui->btnAjouter->setIcon(iconOf("plus", Qt::white, 18));
    ui->btnAjouter->setText("  Ajouter");
    ui->formTitle->setText(formTitleHtml(false));
}

void MainWindow::onFiltersChanged()
{
    m_page = 1;
    refreshTable();
}

// ---------------------------------------------------------------------------
//  Tri : un seul bouton « Trier » qui ouvre le menu des critères
// ---------------------------------------------------------------------------
void MainWindow::setupSortMenu()
{
    m_sortMenu = new QMenu(this);
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

void MainWindow::updateSortUi()
{
    static const QStringList names = { "ID", "Nom", "Rôle", "Statut" };
    for (int i = 0; i < m_sortKeyActions.size(); ++i)
        m_sortKeyActions[i]->setChecked(m_sortKey != SortKey::None && int(m_sortKey) - 1 == i);
    m_actAsc->setChecked(m_sortAsc);
    m_actDesc->setChecked(!m_sortAsc);

    if (m_sortKey == SortKey::None)
        ui->btnTrier->setText("  Trier");
    else
        ui->btnTrier->setText(QString("  %1 %2").arg(names[int(m_sortKey) - 1], m_sortAsc ? "▲" : "▼"));
}

void MainWindow::onSortClicked()
{
    m_sortMenu->exec(ui->btnTrier->mapToGlobal(QPoint(0, ui->btnTrier->height() + 4)));
}

void MainWindow::onHeaderClicked(int section)
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

void MainWindow::onPrevPage() { --m_page; refreshTable(); }
void MainWindow::onNextPage() { ++m_page; refreshTable(); }

void MainWindow::onExporter()
{
    const QString path = QFileDialog::getSaveFileName(this, "Exporter les participants",
                                                      "participants.csv", "Fichiers CSV (*.csv)");
    if (path.isEmpty()) return;

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Erreur", "Impossible d'écrire le fichier.");
        return;
    }
    QTextStream out(&f);
    out.setEncoding(QStringConverter::Utf8);
    out << "ID;Nom;Prénom;Email;Téléphone;Rôle;Équipe;Disponibilité\n";
    for (const Participant &p : filteredData())
        out << p.id << ';' << p.nom << ';' << p.prenom << ';' << p.email << ';' << p.telephone << ';'
            << p.role << ';' << p.equipe << ';' << p.dispo << '\n';
    QMessageBox::information(this, "Export", "Export terminé avec succès.");
}

void MainWindow::onView(int id)
{
    int i = indexOfId(id);
    if (i < 0) return;
    const Participant &p = m_data[i];
    QMessageBox::information(this, "Détails du participant",
        QString("ID : %1\nNom : %2\nPrénom : %3\nEmail : %4\nTéléphone : %5\nRôle : %6\nÉquipe : %7\nDisponibilité : %8")
            .arg(p.id).arg(p.nom, p.prenom, p.email, p.telephone, p.role, p.equipe, p.dispo));
}

void MainWindow::onEdit(int id)
{
    int i = indexOfId(id);
    if (i < 0) return;
    const Participant &p = m_data[i];
    m_editingId = id;
    ui->editId->setText(QString::number(p.id));
    ui->editNom->setText(p.nom);
    ui->editPrenom->setText(p.prenom);
    ui->editEmail->setText(p.email);
    ui->editTel->setText(p.telephone);
    ui->comboRole->setCurrentText(p.role);
    ui->comboEquipe->setCurrentText(p.equipe);
    ui->comboDispo->setCurrentText(p.dispo);
    if (p.pieceJointe.isEmpty() || !QFileInfo::exists(p.pieceJointe)) clearPdfAttachment();
    else setPdfAttachment(p.pieceJointe, false);   // modification : on garde l'équipe enregistrée
    ui->btnAjouter->setIcon(iconOf("pencil", Qt::white, 18));
    ui->btnAjouter->setText("  Mettre à jour");
    ui->formTitle->setText(formTitleHtml(true));
}

void MainWindow::onDelete(int id)
{
    int i = indexOfId(id);
    if (i < 0) return;
    if (QMessageBox::question(this, "Supprimer",
            QString("Supprimer %1 %2 ?").arg(m_data[i].prenom, m_data[i].nom)) != QMessageBox::Yes)
        return;
    const QString who = m_data[i].prenom + " " + m_data[i].nom;
    m_data.removeAt(i);
    saveData();
    addNotification("Participant supprimé", QString("%1 a été supprimé de la liste.").arg(who));
    refreshTable();
}
