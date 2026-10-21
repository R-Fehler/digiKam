/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Description : Photos mode - touchpad gestures for the Qt Quick scene.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#pragma once

// Qt includes

#include <QElapsedTimer>
#include <QEvent>
#include <QLineF>
#include <QList>
#include <QWheelEvent>
#include <QQuickWidget>
#include <QQuickWindow>
#include <QNativeGestureEvent>
#include <QTouchEvent>

namespace Digikam
{

/**
 * QQuickWidget forwards mouse, wheel, touch and key events to its Qt Quick
 * scene, but not native gestures (QEvent::NativeGesture): touchpad pinch on
 * Wayland, X11 (XInput 2.4) and macOS never reached the PinchHandlers.
 * Call this from an event filter installed on the QQuickWidget.
 *
 * Returns true when the event was a native gesture (it is then handled).
 */
inline bool photosForwardNativeGesture(QQuickWidget* const view, QEvent* const event)
{
    if (event->type() != QEvent::NativeGesture)
    {
        return false;
    }

    QQuickWindow* const window = view->quickWindow();

    if (!window)
    {
        return false;
    }

    const QNativeGestureEvent* const gesture = static_cast<QNativeGestureEvent*>(event);

    // The offscreen window covers the widget exactly: widget coordinates are
    // scene coordinates.

    QNativeGestureEvent copy(gesture->gestureType(),
                             gesture->pointingDevice(),
                             gesture->fingerCount(),
                             gesture->position(),
                             gesture->position(),
                             gesture->globalPosition(),
                             gesture->value(),
                             gesture->delta());

    QCoreApplication::sendEvent(window, &copy);
    event->setAccepted(copy.isAccepted());

    return true;
}

/**
 * Number of touch points currently down, from the touch events the
 * QQuickWidget receives (before Qt Quick delivers them), or -1 when the event
 * is not a touch event. A Flickable which is already scrolling with one finger
 * keeps the touch points it sees, and hides a second finger from the handlers
 * inside it: a pinch could not start. Knowing it from here, the scene stops
 * scrolling as soon as a second finger lands, and the pinch takes over.
 */
inline int photosTouchPointCount(QEvent* const event)
{
    switch (event->type())
    {
        case QEvent::TouchBegin:
        case QEvent::TouchUpdate:
        {
            int down = 0;

            for (const QEventPoint& point : static_cast<QTouchEvent*>(event)->points())
            {
                if (point.state() != QEventPoint::Released)
                {
                    ++down;
                }
            }

            return down;
        }

        case QEvent::TouchEnd:
        case QEvent::TouchCancel:
        {
            return 0;
        }

        default:
        {
            return -1;
        }
    }
}

/**
 * Delivers a touch event to the QQuickWidget, then marks all its points as
 * accepted. Qt stops sending a touch point to a widget which did not accept
 * it: when no Qt Quick item or handler takes the second finger, it would
 * vanish from the following events and a pinch could not be recognized.
 * Call from the event filter installed on the QQuickWidget; returns true when
 * the event was a touch event (it is then handled). The filter sees the event
 * a second time during the delivery, with *delivering set: let it through.
 */
inline bool photosDeliverTouch(QQuickWidget* const view, QEvent* const event, bool* const delivering)
{
    if (*delivering || (photosTouchPointCount(event) < 0))
    {
        return false;
    }

    *delivering = true;
    QCoreApplication::sendEvent(view, event);
    *delivering = false;

    QTouchEvent* const touch = static_cast<QTouchEvent*>(event);

    for (qsizetype i = 0 ; i < touch->pointCount() ; ++i)
    {
        touch->point(i).setAccepted(true);
    }

    touch->setAccepted(true);

    return true;
}

/**
 * Two finger pinch on touch screens, recognized from the touch events the
 * QQuickWidget receives (see photosTouchPointCount() for why not with a
 * PinchHandler): the scale is the finger distance relative to its value when
 * the second finger landed, the center is between the fingers, in scene
 * (widget) coordinates.
 */
class PhotosTouchPinch
{
public:

    enum Phase
    {
        None = -1,
        Started,
        Updated,
        Finished
    };

    struct Step
    {
        Phase   phase  = None;
        qreal   scale  = 1.0;
        QPointF center;
    };

