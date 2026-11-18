#include <QApplication>
#include <QMessageBox>
#include <QSqlDatabase>
#include <QSqlError>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QSqlDatabase db = QSqlDatabase::addDatabase("QODBC", "hackflow_oracle");
    db.setDatabaseName("Source_Projet2A");
    db.setUserName("INES");
    db.setPassword("2025");
    if (!db.open()) {
        QMessageBox::critical(nullptr, QString::fromUtf8("Connexion Oracle"),
            QString::fromUtf8("Impossible de se connecter à Oracle via Source_Projet2A.\n\n") + db.lastError().text());
        return 1;
    }
    MainWindow w;
    w.show();
    const int result = app.exec();
    db.close();
    return result;
}
