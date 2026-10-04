#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QDateTime>
#include <QList>
#include <QPixmap>
#include <QString>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

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

class QStackedWidget;
class QFrame;
class QPushButton;
class QTimer;
class QAction;

struct Notification
{
    QString   titre;
    QString   texte;
    QDateTime date;
    bool      lue = false;
    int       participantId = -1;      // >0 : un clic ouvre la fiche du participant
};

class QLabel;
class QMenu;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

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
    void setupLogo();
    void setupNavigation();
    void setupIcons();
    void setupNotifications();
    void addNotification(const QString &titre, const QString &texte, int participantId = -1);
    void updateNotifBadge();
    void showNotifications();
    void showToast(const QString &titre, const QString &texte);
    void saveNotifs() const;
    void loadNotifs();
    static QString notifFilePath();
    void setupSortMenu();
    void updateSortUi();
    QWidget *makePage(const QString &icon, const QString &title,
                      const QString &accent, const QString &subtitle);
    void applyStyleSheet();
    void setupCombos();
    void setupTable();
    bool setPdfAttachment(const QString &path, bool detectTeam = true);
    void detectTeamFromPdf(const QString &path);   // lit le CV et sélectionne l'équipe
    void clearPdfAttachment();
    void loadSampleData();
    bool loadData();            // lit participants.json (false si absent/invalide)
    void sendConfirmationSms(const Participant &p);   // point d'entrée pour un vrai service SMS
    void saveData() const;      // écrit participants.json
    static QString dataFilePath();
    void refreshTable();

    QList<Participant> filteredData() const;
    int  indexOfId(int id) const;

    QWidget *makeBadge(const QString &text, const QString &kind);
    QWidget *makeCheckCell();
    QWidget *makeActionsCell(int id);
    QPixmap  makeAvatar(const QString &initials, int colorIndex, int size = 34) const;

    Ui::MainWindow     *ui;
    QStackedWidget     *m_stack = nullptr;
    QList<Notification> m_notifs;
    QLabel             *m_notifBadge = nullptr;
    QFrame             *m_notifPanel = nullptr;   // panneau intégré à la fenêtre (pas de fenêtre flottante)
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
    QList<QAction *>    m_sortKeyActions;     // ID, Nom, Rôle, Statut
    QAction            *m_actAsc  = nullptr;
    QAction            *m_actDesc = nullptr;
};

#endif // MAINWINDOW_H
