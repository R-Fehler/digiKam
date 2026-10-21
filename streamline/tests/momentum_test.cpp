// SPDX-License-Identifier: GPL-2.0-or-later
//
// Photos mode: touchpad scrolling keeps going when the fingers leave the
// touchpad (PhotosWheelMomentum), in the grid and in the viewer's strip of
// thumbnails, through a QQuickWidget as in PhotosContainer.
//
// The QML mirrors the momentum code of PhotoGrid.qml (vertical list) and
// Filmstrip.qml (horizontal list, snapping to an item).
//
// Build and run:
//   g++ -std=c++17 -fPIC -I core/app/photos streamline/tests/momentum_test.cpp \
//       $(pkg-config --cflags --libs Qt6QuickWidgets Qt6Quick Qt6Test Qt6Widgets) -o momentum_test
//   QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software ./momentum_test

#include <QApplication>
#include <QElapsedTimer>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWidget>
#include <QTest>
#include <QTimer>
#include <QWheelEvent>

#include "photosgestures.h"

static const char* const qmlSource = R"QML(
import QtQuick

Item {
    id: root
    width: 800
    height: 600

    property alias gridY:  grid.contentY
    property alias stripX: strip.contentX

    // Main.qml: the list under the fingers.
    function wheelFling(vx, vy, x, y) {
        gridFling(vx, vy, x, y)
        stripFling(vx, vy, x, y)
    }

    function stopFling() {
        if (grid.flicking)  grid.cancelFlick()
        if (strip.flicking) strip.cancelFlick()
    }

    // PhotoGrid.qml
    property real gridVelocity: 0
    property real gridVelocityTime: 0

    function gridFling(vx, vy, x, y) {
        const p = grid.mapFromItem(null, x, y)
        if (!grid.contains(p)) return
        const v = ((Date.now() - gridVelocityTime) < 150) ? -gridVelocity : vy
        grid.flickDeceleration = 2500
        grid.flick(0, v)
    }

    ListView {
        id: grid
        width: 800; height: 500
        model: 5000
        boundsBehavior: Flickable.StopAtBounds
        flickDeceleration: 4000
        maximumFlickVelocity: 8000
        pixelAligned: true
        onDraggingChanged: if (dragging) flickDeceleration = 4000
        onMovementEnded: flickDeceleration = 4000
        onVerticalVelocityChanged: {
            if ((verticalVelocity !== 0) && !flicking) {
                root.gridVelocity = verticalVelocity
                root.gridVelocityTime = Date.now()
            }
        }
        delegate: Rectangle { width: 800; height: 100; color: (index % 2) ? "gray" : "white" }
    }

    // Filmstrip.qml
    property real stripVelocity: 0
    property real stripVelocityTime: 0

    function stripFling(vx, vy, x, y) {
        const p = strip.mapFromItem(null, x, y)
        if (!strip.contains(p)) return
        const recent = (Date.now() - stripVelocityTime) < 150
        strip.flick(recent ? -stripVelocity : ((Math.abs(vx) >= Math.abs(vy)) ? vx : vy), 0)
    }

    ListView {
        id: strip
        y: 520; width: 800; height: 56
        orientation: ListView.Horizontal
        model: 3000
        spacing: 2
        boundsBehavior: Flickable.StopAtBounds
        flickDeceleration: 3000
        highlightRangeMode: ListView.StrictlyEnforceRange
        preferredHighlightBegin: width / 2 - 22
        preferredHighlightEnd: width / 2 + 22
        highlightMoveDuration: 160
        onHorizontalVelocityChanged: {
            if ((horizontalVelocity !== 0) && !flicking) {
                root.stripVelocity = horizontalVelocity
                root.stripVelocityTime = Date.now()
            }
        }
        delegate: Rectangle { width: 44; height: 44; color: (index % 2) ? "gray" : "white" }
    }
}
)QML";

// As PhotosContainer::eventFilter(): observe, never consume.
class Filter : public QObject
{
public:

