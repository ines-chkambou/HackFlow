#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QDialog>
#include <QComboBox>
#include <QStringList>
#include <QWidget>
#include <QVector>
#include <QString>
#include <QColor>
#include <QFont>
#include <QSize>
#include <QDateTime>
#include <QFutureWatcher>

#include "employe.h"

class QCheckBox;
class QMenu;
class QAction;
class QStandardItemModel;
class QTableWidget;
class QLineEdit;
class QLabel;
class QSpinBox;
class QPushButton;
class QStackedWidget;
class QTimer;

class ParticipantsController;   // module Participants
class JuryController;           // module Jury
class ProjectsPage;             // module Gestion des projets
namespace Ui { class MainWindow; }

// ===========================================================================
//  Graphiques (anciennement chartwidgets)
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

// Donut avec total au centre + légende
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
//  Liste déroulante à choix multiples (anciennement checkcombobox)
// ===========================================================================
class CheckComboBox : public QComboBox
{
    Q_OBJECT
public:
    explicit CheckComboBox(QWidget *parent = nullptr);
    void        setItems(const QStringList &items);
    QStringList checkedItems() const;
    void        setCheckedItems(const QStringList &items);
    void        clearChecked();
protected:
    bool eventFilter(QObject *obj, QEvent *ev) override;
private:
    QStandardItemModel *m_model;
    void updateText();
    void addItem_(const QString &t);
};

// ===========================================================================
//  Page "Choix du rôle" (entre Connexion et Application)
// ===========================================================================
class RolesBackdrop : public QWidget
{
    Q_OBJECT
public:
    explicit RolesBackdrop(QWidget *parent = nullptr) : QWidget(parent) {}
protected:
    void paintEvent(QPaintEvent *) override;
};

class GradientTitle : public QWidget
{
    Q_OBJECT
public:
    explicit GradientTitle(QWidget *parent = nullptr);
    void  setParts(const QString &plain, const QString &accent);
    void  setAlignment(Qt::Alignment a) { m_align = a; update(); }
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override { return sizeHint(); }
protected:
    void paintEvent(QPaintEvent *) override;
private:
    QString m_plain, m_accent;
    Qt::Alignment m_align = Qt::AlignHCenter;
    QFont   titleFont() const;
};

// ===========================================================================
//  BOÎTES DE DIALOGUE EMPLOYÉ (Intégrées directement ici)
// ===========================================================================

// 1. Mot de passe oublié (mailing en direct via SMTP Gmail)
class ForgotPasswordDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ForgotPasswordDialog(const QString &prefillEmail = QString(), QWidget *parent = nullptr);
    QString email() const { return m_email; }

private slots:
    void onSendCode();
    void onMailFinished();
    void onReset();
    void updateResendButton();

private:
    void buildUi();
    void showStep(int i);
    void setBusy(bool b);

    QStackedWidget *m_stack = nullptr;
    QLineEdit *m_lineEmail = nullptr, *m_lineCode = nullptr, *m_linePw = nullptr, *m_linePw2 = nullptr;
    QLabel *m_err1 = nullptr, *m_err2 = nullptr, *m_info2 = nullptr, *m_pwHint = nullptr;
    QPushButton *m_btnSend = nullptr, *m_btnReset = nullptr, *m_btnResend = nullptr;
    QCheckBox *m_showPw = nullptr;
    QTimer *m_cooldownTimer = nullptr;
    QFutureWatcher<MailResult> m_watcher;

    QString    m_email;
    QByteArray m_codeHash, m_codeSalt;
    QDateTime  m_codeExpiry;
    QDateTime  m_nextSendAllowed;
    int        m_attempts = 0;
    bool       m_pendingKnownUser = false;
};

// 2. Création de compte (avec accès complet par défaut)
class CreateAccountDialog : public QDialog
{
    Q_OBJECT
public:
    explicit CreateAccountDialog(QWidget *parent = nullptr);
    QString email() const { return m_email; }
    bool verified() const { return m_verified; }

private slots:
    void onCreate();
    void onMailFinished();
    void onVerify();

private:
    void buildUi();
    void showStep(int i);
    bool finishCreation();

    QStackedWidget *m_stack = nullptr;
    QLineEdit *m_name = nullptr, *m_mail = nullptr, *m_pw = nullptr, *m_pw2 = nullptr, *m_code = nullptr;
    QLabel *m_err1 = nullptr, *m_err2 = nullptr, *m_info2 = nullptr;
    QPushButton *m_btnCreate = nullptr, *m_btnVerify = nullptr;
    QCheckBox *m_show = nullptr;
    QFutureWatcher<MailResult> m_watcher;

