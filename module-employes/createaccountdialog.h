#ifndef CREATEACCOUNTDIALOG_H
#define CREATEACCOUNTDIALOG_H

#include <QDialog>
#include <QDateTime>
#include <QFutureWatcher>
#include "mailer.h"

class QStackedWidget;
class QLineEdit;
class QLabel;
class QPushButton;
class QCheckBox;

// "Creer un compte" : inscription en libre-service.
//   etape 1 : nom, e-mail, mot de passe (+ confirmation, regles de securite)
//   etape 2 : verification de l'adresse e-mail par un code a 6 chiffres envoye par e-mail
//             (uniquement si le SMTP est configure ; sinon le compte est cree directement)
// Le compte cree a le role "Visiteur" (acces minimal) ; l'administrateur peut ensuite
// lui attribuer un autre role (menu Admin > Gerer les utilisateurs).
class CreateAccountDialog : public QDialog
{
    Q_OBJECT
public:
    explicit CreateAccountDialog(QWidget *parent = nullptr);
    QString email() const { return m_email; }
    bool verified() const { return m_verified; }   // true = adresse e-mail confirmee par code

private slots:
    void onCreate();
    void onMailFinished();
    void onVerify();

private:
    void buildUi();
    void showStep(int i);
    bool finishCreation();

    QStackedWidget *m_stack = nullptr;
    QLineEdit *m_name, *m_mail, *m_pw, *m_pw2, *m_code;
    QLabel *m_err1, *m_err2, *m_info2;
    QPushButton *m_btnCreate, *m_btnVerify;
    QCheckBox *m_show;
    QFutureWatcher<MailResult> m_watcher;

    QString m_email, m_nameVal, m_pwVal;
    QByteArray m_codeHash, m_codeSalt;
    QDateTime m_expiry;
    int m_attempts = 0;
    bool m_verified = false;
};

#endif
