#include "cvanalyzer.h"

#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QXmlStreamReader>
#include <QtMath>
#include <algorithm>

#include <private/qzipreader_p.h>          // lecture du .docx (zip)
#if defined(HAVE_QTPDF) && QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
#  include <QPdfDocument>
#  include <QPdfSelection>
#  define CV_PDF 1
#endif

// ===========================================================================
//  DICTIONNAIRE DE COMPETENCES  (mots-cles en minuscules, SANS accents)
//  poids 1.0 = specifique ; 0.5 = generique
// ===========================================================================
namespace {
struct Kw { const char *word; double w; };
struct Cat { const char *name; QList<Kw> kws; };

const QList<Cat> &categories()
{
    static const QList<Cat> c = {
      { "Web", {
        {"html",1},{"css",1},{"javascript",1},{"typescript",1},{"react",1},{"angular",1},{"vue",1},{"vue.js",1},
        {"node.js",1},{"nodejs",1},{"express",.5},{"php",1},{"laravel",1},{"symfony",1},{"django",1},{"flask",1},
        {"spring boot",1},{"wordpress",1},{"bootstrap",1},{"tailwind",1},{"next.js",1},{"rest api",1},{"api rest",1},
        {"graphql",1},{"frontend",1},{"front-end",1},{"front end",1},{"backend",1},{"back-end",1},{"back end",1},
        {"full stack",1},{"fullstack",1},{"full-stack",1},{"developpement web",1},{"web",.5},{"jquery",1},{"ui/ux",.5},{"figma",.5} } },
      { "Mobile", {
        {"android",1},{"ios",1},{"flutter",1},{"dart",1},{"kotlin",1},{"swift",1},{"swiftui",1},{"react native",1},
        {"xamarin",1},{"ionic",1},{"jetpack compose",1},{"application mobile",1},{"applications mobiles",1},
        {"developpement mobile",1},{"mobile",.5},{"firebase",.5},{"android studio",1},{"xcode",1} } },
      { "IA & Data", {
        {"python",.7},{"machine learning",1},{"deep learning",1},{"apprentissage automatique",1},{"tensorflow",1},
        {"pytorch",1},{"keras",1},{"scikit-learn",1},{"sklearn",1},{"nlp",1},{"traitement du langage",1},
        {"computer vision",1},{"vision par ordinateur",1},{"opencv",1},{"data science",1},{"data scientist",1},
        {"data analyst",1},{"data engineer",1},{"pandas",1},{"numpy",1},{"big data",1},{"spark",.7},{"hadoop",1},
        {"power bi",1},{"tableau",.5},{"statistiques",.5},{"intelligence artificielle",1},{"ia",.7},{"ai",.7},
        {"llm",1},{"sql",.5},{"mongodb",.5},{"etl",.7},{"data mining",1},{"reseaux de neurones",1},{"jupyter",1} } },
      { "IoT & Embarque", {
        {"arduino",1},{"raspberry pi",1},{"raspberry",1},{"esp32",1},{"esp8266",1},{"iot",1},{"internet des objets",1},
        {"embarque",1},{"systemes embarques",1},{"microcontroleur",1},{"microcontrolleur",1},{"stm32",1},{"mqtt",1},
        {"capteur",1},{"capteurs",1},{"electronique",1},{"fpga",1},{"vhdl",1},{"verilog",1},{"rtos",1},{"freertos",1},
        {"robotique",1},{"ros",.7},{"lora",1},{"zigbee",1},{"automate",.5},{"plc",1},{"pcb",1},{"embedded",1} } },
      { "Cybersecurite & Reseaux", {
        {"cybersecurite",1},{"securite informatique",1},{"pentest",1},{"penetration testing",1},{"ethical hacking",1},
        {"kali linux",1},{"owasp",1},{"cryptographie",1},{"firewall",1},{"pare-feu",1},{"siem",1},{"soc",.7},
        {"reseau",.7},{"reseaux",.7},{"cisco",1},{"ccna",1},{"ccnp",1},{"vpn",1},{"tcp/ip",1},{"vlan",1},{"routage",1},
        {"wireshark",1},{"nmap",1},{"metasploit",1},{"iso 27001",1},{"forensic",1},{"ctf",1},{"securite",.5} } },
      { "DevOps & Cloud", {
        {"docker",1},{"kubernetes",1},{"k8s",1},{"jenkins",1},{"gitlab ci",1},{"github actions",1},{"ci/cd",1},
        {"terraform",1},{"ansible",1},{"aws",1},{"azure",1},{"gcp",1},{"google cloud",1},{"cloud",.7},{"linux",.7},
        {"devops",1},{"nginx",.7},{"git",.5},{"prometheus",1},{"grafana",1},{"administration systeme",1},{"bash",.5} } },
      { "Logiciel & Desktop", {
        {"c++",1},{"c#",1},{".net",1},{"qt",1},{"qt creator",1},{"qml",1},{"java",1},{"javafx",1},{"swing",.7},{"wpf",1},
        {"winforms",1},{"langage c",1},{"uml",.7},{"poo",.7},{"oop",.7},{"programmation orientee objet",1},
        {"design patterns",1},{"rust",1},{"golang",1},{"qmake",1},{"cmake",1},{"sqlite",.5},{"oracle",.5} } },
      { "Gestion & Mentorat", {
        {"gestion de projet",1},{"project management",1},{"scrum",1},{"agile",1},{"kanban",1},{"jira",.7},
        {"product owner",1},{"scrum master",1},{"mentorat",1},{"mentor",1},{"coaching",1},{"coach",.7},{"formation",.5},
        {"formateur",1},{"enseignement",1},{"enseignant",1},{"pedagogie",1},{"pedagogique",1},{"leadership",1},
        {"encadrement",1},{"animation",.7},{"communication",.5},{"pitch",.7},{"soft skills",.5},{"management",.7} } },
      { "Logistique & Organisation", {
        {"logistique",1},{"organisation d'evenements",1},{"evenementiel",1},{"evenement",.5},{"evenements",.5},
        {"planification",.7},{"planning",.5},{"coordination",.7},{"gestion de stock",1},{"approvisionnement",1},
        {"achats",.7},{"budget",.5},{"hospitalite",1},{"accueil",.5},{"protocole",.5},{"benevole",.7},{"benevolat",.7},
        {"gestion des equipements",1},{"materiel",.5},{"sponsoring",.7} } },
    };
    return c;
}

// Profil de chaque equipe : poids par categorie (1.0 = coeur de metier, <1 = competence utile)
struct TeamProfile { const char *team, *label; QMap<QString, double> weights; };
const QList<TeamProfile> &teamProfiles()
{
    static const QList<TeamProfile> t = {
        { "Team Alpha", "Développement Web",         { {"Web",1.0}, {"DevOps & Cloud",0.35}, {"Logiciel & Desktop",0.2} } },
        { "Team Beta",  "IA & Data",                 { {"IA & Data",1.0}, {"Web",0.15}, {"Logiciel & Desktop",0.15} } },
        { "Team Gamma", "Applications mobiles",      { {"Mobile",1.0}, {"Web",0.3}, {"Logiciel & Desktop",0.2} } },
        { "Team Delta", "IoT & Systèmes embarqués",  { {"IoT & Embarque",1.0}, {"Logiciel & Desktop",0.3}, {"Cybersecurite & Reseaux",0.15} } },
        { "Team Omega", "Cybersécurité & Cloud",     { {"Cybersecurite & Reseaux",1.0}, {"DevOps & Cloud",0.6} } },
    };
    return t;
}

QString normalize(QString s)
{
    s.replace(QChar(0x2019), '\'').replace(QChar(0x00A0), ' ').replace(QChar(0x2011), '-');
    s = s.normalized(QString::NormalizationForm_D);
    QString out; out.reserve(s.size());
    for (const QChar c : s)
        if (c.category() != QChar::Mark_NonSpacing) out.append(c);
    return out.toLower();
}

int countOccurrences(const QString &text, const QString &kw)
{
    const QString pat = "(?<![\\p{L}\\p{N}])" + QRegularExpression::escape(kw) + "(?![\\p{L}\\p{N}])";
    QRegularExpression re(pat);
    int n = 0;
    auto it = re.globalMatch(text);
    while (it.hasNext()) { it.next(); ++n; }
    return n;
}

int detectYears(const QString &t)
{
    static const QRegularExpression a("(\\d{1,2})\\s*\\+?\\s*(?:ans?|annees?|years?)\\s*(?:d'|de\\s+)?\\s*(?:experience|exp\\b)");
    static const QRegularExpression b("experience[^\\n\\d]{0,25}(\\d{1,2})\\s*\\+?\\s*(?:ans?|annees?|years?)");
    int best = -1;
    for (const QRegularExpression *re : { &a, &b }) {
        auto it = re->globalMatch(t);
        while (it.hasNext()) { const int y = it.next().captured(1).toInt(); if (y <= 45) best = qMax(best, y); }
    }
    return best;
}
} // namespace

