#include "connection.h"

// Ctor
// Initialise l'attribut `db` avec le driver ODBC.
// On le fait dans le constructeur pour que `db` soit stocke dans le Singleton.
Connection::Connection() {
    db = QSqlDatabase::addDatabase("QODBC");
}

// Dtor
// Ferme la connexion si elle est encore ouverte lorsque l'objet est detruit
Connection::~Connection() {
    if (db.isOpen())
        db.close(); // Libere proprement les ressources
}

// --- Acces a l'instance unique du Singleton ---
Connection& Connection::createInstance() {
    static Connection instance;  // Instance unique du Singleton
    return instance; // Retourne une reference a cette instance
}

bool Connection::createConnection() {
    db.setDatabaseName("Source_Projet2A");
    db.setUserName("INES");
    db.setPassword("2025");

    if (db.open()) {
        qDebug() << "Connexion Singleton etablie (Source_Projet2A)";
        return true;
    }

    // Deuxieme tentative sur hackflow1 si Source_Projet2A echoue
    db.setDatabaseName("hackflow1");
    db.setUserName("ines");
    db.setPassword("2025");

    if (db.open()) {
        qDebug() << "Connexion Singleton etablie (hackflow1)";
        return true;
    }

    qDebug() << "Echec de la connexion Singleton :" << db.lastError().text();
    return false;
}
