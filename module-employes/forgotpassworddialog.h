#ifndef FORGOTPASSWORDDIALOG_H
#define FORGOTPASSWORDDIALOG_H

#include <QDialog>
#include <QDateTime>
#include <QFutureWatcher>
#include "mailer.h"

class QStackedWidget;
class QLineEdit;
class QLabel;
class QPushButton;
class QCheckBox;
class QTimer;

// "Mot de passe oublie ?" :  1) saisie de l'e-mail  ->  code a 6 chiffres envoye par e-mail (valable 10 min)
//                            2) saisie du code + nouveau mot de passe  ->  mot de passe modifie
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

    QString   m_email;
    QByteArray m_codeHash, m_codeSalt;
    QDateTime m_codeExpiry;
    QDateTime m_nextSendAllowed;
    int       m_attempts = 0;
    bool      m_pendingKnownUser = false;
};

#endif // FORGOTPASSWORDDIALOG_H
