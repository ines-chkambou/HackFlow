#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QComboBox>
#include <QStringList>
#include <QWidget>
#include <QVector>
#include <QString>
#include <QColor>

class QCheckBox;
class QStandardItemModel;
class ParticipantsController;   // module Participants (participantscontroller.h)
namespace Ui { class MainWindow; }

// ===========================================================================
//  Graphiques (anciennement chartwidgets.h / .cpp)
// ===========================================================================
struct ChartItem {
    QString label;
    double  value = 0;
    QColor  color;
};
typedef QVector<ChartItem> ChartData;

class ChartBase : public QWidget
{
    Q_OBJECT
public:
    explicit ChartBase(QWidget *parent = nullptr);
    void setData(const ChartData &data);
    QSize minimumSizeHint() const override { return QSize(200, 120); }
protected:
    ChartData m_data;
};

// Donut avec total au centre + legende (nom, effectif, pourcentage)
class PieChartWidget : public ChartBase
{
    Q_OBJECT
public:
    explicit PieChartWidget(QWidget *parent = nullptr) : ChartBase(parent) {}
    void setCenterCaption(const QString &c) { m_caption = c; update(); }
protected:
    void paintEvent(QPaintEvent *) override;
private:
    QString m_caption = "Employés";
};

// Histogramme
class BarChartWidget : public ChartBase
{
    Q_OBJECT
public:
    explicit BarChartWidget(QWidget *parent = nullptr) : ChartBase(parent) {}
protected:
    void paintEvent(QPaintEvent *) override;
};

// ===========================================================================
//  Liste deroulante a choix multiples (anciennement checkcombobox.h / .cpp)
//  utilisee pour "Equipe(s)"
// ===========================================================================
class CheckComboBox : public QComboBox
{
    Q_OBJECT
public:
    explicit CheckComboBox(QWidget *parent = nullptr);
    void        setItems(const QStringList &items);
    QStringList checkedItems() const;
    void        setCheckedItems(const QStringList &items);   // ajoute les valeurs inconnues
    void        clearChecked();
protected:
    bool eventFilter(QObject *obj, QEvent *ev) override;
private:
    QStandardItemModel *m_model;
    void updateText();
    void addItem_(const QString &t);
};

// ===========================================================================
//  Fenetre principale (anciennement GSmartHackathon)
// ===========================================================================
// Phase "interface graphique" (un seul fichier .ui, vues = pages de QStackedWidget) :
//  - seuls les boutons de NAVIGATION fonctionnent :
//      "Se connecter"            : Connexion  -> application (Tableau de bord)
//      menu de gauche            : change de page dans "stackedPages"
//      "Deconnexion"             : application -> Connexion
//  - tous les boutons d'ACTION (Ajouter, Vider, Trier, Exporter, modifier, supprimer,
//    oeil du mot de passe, mot de passe oublie, creer un compte...) sont uniquement
//    visuels : aucun traitement n'est branche dessus.
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;
    QCheckBox *m_headCheck = nullptr;
    ParticipantsController *m_participants = nullptr;   // logique de la page Participants

    void setupNavigation();     // boutons de navigation -> pages des QStackedWidget
    void showConnexion();       // page Connexion (fenetre 960 x 640)
    void showApplication();     // application (menu + pages), fenetre agrandie
    void setupDecor();          // donnees d'exemple affichees (tableau, graphiques)
    void positionHeaderCheck(); // placement de la case de l'en-tete (affichage uniquement)
};

#endif // MAINWINDOW_H