    /// Feed every touch event; returns what happened to the pinch.
    Step handle(QEvent* const event)
    {
        Step step;

        if (photosTouchPointCount(event) < 0)
        {
            return step;
        }

        QList<QEventPoint> down;

        if ((event->type() == QEvent::TouchBegin) || (event->type() == QEvent::TouchUpdate))
        {
            for (const QEventPoint& point : static_cast<QTouchEvent*>(event)->points())
            {
                if (point.state() != QEventPoint::Released)
                {
                    down << point;
                }
            }
        }

        if (down.size() >= 2)
        {
            const QPointF a        = down.at(0).position();
            const QPointF b        = down.at(1).position();
            const qreal distance   = qMax(1.0, QLineF(a, b).length());
            const QPair<int, int> ids(down.at(0).id(), down.at(1).id());
            step.center            = (a + b) / 2.0;

            if (!m_active)
            {
                m_active   = true;
                m_ids      = ids;
                m_start    = distance;
                m_scale    = 1.0;
                step.phase = Started;
                step.scale = 1.0;
            }
            else
            {
                if (ids != m_ids)
                {
                    // Another finger pair: continue from the current scale.

                    m_ids   = ids;
                    m_start = distance / m_scale;
                }

                m_scale    = distance / m_start;
                step.phase = Updated;
                step.scale = m_scale;
            }

            m_center = step.center;
        }
        else if (m_active)
        {
            m_active    = false;
            step.phase  = Finished;
            step.scale  = m_scale;
            step.center = m_center;
        }

        return step;
    }

private:

    bool            m_active = false;
    QPair<int, int> m_ids;
    qreal           m_start  = 1.0;
    qreal           m_scale  = 1.0;
    QPointF         m_center;
};

/**
 * Touchpad scrolling keeps going after the fingers leave the touchpad
 * (momentum, "kinetic" scrolling), as on a phone.
 *
 * macOS adds momentum itself (Qt::ScrollMomentum events). On Wayland, the
 * compositor only reports when the fingers left (Qt::ScrollEnd) and the
 * application continues the movement, as GTK does; Qt Quick's Flickable
 * stops dead instead. This tells when to continue: at the end of a touchpad
 * scroll whose fingers were still moving when they left, without momentum
 * from the system. Not for wheels of mice (no scroll phases), nor X11 and
 * Windows, whose events have no end.
 *
 * Feed it the wheel events of the scene (from an event filter): it only
 * observes them.
 */
class PhotosWheelMomentum
{
public:

    enum Action
    {
        None  = 0,
        Stop  = 1,      ///< fingers on the touchpad again: stop a running fling
        Fling = 2       ///< continue with velocity (pixels/s, in the direction of pixelDelta)
    };

    struct Step
    {
        Action  action = None;
        QPointF velocity;
        QPointF position;           ///< where, in the coordinates of the event
    };

    /// Samples older than this before the end are not used for the velocity.
    static constexpr qint64 s_windowMs    = 100;

    /// Fingers resting longer than this before leaving: no fling.
    static constexpr qint64 s_liftGapMs   = 80;

    /// Slower than this (pixels/s): no fling.
    static constexpr qreal  s_minVelocity = 150.0;

    PhotosWheelMomentum()
    {
        m_clock.start();
    }

    Step handle(const QEvent* const event)
    {
        Step step;

        if (event->type() != QEvent::Wheel)
        {
            return step;
        }

        const QWheelEvent* const wheel = static_cast<const QWheelEvent*>(event);
        const qint64 time              = wheel->timestamp() ? qint64(wheel->timestamp()) : m_clock.elapsed();

        if (wheel->modifiers() & Qt::ControlModifier)
        {
            reset();            // Ctrl: zoom, not scrolling

            return step;
        }

        switch (wheel->phase())
        {
            case Qt::ScrollBegin:
            {
                reset();
                m_active    = true;
                step.action = Stop;
                break;
            }

            case Qt::ScrollUpdate:
            {
                if (m_active && !wheel->pixelDelta().isNull())
                {
                    m_samples << Sample { time, QPointF(wheel->pixelDelta()) };

                    while ((m_samples.size() > 2) && ((time - m_samples.constFirst().time) > s_windowMs))
                    {
                        m_samples.removeFirst();
                    }
                }

                break;
            }

            case Qt::ScrollMomentum:
            {
                m_systemMomentum = true;
                break;
            }

            case Qt::ScrollEnd:
            {
                if (
                    m_active && !m_systemMomentum && (m_samples.size() >= 2) &&
                    ((time - m_samples.constLast().time) <= s_liftGapMs)
                   )
                {
                    const qint64 span = m_samples.constLast().time - m_samples.constFirst().time;
                    QPointF distance;

                    for (int i = 1 ; i < m_samples.size() ; ++i)
                    {
                        distance += m_samples.at(i).delta;
                    }

                    if (span > 0)
                    {
                        const QPointF velocity = distance * 1000.0 / qreal(span);

                        if (qMax(qAbs(velocity.x()), qAbs(velocity.y())) >= s_minVelocity)
                        {
                            step.action   = Fling;
                            step.velocity = velocity;
                            step.position = wheel->position();
                        }
                    }
                }

                reset();
                break;
            }

            default:
            {
                reset();        // a mouse wheel
                break;
            }
        }

        return step;
    }

private:

    void reset()
    {
        m_samples.clear();
        m_active         = false;
        m_systemMomentum = false;
    }

private:

    struct Sample
    {
        qint64  time;
        QPointF delta;
    };

    QList<Sample> m_samples;
    QElapsedTimer m_clock;
    bool          m_active         = false;
    bool          m_systemMomentum = false;
};

} // namespace Digikam
