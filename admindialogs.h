#ifndef ADMINDIALOGS_H
#define ADMINDIALOGS_H

#include <QDialog>
#include <QFutureWatcher>
#include "mailer.h"

class QTableWidget;
class QLineEdit;
class QComboBox;
class QLabel;
class QSpinBox;
class QPushButton;

// Gestion des comptes et des roles (reserve au privilege ManageUsers)
class UserManagementDialog : public QDialog
{
    Q_OBJECT
public:
    explicit UserManagementDialog(QWidget *parent = nullptr);
private:
    void refresh();
    void updatePrivList();
    QString selectedEmail() const;
    QTableWidget *m_table; QLineEdit *m_email, *m_name, *m_pw; QComboBox *m_role; QLabel *m_privs, *m_msg;
};

// Configuration du serveur SMTP pour l'envoi des e-mails (reserve au privilege ManageUsers)
class SmtpSettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SmtpSettingsDialog(const QString &testRecipient, QWidget *parent = nullptr);
private:
    MailConfig readForm() const;
    QLineEdit *m_host, *m_user, *m_pass, *m_from; QSpinBox *m_port; QComboBox *m_sec;
    QLabel *m_msg; QPushButton *m_btnTest; QString m_testTo;
    QFutureWatcher<MailResult> m_watcher;
};

#endif
