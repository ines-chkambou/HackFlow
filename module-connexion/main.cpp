#include "mainwindow.h"
#include <QApplication>
#include <QIcon>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setApplicationName("HackFlow");
    a.setWindowIcon(QIcon(":/img/logo_full.png"));
    MainWindow w;
    w.show();
    return a.exec();
}
