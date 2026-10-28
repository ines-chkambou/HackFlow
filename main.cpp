#include "mainwindow.h"
#include "connection.h"

#include <QApplication>
#include <QMessageBox>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setApplicationName("Participants");

    // Singleton de Meyers : connexion unique à la base (Oracle via ODBC)
    Connection &c = Connection::createInstance();
    if (!c.createConnection()) {
        QMessageBox::critical(nullptr, QObject::tr("Base de données"),
                              QObject::tr("Échec de la connexion.\nCliquez sur Annuler pour quitter."),
                              QMessageBox::Cancel);
        return -1;
    }

    MainWindow w;      // créé après la connexion : le constructeur lit la table PARTICIPANT
    w.show();
    return a.exec();
}
