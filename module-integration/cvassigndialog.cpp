#include "cvassigndialog.h"
#include "dialogstyle.h"

#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QStyle>
#include <QTableWidget>
#include <QVBoxLayout>

CvAssignDialog::CvAssignDialog(const EmployeeInfo &emp, const QMap<QString, int> &teamLoad,
                               int maxTeamSize, QWidget *parent)
    : QDialog(parent), m_emp(emp), m_load(teamLoad), m_max(maxTeamSize)
{
    setWindowTitle("Attribution automatique à une équipe");
    setStyleSheet(hackflowDialogStyle());
    resize(860, 700);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 18); root->setSpacing(10);

    auto *title = new QLabel(QString("Attribution automatique — %1 %2").arg(emp.prenom, emp.nom));
    title->setObjectName("dlgTitle");
    const QString teams = emp.teams.isEmpty() ? "aucune" : emp.teams.join(", ");
    auto *sub = new QLabel(QString("%1 · %2 · %3 · Équipe(s) actuelle(s) : %4")
                           .arg(emp.role, emp.dept, emp.dispo, teams));
    sub->setObjectName("dlgSub"); sub->setWordWrap(true);
    root->addWidget(title); root->addWidget(sub);

    auto *rowCv = new QHBoxLayout;
    auto *btnBrowse = new QPushButton("Charger le CV…");
    m_cvName = new QLabel(CvAnalyzer::pdfSupported() ? "Aucun CV chargé (PDF, DOCX, TXT)"
                                                     : "Aucun CV chargé (DOCX, TXT — PDF indisponible dans ce Qt)");
    m_cvName->setObjectName("dlgSub");
    rowCv->addWidget(btnBrowse); rowCv->addWidget(m_cvName, 1);
    root->addLayout(rowCv);

    m_status = new QLabel; m_status->setWordWrap(true); m_status->hide();
    root->addWidget(m_status);
    if (emp.dispo != "Disponible")
        setStatus("Attention : cet employé est marqué « " + emp.dispo + " ».", "dlgInfo");

    auto *lblS = new QLabel("<b>Compétences détectées</b>");
    m_skills = new QTableWidget(0, 3);
    m_skills->setHorizontalHeaderLabels({ "Domaine", "Niveau", "Mots-clés trouvés dans le CV" });
    auto *lblT = new QLabel("<b>Compatibilité avec les équipes</b>");
    m_teams = new QTableWidget(0, 5);
    m_teams->setHorizontalHeaderLabels({ "Équipe", "Domaine", "Compatibilité", "Effectif", "Justification" });
    for (QTableWidget *t : { m_skills, m_teams }) {
        t->verticalHeader()->hide();
        t->setEditTriggers(QAbstractItemView::NoEditTriggers);
        t->setSelectionMode(QAbstractItemView::NoSelection);
        t->setAlternatingRowColors(true);
        t->setShowGrid(false);
        t->setWordWrap(true);
        t->setFocusPolicy(Qt::NoFocus);
    }
    m_skills->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_skills->setColumnWidth(1, 150);
    m_skills->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_teams->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_teams->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_teams->setColumnWidth(2, 150);
    m_teams->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_teams->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    root->addWidget(lblS); root->addWidget(m_skills, 2);
    root->addWidget(lblT); root->addWidget(m_teams, 2);

    m_suggest = new QLabel; m_suggest->setObjectName("dlgInfo"); m_suggest->setWordWrap(true); m_suggest->hide();
    root->addWidget(m_suggest);

    auto *foot = new QHBoxLayout;
    foot->addWidget(new QLabel("Équipe retenue :"));
    m_combo = new QComboBox; m_combo->setMinimumWidth(340); m_combo->setEnabled(false);
    foot->addWidget(m_combo); foot->addStretch();
    auto *btnCancel = new QPushButton("Annuler"); btnCancel->setObjectName("secondary");
    m_btnOk = new QPushButton("Affecter à cette équipe"); m_btnOk->setEnabled(false);
    foot->addWidget(btnCancel); foot->addWidget(m_btnOk);
    root->addLayout(foot);

    connect(btnBrowse, &QPushButton::clicked, this, &CvAssignDialog::onBrowse);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_btnOk, &QPushButton::clicked, this, &CvAssignDialog::onConfirm);
}

void CvAssignDialog::setStatus(const QString &text, const char *kind)
{
    m_status->setObjectName(kind);
    m_status->setText(text);
    m_status->style()->unpolish(m_status); m_status->style()->polish(m_status);
    m_status->setMinimumHeight(m_status->heightForWidth(width() - 48) > 0 ? m_status->heightForWidth(width() - 48) + 20 : 0);
    m_status->updateGeometry();
    m_status->show();
}

void CvAssignDialog::onBrowse()
{
    const QString path = QFileDialog::getOpenFileName(this, "Choisir le CV de l'employé", QString(),
                                                      CvAnalyzer::fileFilter());
    if (!path.isEmpty()) loadCvFile(path);
}

