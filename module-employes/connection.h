#ifndef CONNECTION_H
#define CONNECTION_H

#include <QSqlDatabase>
#include <QDebug>
#include <QSqlError>

/* Classe Connection
 * Implémentation du pattern Singleton (Meyers Singleton) pour gérer une connexion unique à la base de données Oracle.
 * Utilise QSqlDatabase pour établir et fermer la connexion via le driver ODBC.
 */
class Connection {
public:
    static Connection& createInstance(); // Retourne une référence unique au Singleton
    bool createConnection();            // Établit la connexion à la base Oracle

private:
    QSqlDatabase db;

    Connection();
    ~Connection();

    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;
};

#endif // CONNECTION_H
