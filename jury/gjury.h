#ifndef GJURY_H
#define GJURY_H

#include <QMainWindow>
#include <QTableWidgetItem>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QFileDialog>
#include "jury.h"
#include "evaluation.h"
#include "chartwidgets.h"

class QCheckBox;
class QComboBox;
class QLineEdit;
class ParametresJuryPage;
class PlanningEvaluationsPage;
class EvaluationProjetsPage;

QT_BEGIN_NAMESPACE
namespace Ui { class GJury; }
QT_END_NAMESPACE

/**
 * @brief Fenêtre principale de l'application HackFlow pour le module Gestion des Jurys.
 * Nommée GJury conformément aux bonnes pratiques universitaires ("G" + Nom du sujet).
 * Respecte strictement la règle d'un fichier .ui unique avec conteneur QStackedWidget.
 */
class GJury : public QMainWindow
{
    Q_OBJECT

public:
    explicit GJury(QWidget *parent = nullptr);
    ~GJury();

private slots:
    // Navigation dans la sidebar
    void on_btnNavAccueil_clicked();
    void on_btnNavParticipants_clicked();
    void on_btnNavProjets_clicked();
    void on_btnNavEmployes_clicked();
    void on_btnNavJury_clicked();
    void on_btnNavEvals_clicked();
    void on_btnNavInnovant_clicked();
    void on_btnNavConnexion_clicked();
    void on_btnNavParametres_clicked();

    // Raccourcis (colonne de droite de la page Jury)
    void on_btnRaccEvals_clicked();
    void on_btnRaccEquipes_clicked();
    void on_btnRaccPlanning_clicked();
    void on_btnRaccParametres_clicked();
    void retourDepuisParametres();

    // Page Jury : en-tête, tableau, filtres, pagination
    void on_btnHeaderAjouter_clicked();
    void on_tableJurys_cellDoubleClicked(int row, int column);
    void on_inputRechercheJury_textChanged(const QString &text);
    void on_comboFiltreRole_currentIndexChanged(int index);
    void on_comboFiltreEquipe_currentIndexChanged(int index);
    void on_comboFiltreDispo_currentIndexChanged(int index);
    void on_btnFiltrer_clicked();
    void on_btnPagePrecedente_clicked();
    void on_btnPageSuivante_clicked();

    // Tri et Exports Jury
    void on_btnTrierJury_clicked();
    void on_btnExporterJury_clicked();
    void on_btnExportPDF_clicked();
    void on_btnExportCSV_clicked();

    // Formulaire Évaluation
    void on_btnEnregistrerEval_clicked();
    void on_btnViderEval_clicked();
    void on_tableEvals_cellClicked(int row, int column);
    void on_inputRechercheEval_textChanged(const QString &text);
    void on_sliderInnovation_valueChanged(int value);
    void on_sliderTechnique_valueChanged(int value);
    void on_sliderPresentation_valueChanged(int value);
    void on_sliderPitch_valueChanged(int value);

    // Métiers Innovants
    void on_btnActualiserClassement_clicked();
    void on_btnExportClassementPDF_clicked();
    void on_btnAnalyserDivergences_clicked();

    // Authentification & Gestion des privilèges
    void on_btnSeConnecter_clicked();
    void on_btnDeconnexion_clicked();
    void on_btnTogglePassword_clicked();

private:
    Ui::GJury *ui;

    // Page Jury : pagination et tri courants
    int m_pageCourante;
    int m_taillePage;
    QString m_triCritere;
    QString m_triOrdre;
    QCheckBox *m_chkTous;       // case « tout sélectionner » de l'en-tête du tableau

    // Mode édition évaluation
    bool m_modeEditionEval;
    int m_idEvalEnEdition;

    // Rôle et privilèges courants
    QString m_roleCourant; // "Administrateur", "Membre du Jury", "Mentor"
    QString m_nomUtilisateurCourant;

    // Anneau de statistiques
    DonutChartWidget *m_donutChart;

    // Page « Paramètres du jury » (page 5 du QStackedWidget), ouverte depuis le raccourci de la page Jury
    ParametresJuryPage *m_pageParametres;

    // Page « Planning des évaluations » (page 6 du QStackedWidget)
    PlanningEvaluationsPage *m_pagePlanning;

    // Page « Saisie et Grille d'Évaluation des Projets » (ouverte depuis « Voir les évaluations »)
    EvaluationProjetsPage *m_pageEvaluations;

    // Initialisation
    void setupUIStyling();
    void configurerPageParametres();
    void configurerPagePlanning();
    void configurerPageEvaluations();
    void configurerBarreSuperieure();
    void ajouterLoupe(QLineEdit *champ, int x, int y, int taille);
    void chargerFiltresJurys();
    void chargerTableauJurys();
    void chargerTableauEvaluations();
    void chargerCombosProjetsEtJurys();
    void actualiserStatistiquesEtGraphiques();
    void actualiserClassementInnovant();
    void actualiserAlertesDivergence();
    void appliquerPrivilegesRole();
    void rafraichirApresModificationJury();
    bool estAdministrateur() const;

    // Cellules du tableau
    QWidget *creerCelluleBadge(const QString &texte, const QString &fond, const QString &couleurTexte, int largeur);
    void remplirCombo(QComboBox *combo, const QString &premier, const QStringList &valeurs);

    // Actions du tableau (Détail, Modifier, Supprimer)
    void afficherDetailJury(int idJury);
    void ouvrirDialogueJury(int idJury = -1);   // -1 : ajout, sinon modification
    void supprimerJury(int idJury);

    void recalculerNoteGlobaleFormulaire();
    void definirNavActif(QPushButton *actif);
};

#endif // GJURY_H