void CvAssignDialog::loadCvFile(const QString &path)
{
    m_cvName->setText(QFileInfo(path).fileName());
    m_skills->setRowCount(0); m_teams->setRowCount(0);
    m_combo->clear(); m_combo->setEnabled(false); m_btnOk->setEnabled(false); m_suggest->hide();

    QString err;
    const QString text = CvAnalyzer::extractText(path, &err);
    if (text.isEmpty() && !err.isEmpty()) { setStatus(err, "dlgError"); return; }
    const CvAnalysis a = CvAnalyzer::analyze(text, m_load, m_emp.teams, m_max);
    if (!a.ok) { setStatus(a.error, "dlgError"); return; }
    showAnalysis(a);
}

void CvAssignDialog::showAnalysis(const CvAnalysis &a)
{
    // ---- competences
    m_skills->setRowCount(a.skills.size());
    for (int i = 0; i < a.skills.size(); ++i) {
        const SkillHit &h = a.skills[i];
        m_skills->setItem(i, 0, new QTableWidgetItem(h.category));
        auto *pb = new QProgressBar; pb->setRange(0, 100); pb->setValue(int(h.score * 100 + 0.5));
        m_skills->setCellWidget(i, 1, pb);
        m_skills->setItem(i, 2, new QTableWidgetItem(h.keywords.mid(0, 12).join(", ")));
    }
    m_skills->resizeRowsToContents();

    // ---- equipes
    m_teams->setRowCount(a.teams.size());
    for (int i = 0; i < a.teams.size(); ++i) {
        const TeamScore &t = a.teams[i];
        m_teams->setItem(i, 0, new QTableWidgetItem(t.team));
        m_teams->setItem(i, 1, new QTableWidgetItem(t.label));
        auto *pb = new QProgressBar; pb->setRange(0, 100); pb->setValue(int(t.finalScore * 100 + 0.5));
        m_teams->setCellWidget(i, 2, pb);
        QString eff = QString("%1 / %2").arg(t.load).arg(m_max);
        if (t.full) eff += " (pleine)";
        m_teams->setItem(i, 3, new QTableWidgetItem(eff));
        QString why = t.reason;
        if (t.alreadyMember) why = "Déjà membre de cette équipe. " + why;
        m_teams->setItem(i, 4, new QTableWidgetItem(why));
        if (!t.eligible())
            for (int c : { 0, 1, 3, 4 }) m_teams->item(i, c)->setForeground(QColor("#6F7FA8"));
    }
    m_teams->resizeRowsToContents();

    // ---- equipe retenue (combo = equipes eligibles ; la meilleure est preselectionnee)
    m_combo->clear();
    for (const TeamScore &t : a.teams)
        if (t.eligible()) m_combo->addItem(QString("%1 — %2 (%3 %)").arg(t.team, t.label)
                                           .arg(int(t.finalScore * 100 + 0.5)), t.team);
    m_combo->setEnabled(m_combo->count() > 0);
    m_btnOk->setEnabled(m_combo->count() > 0);
    if (m_combo->count()) m_combo->setCurrentIndex(0);

    // ---- messages
    QStringList notes;
    if (!a.cvEmail.isEmpty() && !m_emp.email.isEmpty() && a.cvEmail != m_emp.email.toLower())
        notes << QString("L'e-mail trouvé dans le CV (%1) ne correspond pas à celui de l'employé (%2) : "
                         "vérifiez qu'il s'agit du bon CV.").arg(a.cvEmail, m_emp.email);
    if (m_combo->count() == 0)
        setStatus("Aucune équipe disponible (toutes pleines ou déjà rejointes).", "dlgError");
    else if (!a.hasSkills())
        setStatus("Aucune compétence reconnue dans ce CV : l'attribution automatique n'est pas fiable. "
                  "Vous pouvez choisir une équipe manuellement ci-dessous.", "dlgInfo");
    else if (const TeamScore *b = a.best())
        setStatus(QString("Équipe recommandée : %1 — %2 (compatibilité %3 %).%4")
                  .arg(b->team, b->label).arg(int(b->finalScore * 100 + 0.5))
                  .arg(notes.isEmpty() ? QString() : "\n" + notes.join("\n")),
                  notes.isEmpty() ? "dlgOk" : "dlgInfo");
    else
        setStatus(notes.join("\n"), "dlgInfo");

    QStringList sug;
    if (!a.suggestedDept.isEmpty())
        sug << QString("Profil suggéré : rôle <b>%1</b>, département <b>%2</b>").arg(a.suggestedRole, a.suggestedDept);
    if (a.years >= 0) sug << QString("expérience détectée : <b>%1 an(s)</b>").arg(a.years);
    if (!sug.isEmpty()) { m_suggest->setText(sug.join(" · ") + " (à titre indicatif)."); m_suggest->show(); }
}

void CvAssignDialog::onConfirm()
{
    if (m_combo->currentIndex() < 0) return;
    m_chosen = m_combo->currentData().toString();
    accept();
}
