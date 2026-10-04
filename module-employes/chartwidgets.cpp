#include "chartwidgets.h"
#include <QPainter>
#include <QPainterPath>
#include <QtMath>

static const QColor kText("#FFFFFF");
static const QColor kMuted("#AEBBDD");
static const QColor kGrid("#2B3A5E");
static const QColor kCard("#1C2740");

ChartBase::ChartBase(QWidget *parent) : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void ChartBase::setData(const ChartData &data)
{
    m_data = data;
    update();
}

void PieChartWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    double total = 0;
    for (const auto &d : m_data) total += d.value;
    if (total <= 0) {
        p.setPen(kText);
        p.drawText(rect(), Qt::AlignCenter, tr("Aucune donnée"));
        return;
    }
    const int side = qMin(height() - 8, int(width() * 0.42));
    QRectF pie(6, (height() - side) / 2.0, side, side);
    double start = 90 * 16;
    QVector<double> mids;
    for (const auto &d : m_data) {
        double span = -360.0 * 16 * d.value / total;
        p.setPen(QPen(kCard, 2));
        p.setBrush(d.color);
        p.drawPie(pie, int(start), int(span));
        mids << (start + span / 2) / 16.0;
        start += span;
    }
    // trou du donut
    const double hole = side * 0.56;
    QRectF inner(pie.center().x() - hole / 2, pie.center().y() - hole / 2, hole, hole);
    p.setPen(Qt::NoPen);
    p.setBrush(kCard);
    p.drawEllipse(inner);
    // pourcentages sur les parts
    QFont f = font(); f.setBold(true); f.setPointSizeF(f.pointSizeF() - 1); p.setFont(f);
    p.setPen(QColor("#101B31"));
    for (int i = 0; i < m_data.size(); ++i) {
        double rad = qDegreesToRadians(mids[i]);
        double r = (side / 2.0 + hole / 2) / 2.0;
        QPointF c = pie.center() + QPointF(qCos(rad), -qSin(rad)) * r;
        p.drawText(QRectF(c.x() - 24, c.y() - 9, 48, 18), Qt::AlignCenter,
                   QString("%1%").arg(qRound(100.0 * m_data[i].value / total)));
    }
    // total au centre
    p.setPen(kText);
    QFont big = font(); big.setPointSize(20); big.setBold(true); p.setFont(big);
    p.drawText(QRectF(inner.left(), inner.center().y() - 26, inner.width(), 30), Qt::AlignCenter, QString::number(int(total)));
    QFont sm = font(); sm.setPointSizeF(8.5); sm.setBold(false); p.setFont(sm);
    p.setPen(kMuted);
    p.drawText(QRectF(inner.left(), inner.center().y() + 4, inner.width(), 16), Qt::AlignCenter, m_caption);

    // legende
    QFont lf = font(); lf.setBold(false); p.setFont(lf);
    const int x = int(pie.right()) + 26;
    const int rowH = 28;
    int y = qMax(6, height() / 2 - m_data.size() * rowH / 2);
    const int pctX = qMax(x + 150, width() - 50);
    for (int i = 0; i < m_data.size(); ++i) {
        p.setBrush(m_data[i].color); p.setPen(Qt::NoPen);
        p.drawRoundedRect(x, y + i * rowH + 4, 13, 13, 3, 3);
        p.setPen(kText);
        p.drawText(x + 22, y + i * rowH + 16, QString("%1 (%2)").arg(m_data[i].label).arg(int(m_data[i].value)));
        p.setPen(kMuted);
        p.drawText(QRectF(pctX, y + i * rowH + 2, 44, 18), Qt::AlignRight | Qt::AlignVCenter,
                   QString("%1%").arg(qRound(100.0 * m_data[i].value / total)));
    }
}

void BarChartWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    if (m_data.isEmpty()) {
        p.setPen(kText);
        p.drawText(rect(), Qt::AlignCenter, tr("Aucune donnée"));
        return;
    }
    double maxV = 1;
    for (const auto &d : m_data) maxV = qMax(maxV, d.value);
    const int maxTick = int(qCeil(maxV));
    const int left = 30, bottom = 24, top = 22, right = 8;
    QRectF plot(left, top, width() - left - right, height() - top - bottom);

    p.setFont(font());
    for (int t = 0; t <= maxTick; ++t) {
        double y = plot.bottom() - plot.height() * t / maxTick;
        p.setPen(QPen(kGrid, 1, Qt::DotLine));
        p.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
        p.setPen(kMuted);
        p.drawText(QRectF(0, y - 9, left - 6, 18), Qt::AlignRight | Qt::AlignVCenter, QString::number(t));
    }
    const double slot = plot.width() / m_data.size();
    const double bw = qMin(56.0, slot * 0.55);
    for (int i = 0; i < m_data.size(); ++i) {
        double h = plot.height() * m_data[i].value / maxTick;
        QRectF r(plot.left() + slot * i + (slot - bw) / 2, plot.bottom() - h, bw, h);
        QPainterPath path;
        path.addRoundedRect(r, 4, 4);
        p.setPen(Qt::NoPen);
        QLinearGradient g(r.topLeft(), r.bottomLeft());
        g.setColorAt(0, m_data[i].color);
        g.setColorAt(1, m_data[i].color.darker(150));
        p.setBrush(g);
        p.drawPath(path);
        QFont f = font(); f.setBold(true); p.setFont(f);
        p.setPen(kText);
        p.drawText(QRectF(r.left() - 10, r.top() - 19, bw + 20, 16), Qt::AlignCenter, QString::number(int(m_data[i].value)));
        f.setBold(false); p.setFont(f);
        p.setPen(kMuted);
        QString lbl = p.fontMetrics().elidedText(m_data[i].label, Qt::ElideRight, int(slot) - 4);
        p.drawText(QRectF(plot.left() + slot * i, plot.bottom() + 4, slot, 18), Qt::AlignCenter, lbl);
    }
}
