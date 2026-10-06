#ifndef PARTICIPANTSCONTROLLER_H
#define PARTICIPANTSCONTROLLER_H

// ===========================================================================
//  Module « Gestion des Participants » (integre dans HackFlow)
//  Les widgets sont dans mainwindow.ui (page « pageParticipants » du QStackedWidget
//  « stackedPages »). Cette classe contient toute la logique du module : ajout,
//  modification, suppression, filtres, tri, pagination, export CSV, piece jointe PDF
//  (detection de l'equipe), sauvegarde JSON et notifications (cloche « btnBell »).
//  Elle ne change rien aux autres pages de l'application.
// ===========================================================================

#include <QObject>
#include <QDateTime>
#include <QList>
#include <QMessageBox>
#include <QPixmap>
#include <QString>

class QMainWindow;
class QFrame;
class QLabel;
class QMenu;
class QAction;
class QPushButton;
class QTimer;
class QWidget;

namespace Ui { class MainWindow; }

struct Participant
{
    int     id = 0;
    QString nom;
    QString prenom;
    QString email;
    QString telephone;
    QString pieceJointe;
    QString role;
    QString equipe;
    QString dispo;
};

struct Notification
{
    QString   titre;
    QString   texte;
    QDateTime date;
    bool      lue = false;
    int       participantId = -1;      // >0 : un clic ouvre la fiche du participant
};

class ParticipantsController : public QObject
{
    Q_OBJECT

public:
    ParticipantsController(Ui::MainWindow *ui, QMainWindow *host);
    ~ParticipantsController() override;

protected:
    bool eventFilter(QObject *obj, QEvent *ev) override;

private slots:
    void onAjouter();
    void onVider();
    void onFiltersChanged();
    void onSortClicked();
    void onExporter();
    void onPrevPage();
    void onNextPage();
    void onHeaderClicked(int section);

    void onView(int id);
    void onEdit(int id);
    void onDelete(int id);

private:
    void setupIcons();
    void setupNotifications();
    void setupTopSearch();
    bool isParticipantsPageVisible() const;
    int  showMsg(QMessageBox::Icon icon, const QString &title, const QString &text);
    void addNotification(const QString &titre, const QString &texte, int participantId = -1);
    void updateNotifBadge();
    void showNotifications();
    void showToast(const QString &titre, const QString &texte);
    void saveNotifs() const;
    void loadNotifs();
    static QString notifFilePath();
    void setupSortMenu();
    void updateSortUi();
    void applyStyleSheet();
    void setupCombos();
    void setupTable();
    bool setPdfAttachment(const QString &path, bool detectTeam = true);
    void detectTeamFromPdf(const QString &path);   // lit le CV et selectionne l'equipe
    void clearPdfAttachment();
    void loadSampleData();
    bool loadData();            // lit participants.json (false si absent/invalide)
    void sendConfirmationSms(const Participant &p);   // point d'entree pour un vrai service SMS
    void saveData() const;      // ecrit participants.json
    static QString dataFilePath();
    void refreshTable();

    QList<Participant> filteredData() const;
    int  indexOfId(int id) const;

    QWidget *makeBadge(const QString &text, const QString &kind);
    QWidget *makeCheckCell();
    QWidget *makeActionsCell(int id);
    QPixmap  makeAvatar(const QString &initials, int colorIndex, int size = 34) const;

    Ui::MainWindow     *ui     = nullptr;
    QMainWindow        *m_host = nullptr;          // fenetre principale (parent des dialogues)
    QString             m_searchPlaceholder;       // texte d'origine de lineTopSearch
    QList<Notification> m_notifs;
    QLabel             *m_notifBadge = nullptr;
    QFrame             *m_notifPanel = nullptr;    // panneau integre a la fenetre (pas de fenetre flottante)
    QPushButton        *m_toast = nullptr;
    QTimer             *m_toastTimer = nullptr;
    QList<Participant>  m_data;
    int                 m_nextId   = 1;
    int                 m_editingId = -1;
    QString             m_pdfPath;
    int                 m_page     = 1;
    const int           m_pageSize = 10;
    enum class SortKey { None, Id, Nom, Role, Statut };
    SortKey             m_sortKey = SortKey::None;
    bool                m_sortAsc = true;
    QMenu              *m_sortMenu = nullptr;
    QList<QAction *>    m_sortKeyActions;          // ID, Nom, Role, Statut
    QAction            *m_actAsc  = nullptr;
    QAction            *m_actDesc = nullptr;
};

#endif // PARTICIPANTSCONTROLLER_H