// ===========================================================================
//  EXTRACTION DU TEXTE
// ===========================================================================
bool CvAnalyzer::pdfSupported()
{
#ifdef CV_PDF
    return true;
#else
    return false;
#endif
}

QString CvAnalyzer::fileFilter()
{
    return pdfSupported() ? "CV (*.pdf *.docx *.txt *.md);;Tous les fichiers (*)"
                          : "CV (*.docx *.txt *.md);;Tous les fichiers (*)";
}

QString CvAnalyzer::extractText(const QString &path, QString *error)
{
    auto fail = [&](const QString &m) { if (error) *error = m; return QString(); };
    const QFileInfo fi(path);
    if (!fi.exists() || !fi.isFile())  return fail("Fichier introuvable.");
    if (fi.size() > 25 * 1024 * 1024)  return fail("Fichier trop volumineux (max 25 Mo).");
    const QString ext = fi.suffix().toLower();

    if (ext == "txt" || ext == "md") {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) return fail("Lecture du fichier impossible.");
        return QString::fromUtf8(f.read(4 * 1024 * 1024));
    }

    if (ext == "docx") {
        QZipReader zip(path);
        if (!zip.isReadable()) return fail("Fichier .docx illisible ou corrompu.");
        const QByteArray xml = zip.fileData("word/document.xml");
        if (xml.isEmpty()) return fail("Document Word vide ou invalide.");
        QXmlStreamReader r(xml);
        QString out;
        while (!r.atEnd()) {
            r.readNext();
            if (r.isStartElement()) {
                const auto n = r.name();
                if (n == QLatin1String("t"))        out += r.readElementText();
                else if (n == QLatin1String("tab")) out += ' ';
                else if (n == QLatin1String("br"))  out += '\n';
            } else if (r.isEndElement() && r.name() == QLatin1String("p")) {
                out += '\n';
            }
        }
        return out;
    }

    if (ext == "pdf") {
#ifdef CV_PDF
        QPdfDocument doc;
        const auto err = doc.load(path);
        if (err == QPdfDocument::Error::IncorrectPassword) return fail("PDF protégé par mot de passe.");
        if (err != QPdfDocument::Error::None)              return fail("PDF illisible ou invalide.");
        QString out;
        for (int p = 0; p < doc.pageCount() && p < 30; ++p)
            out += doc.getAllText(p).text() + "\n";
        return out;
#else
        return fail("La lecture des PDF nécessite le module Qt PDF (non installé dans ce Qt).\n"
                    "Enregistrez le CV en .docx ou .txt, ou installez « Qt PDF » avec le Qt Maintenance Tool.");
#endif
    }
    return fail("Format non pris en charge (.pdf, .docx, .txt, .md).");
}

