#include "gjury.h"
#include "connection.h"
#include <QApplication>
#include <QMessageBox>
#include <QIcon>

int main(int argc, char *argv[])
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif

    QApplication a(argc, argv);
    a.setApplicationName("HackFlow");
    a.setOrganizationName("ChronoHack");
    a.setApplicationVersion("2.5");

    // Connexion à la base de données (SQLite immédiat ou Oracle selon configuration)
    Connection conn;
    if (!conn.createConnection(false)) { // passer true pour activer Oracle QODBC / QOCI
        QMessageBox::critical(nullptr, "Erreur Base de Données",
                              "Impossible de se connecter à la base de données :\n" + conn.getLastError());
        return -1;
    }

    GJury w;
    w.setWindowTitle("HackFlow — Smart Hackathon Management | Équipe ChronoHack (AU: 25-26)");
    w.show();

    return a.exec();
}
