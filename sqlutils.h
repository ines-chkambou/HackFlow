#ifndef SQLUTILS_H
#define SQLUTILS_H

#include <QDebug>
#include <QMessageBox>
#include <QSqlError>
#include <QString>
#include <QtGlobal>

/**
 * @brief Signale une erreur SQL à l'utilisateur et dans le journal de débogage.
 *
 * Les méthodes métier restent utilisables sans interface (retour booléen / vecteur vide),
 * mais une erreur SQL ne doit jamais être ignorée silencieusement.
 */
inline void signalerErreurSql(const QString &contexte, const QSqlError &erreur)
{
    const QString detail = erreur.text().isEmpty()
                               ? QStringLiteral("Erreur SQL inconnue.")
                               : erreur.text();
    qCritical().noquote() << contexte << ":" << detail;
    // Les harnais de test peuvent supprimer la boîte modale : le détail reste dans qCritical.
    if (!qEnvironmentVariableIsSet("HACKFLOW_SUPPRESS_SQL_DIALOGS"))
        QMessageBox::critical(nullptr, QStringLiteral("Erreur SQL"),
                              contexte + QStringLiteral("\n\n") + detail);
}

inline void signalerErreurSql(const QString &contexte, const QString &detail)
{
    const QString texte = detail.isEmpty() ? QStringLiteral("Erreur inconnue.") : detail;
    qCritical().noquote() << contexte << ":" << texte;
    if (!qEnvironmentVariableIsSet("HACKFLOW_SUPPRESS_SQL_DIALOGS"))
        QMessageBox::critical(nullptr, QStringLiteral("Erreur SQL"),
                              contexte + QStringLiteral("\n\n") + texte);
}

#endif // SQLUTILS_H