    QString    m_email, m_nameVal, m_pwVal;
    QByteArray m_codeHash, m_codeSalt;
    QDateTime  m_expiry;
    int        m_attempts = 0;
    bool       m_verified = false;
};

// 3. Gestion des comptes et privilèges
class UserManagementDialog : public QDialog
{
    Q_OBJECT
public:
    explicit UserManagementDialog(QWidget *parent = nullptr);
private:
    void refresh();
    void updatePrivList();
    QString selectedEmail() const;
    QTableWidget *m_table = nullptr;
    QLineEdit *m_email = nullptr, *m_name = nullptr, *m_pw = nullptr;
    QComboBox *m_role = nullptr;
    QLabel *m_privs = nullptr, *m_msg = nullptr;
};

// 4. Configuration SMTP
class SmtpSettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SmtpSettingsDialog(const QString &testRecipient, QWidget *parent = nullptr);
private:
    MailConfig readForm() const;
    QLineEdit *m_host = nullptr, *m_user = nullptr, *m_pass = nullptr, *m_from = nullptr;
    QSpinBox *m_port = nullptr;
    QComboBox *m_sec = nullptr;
    QLabel *m_msg = nullptr;
    QPushButton *m_btnTest = nullptr;
    QString m_testTo;
    QFutureWatcher<MailResult> m_watcher;
};

// 5. Attribution automatique intelligente (CV)
struct EmployeeInfo {
    QString id, nom, prenom, email, role, dept, dispo;
    QStringList teams;
};

class CvAssignDialog : public QDialog
{
    Q_OBJECT
public:
    CvAssignDialog(const EmployeeInfo &emp, const QMap<QString, int> &teamLoad,
                   int maxTeamSize, QWidget *parent = nullptr);
    QString chosenTeam() const { return m_chosen; }
    void loadCvFile(const QString &path);

private slots:
    void onBrowse();
    void onConfirm();

private:
    void setStatus(const QString &text, const char *kind);
    void showAnalysis(const CvAnalysis &a);

    EmployeeInfo m_emp;
    QMap<QString, int> m_load;
    int m_max = 0;
    QString m_chosen;

    QLabel *m_cvName = nullptr, *m_status = nullptr, *m_suggest = nullptr;
    QTableWidget *m_skills = nullptr, *m_teams = nullptr;
    QComboBox *m_combo = nullptr;
    QPushButton *m_btnOk = nullptr;
};

// ===========================================================================
//  FENÊTRE PRINCIPALE (MAINWINDOW)
// ===========================================================================
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;
    QCheckBox *m_headCheck = nullptr;
    ParticipantsController *m_participants = nullptr;
    JuryController *m_jury = nullptr;
    ProjectsPage *m_projects = nullptr;

    enum class Role { Participant, Jury, Organisateur, Mentor, Technicien };
    Role m_role = Role::Organisateur;

    void setupNavigation();
    void setupRoleMenu();
    void showConnexion();
    void showRoles();
    void showApplication(Role role);
    void applyRole(Role role);
    void enterAsEmploye();
    void setupDecor();
    void positionHeaderCheck();

    // Authentification / Privilèges
    void setupAuth();
    void doLogin();
    void doLogout();
    void applySession(const QString &profile);
    void applyPrivileges();

    // CRUD & Métier Employés (Oracle)
    Employe m_employeModel;
    int m_editingEmployeId = -1;
    QPushButton *m_btnSupprimerForm = nullptr;
    void setupEmployeCrud();
    void onAjouterEmploye();
    void onViderEmploye();
    void onSupprimerEmploye(int id);
    void onModifierEmploye(int id);
    void onFilterEmployes();
    void onTrierEmploye();
    void onExporterEmploye();
    void refreshEmployesTable(QSqlQueryModel *customModel = nullptr);
    void updateEmployeStats();
    void fillEmployeForm(int row);

    // Attribution automatique intelligente
    void onAssignToTeam();

    QMenu *m_adminMenu = nullptr;
    QAction *m_actWho = nullptr, *m_actUsers = nullptr, *m_actSmtp = nullptr, *m_actLogout = nullptr;
};

#endif // MAINWINDOW_H
