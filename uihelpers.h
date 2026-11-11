#ifndef UIHELPERS_H
#define UIHELPERS_H

// Petits éléments graphiques partagés par les pages « Paramètres du jury » et « Planning des évaluations ».

#include <QColor>
#include <QFontMetrics>
#include <QHeaderView>
#include <QIcon>
#include <QMap>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QString>
#include <QStringList>

namespace UiHelpers {

// Couleurs d'avatars (ordre de la maquette)
inline const QStringList &couleursAvatars()
{
    static const QStringList l = {"#EC4899", "#14B8A6", "#F9A825", "#3B9AF5", "#3F6FF2",
                                  "#1FA8F5", "#E8468D", "#22D3D3", "#F6A632", "#22C7E8"};
    return l;
}

inline QString couleurAvatar(int n)
{
    const QStringList &l = couleursAvatars();
    return l.at(((n % l.size()) + l.size()) % l.size());
}

inline QString initiales(const QString &nom)
{
    const QStringList mots = nom.simplified().split(' ');
    if (mots.size() >= 2) return (mots.at(0).left(1) + mots.at(1).left(1)).toUpper();
    return nom.simplified().left(2).toUpper();
}

inline int sommeCaracteres(const QString &s)
{
    int h = 0;
    for (const QChar &c : s) h += c.unicode();
    return h;
}

inline const QString &cssCheckbox()
{
    static const QString css =
        "QCheckBox { background: transparent; spacing: 0px; } "
        "QCheckBox::indicator { width: 18px; height: 18px; border: 1.5px solid #AFC4E8; border-radius: 4px; background: transparent; } "
        "QCheckBox::indicator:hover { border-color: #19C3F0; } "
        "QCheckBox::indicator:checked { background-color: #1E90FF; border-color: #1E90FF; image: url(:/img/check.png); }";
    return css;
}

inline const QString &cssMenu()
{
    static const QString css =
        "QMenu { background-color: #021B4A; color: #E6EEFF; border: 1px solid #1F57A4; border-radius: 6px; padding: 4px; } "
        "QMenu::item { padding: 7px 22px 7px 26px; border-radius: 4px; } "
        "QMenu::item:selected { background-color: #0B6BFF; } "
        "QMenu::separator { height: 1px; background: #1F57A4; margin: 4px 6px; }";
    return css;
}

inline QPushButton *boutonAction(const QString &icone, const QString &info)
{
    QPushButton *b = new QPushButton();
    b->setFixedSize(24, 24);
    b->setIcon(QIcon(":/img/" + icone));
    b->setIconSize(QSize(20, 20));
    b->setFlat(true);
    b->setCursor(Qt::PointingHandCursor);
    b->setToolTip(info);
    b->setStyleSheet("QPushButton { background: transparent; border: none; } "
                     "QPushButton:hover { background-color: rgba(255, 255, 255, 0.10); border-radius: 4px; } "
                     "QPushButton:disabled { background: transparent; }");
    return b;
}

// En-tête de tableau dessiné à la main : barre arrondie, case « tout sélectionner » en colonne 0,
// petite icône de tri sur les colonnes triables.
class EnteteTri : public QHeaderView
{
public:
    explicit EnteteTri(QWidget *parent) : QHeaderView(Qt::Horizontal, parent) {}

    QList<int> triables;          // colonnes avec icône de tri
    int colonneActive = -1;       // colonne actuellement triée (texte cyan)
    bool toutCoche = false;       // état de la case de la colonne 0
    QMap<int, int> margesGauche;  // marge gauche du texte par colonne (4 px par défaut)

protected:
    void paintSection(QPainter *p, const QRect &r, int index) const override
    {
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        p->setClipRect(r);
        const int n = count();
        QRect fond = r;
        if (index > 0) fond.adjust(-12, 0, 0, 0);
        if (index < n - 1) fond.adjust(0, 0, 12, 0);
        p->setPen(Qt::NoPen);
        p->setBrush(QColor("#05215A"));
        p->drawRoundedRect(fond, 8, 8);

        if (index == 0) {
            const QRect boite(r.center().x() - 9, r.center().y() - 9, 18, 18);
            QPen stylo(toutCoche ? QColor("#1E90FF") : QColor("#AFC4E8"));
            stylo.setWidthF(1.5);
            p->setPen(stylo);
            p->setBrush(toutCoche ? QBrush(QColor("#1E90FF")) : Qt::NoBrush);
            p->drawRoundedRect(QRectF(boite).adjusted(0.75, 0.75, -0.75, -0.75), 4, 4);
            if (toutCoche) p->drawPixmap(boite.adjusted(3, 3, -3, -3), QPixmap(":/img/check.png"));
            p->restore();
            return;
        }

        const QString texte = model()->headerData(index, orientation(), Qt::DisplayRole).toString();
        QFont f = font();
        f.setBold(true);
        f.setPixelSize(12);
        p->setFont(f);
        p->setPen(index == colonneActive ? QColor("#19C3F0") : QColor("#E6EEFF"));
        const QRect zoneTexte = r.adjusted(margesGauche.value(index, 4), 0, -4, 0);
        p->drawText(zoneTexte, Qt::AlignLeft | Qt::AlignVCenter, texte);

        if (triables.contains(index)) {
            const int largeur = QFontMetrics(f).horizontalAdvance(texte);
            const QPixmap pm(":/img/sort_cyan.png");
            p->drawPixmap(QRect(zoneTexte.left() + largeur + 7, r.center().y() - 6, 12, 12), pm);
        }
        p->restore();
    }
};

} // namespace UiHelpers

#endif // UIHELPERS_H
