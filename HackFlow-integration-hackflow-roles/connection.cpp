#include "connection.h"

// Constructeur : initialise le driver ODBC
Connection::Connection() {
    db = QSqlDatabase::addDatabase("QODBC");
}

// Destructeur : libère la connexion
Connection::~Connection() {
    if (db.isOpen())
        db.close();
}

// Singleton de Meyers (thread-safe, instance unique)
Connection& Connection::createInstance() {
    static Connection instance;
    return instance;
}

// Établit la connexion à la base de données Oracle (DSN "test" sur hackflow1)
bool Connection::createConnection() {
    db.setDatabaseName("test"); // Nom de la source de données ODBC (DSN)
    db.setUserName("ines");     // Nom d'utilisateur Oracle
    db.setPassword("ines");     // Mot de passe utilisateur Oracle

    if (db.open()) {
        qDebug() << "Connexion BD Oracle réussie via ODBC (DSN test, User ines)";
        return true;
    }

    qDebug() << "Échec de la connexion BD Oracle :" << db.lastError().text();
    return false;
}
