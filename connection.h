#ifndef CONNECTION_H
#define CONNECTION_H

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

/**
 * @brief La classe Connection gère la connexion à la base de données
 * pour l'application Smart Hackathon / HackFlow.
 *
 * Elle est configurée pour fonctionner immédiatement en SQLite (100% portable sans
 * serveur externe) tout en intégrant les paramètres de connexion pour la base Oracle
 * (QODBC / QOCI) spécifiée dans le cahier des charges universitaire.
 */
class Connection
{
private:
    QSqlDatabase db;
    bool isOracle;
    QString m_lastError;

    // Configuration Oracle lue depuis config.local.ini (fichier local ignore par Git)
    bool configurerOracle();
    bool initialiserSqlite();
    static QString cheminFichierConfig();
    static QString cheminBaseSqlite();
    void definirErreur(const QString &message);

public:
    Connection();
    ~Connection();

    // Le moteur est choisi explicitement par HACKFLOW_DATABASE, puis par
    // [database]/engine dans config.local.ini (sqlite ou oracle).
    bool createConnection();
    void closeConnection();

    // Initialisation du schéma et des données de démonstration
    bool initializeDatabase();
    bool initializeOracleDatabase();
    void seedSampleData();
    void seedSessions();

    bool isOpen() const { return db.isOpen(); }
    QString getLastError() const;
};

#endif // CONNECTION_H
