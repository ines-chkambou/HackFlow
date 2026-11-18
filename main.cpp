#include "mainwindow.h"
#include "connection.h"
#include <QApplication>
#include <QIcon>
#include <QSqlDatabase>
#include <QSqlError>
#include <QDebug>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setApplicationName("HackFlow");
    a.setWindowIcon(QIcon(":/img/logo_full.png"));

    // Connexion Oracle pour le module Projets (hackflow_oracle)
    QSqlDatabase db = QSqlDatabase::addDatabase("QODBC", "hackflow_oracle");
    db.setDatabaseName("Source_Projet2A");
    db.setUserName("INES");
    db.setPassword("2025");
    if (db.open()) {
        qDebug() << "Connexion Oracle (hackflow_oracle) etablie avec succes.";
    } else {
        qWarning() << "Oracle (hackflow_oracle) non connecte :" << db.lastError().text();
    }

    // Connexion unique globale (Singleton Connection)
    Connection& c = Connection::createInstance();
    c.createConnection();

    MainWindow w;
    w.show();
    const int result = a.exec();

    if (db.isOpen()) {
        db.close();
    }
    return result;
}
