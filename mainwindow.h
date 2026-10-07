#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_btnDashboard_clicked();
    void on_btnParticipants_clicked();
    void on_btnProjets_clicked();
    void on_btnEmployes_clicked();
    void on_btnJury_clicked();

private:
    void appliquerStyle();
    Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H
