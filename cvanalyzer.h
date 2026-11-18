#ifndef CVANALYZER_H
#define CVANALYZER_H

#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>

// ---------------------------------------------------------------------------
//  Attribution automatique intelligente des employes - traitement de CV
//   1. extraction du texte du CV            (.pdf [module Qt PDF], .docx, .txt/.md)
//   2. detection des competences            (dictionnaire de ~250 mots-cles en 9 categories)
//   3. score de compatibilite avec chaque equipe (profil de competences de l'equipe)
//   4. equilibrage : une equipe deja chargee est penalisee, une equipe pleine est exclue
//   5. suggestion de role / departement
//  Les profils d'equipes sont definis dans cvanalyzer.cpp (tableau kTeams) : a adapter librement.
// ---------------------------------------------------------------------------
struct SkillHit {
    QString     category;
    double      score = 0;        // 0..1 (saturation)
    QStringList keywords;         // mots-cles retrouves dans le CV
};

struct TeamScore {
    QString team;                 // "Team Alpha"
    QString label;                // "Développement Web"
    double  affinity = 0;         // 0..1 : adequation brute competences <-> equipe
    double  finalScore = 0;       // 0..1 : apres equilibrage de charge
    int     load = 0;             // effectif actuel
    bool    full = false;         // effectif max atteint
    bool    alreadyMember = false;
    QString reason;               // justification lisible
    bool eligible() const { return !full && !alreadyMember; }
};

struct CvAnalysis {
    bool    ok = false;
    QString error;
    int     words = 0;
    int     years = -1;           // annees d'experience detectees (-1 = inconnu)
    QString cvEmail;              // e-mail trouve dans le CV (controle de coherence)
    QList<SkillHit>  skills;      // tries par score decroissant
    QList<TeamScore> teams;       // eligibles d'abord, tries par score decroissant
    QString suggestedRole;
    QString suggestedDept;
    bool hasSkills() const { return !skills.isEmpty(); }
    const TeamScore *best() const { return (!teams.isEmpty() && teams.first().eligible() && hasSkills()) ? &teams.first() : nullptr; }
};

class CvAnalyzer
{
public:
    static QString extractText(const QString &path, QString *error);
    static QStringList teamNames();
    static QString fileFilter();
    static bool pdfSupported();

    static CvAnalysis analyze(const QString &text,
                              const QMap<QString, int> &teamLoad,
                              const QStringList &currentTeams,
                              int maxTeamSize);
};

#endif // CVANALYZER_H