    explicit Filter(QQuickWidget* const view) : QObject(view), m_view(view) { view->installEventFilter(this); }

    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if ((watched == m_view) && (event->type() == QEvent::Wheel))
        {
            const Digikam::PhotosWheelMomentum::Step step = m_momentum.handle(event);
            QObject* const root                           = m_view->rootObject();

            if (step.action == Digikam::PhotosWheelMomentum::Stop)
            {
                QMetaObject::invokeMethod(root, "stopFling");
            }
            else if (step.action == Digikam::PhotosWheelMomentum::Fling)
            {
                ++flings;

                QTimer::singleShot(0, this, [root, step] ()
                    {
                        QMetaObject::invokeMethod(root, "wheelFling",
                                                  Q_ARG(QVariant, step.velocity.x()), Q_ARG(QVariant, step.velocity.y()),
                                                  Q_ARG(QVariant, step.position.x()), Q_ARG(QVariant, step.position.y()));
                    }
                );
            }
        }

        return QObject::eventFilter(watched, event);
    }

    int flings = 0;

private:

    QQuickWidget*                 m_view;
    Digikam::PhotosWheelMomentum  m_momentum;
};

static int failures = 0;

static void check(bool ok, const char* what, double value)
{
    printf("%s  %-62s (%g)\n", ok ? "PASS" : "FAIL", what, value);

    if (!ok)
    {
        ++failures;
    }
}

static QElapsedTimer s_clock;

static void wheel(QQuickWidget* view, const QPointF& pos, Qt::ScrollPhase phase, const QPoint& pixels,
                  Qt::KeyboardModifiers modifiers = Qt::NoModifier)
{
    QWheelEvent event(pos, view->mapToGlobal(pos.toPoint()), pixels, pixels * 3 /*angle*/,
                      Qt::NoButton, modifiers, phase, false);
    event.setTimestamp(ulong(s_clock.elapsed()));
    QApplication::sendEvent(view, &event);
}

/// Two fingers moving: begin, updates every 10 ms, then (after restMs) the fingers leave.
static void swipe(QQuickWidget* view, const QPointF& pos, const QPoint& step, int count, int restMs = 0,
                  Qt::KeyboardModifiers modifiers = Qt::NoModifier, bool systemMomentum = false)
{
    wheel(view, pos, Qt::ScrollBegin, QPoint(), modifiers);

    for (int i = 0 ; i < count ; ++i)
    {
        QTest::qWait(10);
        wheel(view, pos, Qt::ScrollUpdate, step, modifiers);
    }

    if (systemMomentum)
    {
        // macOS: the system continues the movement itself.

        for (int i = 0 ; i < 5 ; ++i)
        {
            QTest::qWait(10);
            wheel(view, pos, Qt::ScrollMomentum, step / 2, modifiers);
        }
    }

    if (restMs > 0)
    {
        QTest::qWait(restMs);
    }

    QTest::qWait(5);
    wheel(view, pos, Qt::ScrollEnd, QPoint(), modifiers);
}

