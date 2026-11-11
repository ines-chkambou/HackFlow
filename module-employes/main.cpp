#include "mainwindow.h"
#include "connection.h"
#include <QApplication>
#include <QIcon>
#include <QMessageBox>
#include <QDebug>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setApplicationName("HackFlow");
    a.setWindowIcon(QIcon(":/img/logo_full.png"));

    // Établissement de la connexion unique à la base de données (Singleton)
    Connection& c = Connection::createInstance();
    bool test = c.createConnection();

    if (test) {
        qDebug() << "Connexion BD établie avec succès.";
    } else {
        qDebug() << "Échec de connexion à la base de données Oracle.";
    }

    MainWindow w;
    w.show();
    return a.exec();
}
