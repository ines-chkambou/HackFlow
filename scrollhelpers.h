#ifndef SCROLLHELPERS_H
#define SCROLLHELPERS_H

// Petits outils pour un defilement fluide de la page Jury.
// (Fichier "header seulement" : aucune classe Qt a declarer dans le .pro en plus.)

#include <QApplication>
#include <QEasingCurve>
#include <QEvent>
#include <QObject>
#include <QPropertyAnimation>
#include <QScrollArea>
#include <QScrollBar>
#include <QWheelEvent>
#include <QWidget>

// Anime la molette de la souris : chaque cran fait glisser la page en douceur
// au lieu de la faire sauter. Le pave tactile (defilement en pixels) reste direct.
class SmoothScroller : public QObject
{
public:
    explicit SmoothScroller(QScrollArea *area)
        : QObject(area)
        , m_area(area)
        , m_anim(new QPropertyAnimation(area->verticalScrollBar(), "value", this))
    {
        m_anim->setDuration(240);
        m_anim->setEasingCurve(QEasingCurve::OutCubic);
        area->verticalScrollBar()->setSingleStep(40);   // fleches haut/bas du clavier
        area->viewport()->installEventFilter(this);
    }

protected:
    bool eventFilter(QObject *obj, QEvent *event) override
    {
        if (event->type() != QEvent::Wheel)
            return QObject::eventFilter(obj, event);

        QWheelEvent *we = static_cast<QWheelEvent *>(event);
        QScrollBar *bar = m_area->verticalScrollBar();

        if (!we->pixelDelta().isNull()) {               // pave tactile
            m_anim->stop();
            bar->setValue(bar->value() - we->pixelDelta().y());
            return true;
        }

        const int dy = we->angleDelta().y();
        if (dy == 0)
            return false;

        const int step = -dy * 100 / 120;               // ~100 px par cran de molette
        const int base = (m_anim->state() == QAbstractAnimation::Running) ? m_target
                                                                           : bar->value();
        m_target = qBound(bar->minimum(), base + step, bar->maximum());
        m_anim->stop();
        m_anim->setStartValue(bar->value());
        m_anim->setEndValue(m_target);
        m_anim->start();
        return true;
    }

private:
    QScrollArea *m_area;
    QPropertyAnimation *m_anim;
    int m_target = 0;
};

// Empeche un widget (curseur, liste deroulante, tableau...) de "voler" la molette
// de la souris : l'evenement est renvoye a la page pour qu'elle defile.
// Si onlyWhenUnfocused = true, le widget garde la molette quand il a le focus.
class WheelGuard : public QObject
{
public:
    WheelGuard(QWidget *forwardTo, bool onlyWhenUnfocused, QObject *parent)
        : QObject(parent), m_target(forwardTo), m_onlyWhenUnfocused(onlyWhenUnfocused) {}

protected:
    bool eventFilter(QObject *obj, QEvent *event) override
    {
        if (event->type() == QEvent::Wheel) {
            bool keep = false;
            if (m_onlyWhenUnfocused) {
                QWidget *w = qobject_cast<QWidget *>(obj);
                keep = w && (w->hasFocus()
                             || (w->parentWidget() && w->parentWidget()->hasFocus()));
            }
            if (!keep) {
                QApplication::sendEvent(m_target, event);
                return true;
            }
        }
        return QObject::eventFilter(obj, event);
    }

private:
    QWidget *m_target;
    bool m_onlyWhenUnfocused;
};

#endif // SCROLLHELPERS_H
