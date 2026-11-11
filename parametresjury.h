#ifndef PARAMETRESJURY_H
#define PARAMETRESJURY_H

#include <QWidget>
#include <QString>
#include <QVector>

class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;
class QHBoxLayout;
class QMenu;

/**
 * @brief Réglages du jury (page « Paramètres du jury »), enregistrés dans la table PARAMETRES_JURY.
 */
struct ParametresJury
{
    int nombreJurys = 8;            // nombre de jurys prévus
    int membresParSession = 3;      // membres par session d'évaluation
    int dureeEvaluationMin = 20;    // durée d'évaluation par projet (minutes)
    int noteTotaleParProjet = 20;   // barème total par projet

    static ParametresJury charger();
    bool enregistrer() const;
};

/**
 * @brief Page « Paramètres du jury » : formulaire « Informations générales » + « Liste des jurys ».
 *
 * La page est purement graphique : elle émet des signaux (retour, consulter, modifier) que la
 * fenêtre principale GJury relie à ses propres actions existantes.
 */
class ParametresJuryPage : public QWidget
{
    Q_OBJECT

public:
    explicit ParametresJuryPage(QWidget *parent = nullptr);

    // Recharge les paramètres enregistrés et la liste des jurys
    void rafraichir();
    // Un non-administrateur peut consulter mais pas modifier
    void definirModificationAutorisee(bool autorisee);

signals:
    void retourDemande();
    void consulterJuryDemande(int idJury);
    void modifierJuryDemande(int idJury);
    void parametresEnregistres();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void enregistrerParametres();
    void surRecherche(const QString &texte);
    void surTriDemande(int colonne);
    void surCaseTous(bool coche);
    void surCaseLigne();
    void allerPage(int page);

private:
    struct Ligne {
        int id;
        QString nom, prenom, specialite, equipe;
        bool actif;
    };

    // Formulaire
    QSpinBox *m_spinJurys;
    QSpinBox *m_spinMembres;
    QSpinBox *m_spinDuree;
    QSpinBox *m_spinNote;
    QPushButton *m_btnEnregistrer;

    // Liste
    QLineEdit *m_inputRecherche;
    QPushButton *m_btnFiltrer;
    QMenu *m_menuFiltre;
    QTableWidget *m_table;
    QLabel *m_lblAffichage;
    QPushButton *m_btnPrecedent;
    QPushButton *m_btnSuivant;
    QHBoxLayout *m_pagesLayout;

    // État
    QString m_filtreStatut;     // "", "Actif", "Inactif"
    QString m_filtreEquipe;     // "" = toutes
    int m_triColonne;           // 1 = Nom, 5 = Statut
    bool m_triCroissant;
    int m_page;
    int m_taillePage;
    bool m_modificationAutorisee;

    void construireInterface();
    QWidget *construireEnTete();
    QWidget *construireCarteInfos();
    QWidget *construireCarteListe();
    QWidget *creerLigneSpin(const QString &libelle, const QString &icone, QSpinBox *spin, const QString &suffixe = QString());
    void reconstruireMenuFiltre();
    void chargerParametres();
    void chargerListe();
    void mettreAJourPagination(int total, int debut, int fin, int nbPages);
    QVector<Ligne> lireJurys() const;
    bool toutCoche() const;
    void definirToutCoche(bool coche);
    int nombreTotalJurys() const;
};

#endif // PARAMETRESJURY_H
