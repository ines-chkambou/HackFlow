#ifndef CONNECTION_H
#define CONNECTION_H

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QDebug>
#include <QDir>
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

    // Configuration Oracle lue depuis config.local.ini (fichier local ignore par Git)
    bool configurerOracle();
    bool initialiserSqlite();
    static QString cheminFichierConfig();

public:
    Connection();
    ~Connection();

    bool createConnection(bool useOracle = false);
    void closeConnection();

    // Initialisation du schéma et des données de démonstration
    bool initializeDatabase();
    bool initializeOracleDatabase();
    void seedSampleData();
    void seedSampleDataOracle();
    void seedSessions();

    bool isOpen() const { return db.isOpen(); }
    QString getLastError() const { return db.lastError().text(); }
};

#endif // CONNECTION_H
