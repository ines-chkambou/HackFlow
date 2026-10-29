#include "chartwidgets.h"
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QFont>
#include <cmath>

DonutChartWidget::DonutChartWidget(QWidget *parent)
    : QWidget(parent), centerLabel("Jury(s)"), totalCount(0)
{
    setFixedSize(110, 110);
    setAttribute(Qt::WA_TranslucentBackground);
}

void DonutChartWidget::setSlices(const QVector<QPair<QColor, int>> &newSlices, const QString &label)
{
    slices = newSlices;
    centerLabel = label;
    totalCount = 0;
    for (const auto &s : slices) totalCount += s.second;
    update();
}

void DonutChartWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const double epaisseur = 14.0;                       // épaisseur de l'anneau
    const QRectF zone = QRectF(rect()).adjusted(epaisseur / 2 + 1, epaisseur / 2 + 1,
                                                -epaisseur / 2 - 1, -epaisseur / 2 - 1);

    // Piste de fond (anneau vide)
    p.setPen(QPen(QColor("#0C2A63"), epaisseur, Qt::SolidLine, Qt::FlatCap));
    p.drawArc(zone, 0, 360 * 16);

    if (totalCount > 0) {
        // Départ en haut (90°), sens horaire, petit espace entre les segments
        double angleDebut = 90.0;
        const double espace = (slices.size() > 1) ? 3.0 : 0.0;
        for (const auto &s : slices) {
            if (s.second <= 0) continue;
            const double etendue = 360.0 * s.second / totalCount;
            p.setPen(QPen(s.first, epaisseur, Qt::SolidLine, Qt::FlatCap));
            p.drawArc(zone, int((angleDebut - espace / 2) * 16), int(-(etendue - espace) * 16));
            angleDebut -= etendue;
        }
    }

    // Texte central : total + libellé
    p.setPen(QColor("#FFFFFF"));
    QFont fTotal("Segoe UI", 1);
    fTotal.setPixelSize(26);
    fTotal.setBold(true);
    p.setFont(fTotal);
    p.drawText(QRectF(0, height() / 2.0 - 24, width(), 28), Qt::AlignCenter, QString::number(totalCount));

    p.setPen(QColor("#B8C7E8"));
    QFont fLabel("Segoe UI", 1);
    fLabel.setPixelSize(11);
    p.setFont(fLabel);
    p.drawText(QRectF(0, height() / 2.0 + 4, width(), 16), Qt::AlignCenter, centerLabel);
}

BarChartWidget::BarChartWidget(QWidget *parent)
    : QWidget(parent), unit("")
{
    setMinimumSize(280, 180);
    // Dégradés fluo / néon
    barGradients.append(qMakePair(QColor("#0BF8F6"), QColor("#0284C7"))); // Cyan -> Bleu
    barGradients.append(qMakePair(QColor("#818CF8"), QColor("#4F46E5"))); // Indigo -> Violet
    barGradients.append(qMakePair(QColor("#FBBF24"), QColor("#D97706"))); // Ambre -> Orange
    barGradients.append(qMakePair(QColor("#34D399"), QColor("#059669"))); // Vert menthe
}

void BarChartWidget::setData(const QMap<QString, double> &newData, const QString &unitLabel)
{
    data = newData;
    unit = unitLabel;
    update();
}

void BarChartWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    if (data.isEmpty()) {
        p.setPen(QColor("#8E9CAE"));
        p.drawText(rect(), Qt::AlignCenter, "Aucune donnée disponible");
        return;
    }

    double maxVal = 0.0;
    for (double val : data) {
        if (val > maxVal) maxVal = val;
    }
    if (maxVal <= 0.0) maxVal = 5.0;
    // Arrondir le max à l'entier supérieur
    int ceilingMax = (int)std::ceil(maxVal);
    if (ceilingMax % 2 != 0) ceilingMax++;

    int leftMargin = 35;
    int rightMargin = 15;
    int topMargin = 25;
    int bottomMargin = 35;

    int plotWidth = width() - leftMargin - rightMargin;
    int plotHeight = height() - topMargin - bottomMargin;

    // Lignes de grille horizontales
    int gridLines = 4;
    p.setFont(QFont("Segoe UI", 8));

    for (int i = 0; i <= gridLines; ++i) {
        int y = topMargin + plotHeight - (i * plotHeight / gridLines);
        double gridVal = (double)i * ceilingMax / gridLines;

        p.setPen(QPen(QColor("#233354"), 1, Qt::DashLine));
        p.drawLine(leftMargin, y, width() - rightMargin, y);

        p.setPen(QColor("#8E9CAE"));
        p.drawText(5, y + 4, 25, 12, Qt::AlignRight, QString::number((int)gridVal));
    }

    // Barres
    int barCount = data.size();
    if (barCount == 0) return;

    int slotWidth = plotWidth / barCount;
    int barWidth = qMin(36, (int)(slotWidth * 0.55));
    int idx = 0;

    for (auto it = data.begin(); it != data.end(); ++it) {
        int barCenterX = leftMargin + (idx * slotWidth) + (slotWidth / 2);
        int barX = barCenterX - (barWidth / 2);
        double ratio = it.value() / ceilingMax;
        int barH = (int)(plotHeight * ratio);
        int barY = topMargin + plotHeight - barH;

        auto gradColors = barGradients[idx % barGradients.size()];
        QLinearGradient grad(barX, barY, barX, barY + barH);
        grad.setColorAt(0.0, gradColors.first);
        grad.setColorAt(1.0, gradColors.second);

        // Dessiner la barre avec coins supérieurs arrondis
        QPainterPath path;
        path.moveTo(barX, barY + barH);
        path.lineTo(barX, barY + 4);
        path.quadTo(barX, barY, barX + 4, barY);
        path.lineTo(barX + barWidth - 4, barY);
        path.quadTo(barX + barWidth, barY, barX + barWidth, barY + 4);
        path.lineTo(barX + barWidth, barY + barH);
        path.closeSubpath();

        p.setPen(Qt::NoPen);
        p.setBrush(grad);
        p.drawPath(path);

        // Valeur affichée au-dessus de la barre
        p.setPen(Qt::white);
        p.setFont(QFont("Segoe UI", 9, QFont::Bold));
        QString valStr = (std::floor(it.value()) == it.value()) ? QString::number((int)it.value()) : QString::number(it.value(), 'f', 1);
        p.drawText(barX - 10, barY - 18, barWidth + 20, 16, Qt::AlignCenter, valStr);

        // Nom sous la barre
        p.setPen(QColor("#A0AEC0"));
        p.setFont(QFont("Segoe UI", 8));
        QString label = it.key();
        if (label.length() > 10) label = label.left(9) + ".";
        p.drawText(barX - 15, topMargin + plotHeight + 6, barWidth + 30, 24, Qt::AlignHCenter | Qt::AlignTop, label);

        idx++;
    }
}