// ===========================================================================
//  ANALYSE
// ===========================================================================
QStringList CvAnalyzer::teamNames()
{
    QStringList l; for (const auto &t : teamProfiles()) l << t.team; return l;
}

CvAnalysis CvAnalyzer::analyze(const QString &rawText, const QMap<QString, int> &teamLoad,
                               const QStringList &currentTeams, int maxTeamSize)
{
    CvAnalysis a;
    const QString text = normalize(rawText);
    a.words = text.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts).size();
    if (a.words < 30) {
        a.error = "Le CV ne contient presque pas de texte exploitable (PDF scanné/image ?). "
                  "L'OCR n'est pas pris en charge : utilisez un CV avec du texte sélectionnable.";
        return a;
    }
    a.ok = true;
    a.years = detectYears(text);
    static const QRegularExpression mailRe("[a-z0-9._%+\\-]+@[a-z0-9\\-]+(?:\\.[a-z0-9\\-]+)*\\.[a-z]{2,}");
    const auto mm = mailRe.match(text);
    if (mm.hasMatch()) a.cvEmail = mm.captured(0);

    // ---- 1) competences par categorie
    QMap<QString, SkillHit> hits;
    QMap<QString, QList<QPair<double, QString>>> kwScores;
    for (const Cat &cat : categories()) {
        double raw = 0;
        for (const Kw &k : cat.kws) {
            const int n = countOccurrences(text, QString::fromLatin1(k.word));
            if (n <= 0) continue;
            const double s = k.w * (1.0 + qLn(double(n)));
            raw += s;
            kwScores[cat.name].append({ s, QString::fromLatin1(k.word) });
        }
        if (raw <= 0) continue;
        SkillHit h; h.category = cat.name;
        h.score = 1.0 - qExp(-raw / 3.0);                 // saturation douce : 0..1
        auto &lst = kwScores[cat.name];
        std::sort(lst.begin(), lst.end(), [](auto &x, auto &y) { return x.first > y.first; });
        for (const auto &p : lst) h.keywords << p.second;
        hits.insert(cat.name, h);
    }
    a.skills = hits.values();
    std::sort(a.skills.begin(), a.skills.end(), [](const SkillHit &x, const SkillHit &y) { return x.score > y.score; });

    // ---- 2) score par equipe
    for (const TeamProfile &tp : teamProfiles()) {
        TeamScore ts; ts.team = tp.team; ts.label = tp.label;
        double sum = 0; QStringList why;
        for (auto it = tp.weights.constBegin(); it != tp.weights.constEnd(); ++it) {
            if (!hits.contains(it.key())) continue;
            sum += it.value() * hits[it.key()].score;
            if (it.value() >= 0.3) {
                QStringList kws = hits[it.key()].keywords.mid(0, 4);
                why << QString("%1 (%2)").arg(it.key(), kws.join(", "));
            }
        }
        ts.affinity = qMin(1.0, sum);
        ts.load = teamLoad.value(tp.team, 0);
        ts.full = ts.load >= maxTeamSize;
        ts.alreadyMember = currentTeams.contains(tp.team);
        const double loadRatio = maxTeamSize > 0 ? qMin(1.0, double(ts.load) / maxTeamSize) : 0;
        ts.finalScore = ts.affinity * (1.0 - 0.20 * loadRatio);      // equilibrage de charge
        ts.reason = why.isEmpty() ? "Aucune compétence correspondante." : why.join(" ; ");
        a.teams << ts;
    }
    std::stable_sort(a.teams.begin(), a.teams.end(), [](const TeamScore &x, const TeamScore &y) {
        if (x.eligible() != y.eligible()) return x.eligible();
        return x.finalScore > y.finalScore;
    });

    // ---- 3) role / departement suggeres
    auto sc = [&](const char *n) { return hits.contains(n) ? hits[n].score : 0.0; };
    const double tech = qMax(qMax(qMax(sc("Web"), sc("Mobile")), qMax(sc("IA & Data"), sc("IoT & Embarque"))),
                             qMax(qMax(sc("Cybersecurite & Reseaux"), sc("DevOps & Cloud")), sc("Logiciel & Desktop")));
    const double mentor = sc("Gestion & Mentorat"), logi = sc("Logistique & Organisation");
    if (tech <= 0 && mentor <= 0 && logi <= 0) {
        a.suggestedDept.clear(); a.suggestedRole.clear();
    } else if (tech >= mentor && tech >= logi) {
        a.suggestedDept = "Technique";
        a.suggestedRole = (a.years >= 6 && mentor >= 0.25) ? "Mentor" : "Technicien";
    } else if (mentor >= logi) {
        a.suggestedDept = "Pédagogie/Mentorat";
        a.suggestedRole = mentor >= 0.4 ? "Mentor" : "Organisateur";
    } else {
        a.suggestedDept = "Logistique";
        a.suggestedRole = a.years >= 4 ? "Organisateur" : "Technicien";
    }
    return a;
}
