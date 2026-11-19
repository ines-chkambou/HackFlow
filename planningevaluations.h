#ifndef PLANNINGEVALUATIONS_H
#define PLANNINGEVALUATIONS_H

#include <QDate>
#include <QMap>
#include <QStringList>
#include <QTime>
#include <QVector>
#include <QWidget>

class QComboBox;
class QDateEdit;
class QHBoxLayout;
class QLabel;
class QLineEdit;
class QMenu;
class QPushButton;
class QTableWidget;

/**
 * @brief Une session du planning : un juré évalue un projet à une date, une heure et dans une salle.
 * Table SESSION_EVALUATION (jointe à PROJET et JURY).
 */
struct SessionEval
{
    int id = -1;
    QDate date;
    QTime heure;
    int idProjet = -1;
    QString titreProjet;
    QString descriptionProjet;
    QString equipe;
    int idJury = -1;
    QString nomJury;
    QString prenomJury;
    QString salle;
    QString statut = "Planifiée";     // Planifiée | En cours | Terminée

    QString nomCompletJury() const { return nomJury + " " + prenomJury; }
};

struct ResultatVerificationConflit
{
    enum Etat { AucunConflit, ConflitDetecte, ErreurSql };
    Etat etat = AucunConflit;
    QString message;
};

/**
 * @brief Mini-calendrier mensuel (lundi en premier) : plage de dates en surbrillance,
 * points colorés sous les jours qui ont des sessions, clic sur un jour.
 */
class MiniCalendrier : public QWidget
{
    Q_OBJECT

public:
    explicit MiniCalendrier(QWidget *parent = nullptr);

    QDate mois() const { return m_mois; }
    void setMois(const QDate &d);                       // n'importe quel jour du mois à afficher
    void setPlage(const QDate &debut, const QDate &fin);
    void setMarques(const QMap<QDate, QStringList> &statutsParJour);

    QSize sizeHint() const override { return QSize(280, 206); }
    QSize minimumSizeHint() const override { return QSize(220, 206); }

    // Date de la cellule située à (ligne, colonne) de la grille affichée
    QDate dateCellule(int ligne, int colonne) const;

signals:
    void jourClique(const QDate &date);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    QDate m_mois;      // 1er jour du mois affiché
    QDate m_debut, m_fin;
    QDate m_survol;
    QMap<QDate, QStringList> m_marques;

    QRectF rectCellule(int ligne, int colonne) const;
    bool cellulePosition(const QPoint &p, int &ligne, int &colonne) const;
};

/**
 * @brief Page « Planning des évaluations » (page du QStackedWidget ouverte depuis le bouton
 * « Planning des évaluations » de la page Jury).
 */
class PlanningEvaluationsPage : public QWidget
{
    Q_OBJECT

public:
    enum Vue { VueJour, VueSemaine, VueMois };

    explicit PlanningEvaluationsPage(QWidget *parent = nullptr);

    void rafraichir();
    void definirModificationAutorisee(bool autorisee);

    // ---- Accès aux données (statiques : utilisables et testables sans l'interface) ----
    static QVector<SessionEval> lireSessions();
    static SessionEval lireSession(int id);
    static ResultatVerificationConflit verifierConflit(const QDate &date, const QTime &heure, int idJury,
                                                       const QString &salle, int exclureId);
    // Valide une session complète (date/heure, salle <= 100, statut, conflits de créneau).
    static bool validerSession(const SessionEval &s, int exclureId, QString *messageErreur = nullptr);
    static bool ajouterSession(const SessionEval &s, int *nouvelId = nullptr, QString *messageErreur = nullptr);
    static bool modifierSession(const SessionEval &s, QString *messageErreur = nullptr);
    static bool supprimerSession(int id);
    static bool exporterCSV(const QString &chemin, const QVector<SessionEval> &sessions);
    static bool exporterPDF(const QString &chemin, const QVector<SessionEval> &sessions, const QString &periode);

    // ---- État / navigation (utilisés aussi par les tests) ----
    void definirVue(Vue vue);
    void definirPeriode(const QDate &debut, const QDate &fin);
    QVector<SessionEval> sessionsFiltrees() const;

signals:
    void retourDemande();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void surFiltreChange();
    void surDateModifiee();
    void surJourCalendrier(const QDate &date);
    void surRecherche(const QString &texte);
    void surTriDemande(int colonne);
    void surCaseLigne();
    void allerPage(int page);
    void ajouterSessionDialogue();
    void exporterDialogue(bool pdf);

private:
    // Filtres
    QComboBox *m_cbProjet;
    QComboBox *m_cbJury;
    QComboBox *m_cbEquipe;
    QComboBox *m_cbStatut;
    QDateEdit *m_dateDu;
    QDateEdit *m_dateAu;
    // Calendrier
    MiniCalendrier *m_calendrier;
    QLabel *m_lblMois;
    // En-tête
    QPushButton *m_btnAjouter;
    QPushButton *m_btnExporter;
    QPushButton *m_btnVue;
    QMenu *m_menuVue;
    // Liste
    QLabel *m_lblNombre;
    QLineEdit *m_inputRecherche;
    QTableWidget *m_table;
    QLabel *m_lblAffichage;
    QPushButton *m_btnPrecedent;
    QPushButton *m_btnSuivant;
    QHBoxLayout *m_pagesLayout;

    Vue m_vue;
    int m_triColonne;        // 1 = Date, 2 = Heure
    bool m_triCroissant;
    int m_page;
    int m_taillePage;
    bool m_modificationAutorisee;
    bool m_chargement;       // vrai pendant la reconstruction des filtres (évite les rechargements en cascade)

    void construireInterface();
    QWidget *construireEnTete();
    QWidget *construireCarteFiltres();
    QWidget *construireCarteCalendrier();
    QWidget *construireCarteListe();
    void rechargerFiltres();
    void chargerListe();
    void mettreAJourCalendrier();
    void mettreAJourPagination(int total, int debut, int fin, int nbPages);
    void appliquerVue();
    void definirToutCoche(bool coche);
    bool toutCoche() const;
    void changerMois(int delta);
    QString libellePeriode() const;

    // Dialogues
    void dialogueSession(int idSession);          // -1 : ajout
    void detailSession(int idSession);
    void confirmerSuppression(int idSession);
};

#endif // PLANNINGEVALUATIONS_H