static qreal prop(QQuickWidget* view, const char* name)
{
    return view->rootObject()->property(name).toReal();
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    s_clock.start();

    QQuickWidget view;
    view.setResizeMode(QQuickWidget::SizeRootObjectToView);
    QQmlComponent component(view.engine());
    component.setData(qmlSource, QUrl());
    view.setContent(QUrl(), &component, component.create());

    if (!view.rootObject())
    {
        printf("QML errors: %s\n", qPrintable(component.errorString()));

        return 1;
    }

    view.resize(800, 600);
    view.show();

    if (!QTest::qWaitForWindowExposed(&view))
    {
        return 1;
    }

    Filter filter(&view);
    QObject* const root = view.rootObject();
    const QPointF overGrid(400, 250);
    const QPointF overStrip(400, 548);

    // --- The grid --------------------------------------------------------------------

    // Scrolling down: the fingers move up (negative deltas with natural scrolling).

    root->setProperty("gridY", 50000);
    QTest::qWait(50);
    swipe(&view, overGrid, QPoint(0, -25), 12);
    const qreal atLift = prop(&view, "gridY");
    QTest::qWait(400);
    const qreal later  = prop(&view, "gridY");
    check(later > atLift + 100, "grid: keeps scrolling down after the fingers leave", later - atLift);
    QTest::qWait(1500);
    const qreal settled = prop(&view, "gridY");
    QTest::qWait(200);
    check(qAbs(prop(&view, "gridY") - settled) < 1, "grid: then comes to rest", settled - later);

    // Scrolling up.

    swipe(&view, overGrid, QPoint(0, 25), 12);
    const qreal upLift = prop(&view, "gridY");
    QTest::qWait(400);
    check(prop(&view, "gridY") < upLift - 100, "grid: keeps scrolling up", upLift - prop(&view, "gridY"));
    QTest::qWait(1500);

    // Fingers back on the touchpad: the fling stops.

    swipe(&view, overGrid, QPoint(0, -25), 12);
    QTest::qWait(100);
    wheel(&view, overGrid, Qt::ScrollBegin, QPoint());
    QTest::qWait(30);
    const qreal caught = prop(&view, "gridY");
    QTest::qWait(300);
    check(qAbs(prop(&view, "gridY") - caught) < 2, "grid: fingers on the touchpad stop it", prop(&view, "gridY") - caught);
    wheel(&view, overGrid, Qt::ScrollEnd, QPoint());
    QTest::qWait(300);

    // Fingers resting before leaving: no fling.

    int before = filter.flings;
    swipe(&view, overGrid, QPoint(0, -25), 12, 150);
    const qreal rested = prop(&view, "gridY");
    QTest::qWait(300);
    check((filter.flings == before) && (qAbs(prop(&view, "gridY") - rested) < 2),
          "grid: fingers resting before leaving: no fling", prop(&view, "gridY") - rested);

    // Slow scrolling: no fling.

    before = filter.flings;
    swipe(&view, overGrid, QPoint(0, -1), 12);
    QTest::qWait(300);
    check(filter.flings == before, "grid: slow scrolling: no fling", filter.flings - before);

    // Ctrl: zoom, not scrolling.

    before = filter.flings;
    swipe(&view, overGrid, QPoint(0, -25), 12, 0, Qt::ControlModifier);
    QTest::qWait(100);
    check(filter.flings == before, "grid: Ctrl + scroll (zoom): no fling", filter.flings - before);

    // macOS: momentum from the system, none added.

    before = filter.flings;
    swipe(&view, overGrid, QPoint(0, -25), 12, 0, Qt::NoModifier, true);
    QTest::qWait(100);
    check(filter.flings == before, "grid: momentum from the system (macOS): none added", filter.flings - before);

    // A mouse wheel: no scroll phases, no fling.

    before = filter.flings;

    for (int i = 0 ; i < 5 ; ++i)
    {
        wheel(&view, overGrid, Qt::NoScrollPhase, QPoint(0, -120));
        QTest::qWait(10);
    }

    QTest::qWait(100);
    check(filter.flings == before, "grid: mouse wheel: no fling", filter.flings - before);

    // --- The strip of thumbnails -------------------------------------------------------

    root->setProperty("stripX", 40000);
    QTest::qWait(300);
    const qreal stripStart = prop(&view, "stripX");
    swipe(&view, overStrip, QPoint(-25, 0), 12);
    const qreal stripLift  = prop(&view, "stripX");
    QTest::qWait(400);
    check(prop(&view, "stripX") > stripLift + 100, "strip: keeps scrolling after the fingers leave",
          prop(&view, "stripX") - stripLift);
    QTest::qWait(1500);
    check(prop(&view, "stripX") > stripStart + 300, "strip: travelled further than the fingers",
          prop(&view, "stripX") - stripStart);

    // Over the strip, the grid does not move.

    const qreal gridBefore = prop(&view, "gridY");
    swipe(&view, overStrip, QPoint(-25, 0), 12);
    QTest::qWait(400);
    check(qAbs(prop(&view, "gridY") - gridBefore) < 1, "strip: the grid stays put", prop(&view, "gridY") - gridBefore);

    printf("\n%s\n", failures ? "FAILED" : "ALL PASSED");

    return failures ? 1 : 0;
}
