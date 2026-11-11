#ifndef CHARTWIDGETS_H
#define CHARTWIDGETS_H

#include <QWidget>
#include <QMap>
#include <QVector>
#include <QColor>
#include <QPair>

/**
 * @brief Anneau de statistiques (donut) de la carte « Statistiques du jury ».
 * Anneau coloré avec le total au centre ; la légende est affichée à côté par l'interface.
 */
class DonutChartWidget : public QWidget
{
    Q_OBJECT
private:
    QVector<QPair<QColor, int>> slices;
    QString centerLabel;
    int totalCount;

public:
    explicit DonutChartWidget(QWidget *parent = nullptr);
    void setSlices(const QVector<QPair<QColor, int>> &newSlices, const QString &label = "Jury(s)");

protected:
    void paintEvent(QPaintEvent *event) override;
};

/**
 * @brief Widget personnalisé pour afficher un Bar Chart (diagramme en bâtons)
 * identique à celui de la maquette (Figure 10).
 * Barres aux coins arrondis, dégradés de couleurs cyan/violet/orange, étiquettes de valeurs.
 */
class BarChartWidget : public QWidget
{
    Q_OBJECT
private:
    QMap<QString, double> data;
    QVector<QPair<QColor, QColor>> barGradients;
    QString unit;

public:
    explicit BarChartWidget(QWidget *parent = nullptr);
    void setData(const QMap<QString, double> &newData, const QString &unitLabel = "");

protected:
    void paintEvent(QPaintEvent *event) override;
};

#endif // CHARTWIDGETS_H
