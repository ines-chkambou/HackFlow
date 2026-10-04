#ifndef CHARTWIDGETS_H
#define CHARTWIDGETS_H

#include <QWidget>
#include <QVector>
#include <QString>
#include <QColor>

struct ChartItem {
    QString label;
    double  value = 0;
    QColor  color;
};
typedef QVector<ChartItem> ChartData;

class ChartBase : public QWidget
{
    Q_OBJECT
public:
    explicit ChartBase(QWidget *parent = nullptr);
    void setData(const ChartData &data);
    QSize minimumSizeHint() const override { return QSize(200, 120); }
protected:
    ChartData m_data;
};

// Donut avec total au centre + legende (nom, effectif, pourcentage)
class PieChartWidget : public ChartBase
{
    Q_OBJECT
public:
    explicit PieChartWidget(QWidget *parent = nullptr) : ChartBase(parent) {}
    void setCenterCaption(const QString &c) { m_caption = c; update(); }
protected:
    void paintEvent(QPaintEvent *) override;
private:
    QString m_caption = "Employés";
};

// Histogramme
class BarChartWidget : public ChartBase
{
    Q_OBJECT
public:
    explicit BarChartWidget(QWidget *parent = nullptr) : ChartBase(parent) {}
protected:
    void paintEvent(QPaintEvent *) override;
};

#endif // CHARTWIDGETS_H
