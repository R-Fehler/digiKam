// SPDX-License-Identifier: GPL-2.0-or-later
//
// Photos mode: pinch to zoom through a QQuickWidget, as in PhotosContainer.
//
//  - touchpad pinch (QNativeGestureEvent), forwarded by photosForwardNativeGesture();
//  - two finger touch pinch, recognized by PhotosTouchPinch, including when
//    the first finger already scrolls the list.
//
// The QML mirrors the input setup of PhotoGrid.qml (pinch inside a ListView,
// mouse overlay above it, touch tap / long-press / point handlers) and of
// Viewer.qml (pinch with a drag handler for pan / swipe).
//
// Build and run (see streamline/docs/03-mvp-status.md):
//   g++ -std=c++17 -fPIC -I core/app/photos streamline/tests/gesture_test.cpp \
//       $(pkg-config --cflags --libs Qt6QuickWidgets Qt6Quick Qt6Test) -o gesture_test
//   QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software ./gesture_test

#include <QApplication>
#include <QFile>
#include <QTemporaryDir>
#include <QPointingDevice>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWidget>
#include <QTest>

#include "photosgestures.h"

static const char* const qmlSource = R"QML(
import QtQuick

Item {
    id: root
    width: 800
    height: 600

    property real gridZoomSteps: 0
    property real viewerScale:   1.0
    property real pinchBase:     1.0
    property real pinchStart:    1.0
    property bool multiTouch:    false     // photosApp.multiTouch
    property int  taps:          0
    property real listY:         list.contentY

    // PhotosContainer::touchPinch* -> Main.qml dispatch (by position here).
    function touchPinch(phase, scale, x, y) {
        if (x < 400) {
            if (phase === 0) pinchBase = 1.0
            else if (phase === 1) gridPinchScaled(scale)
        }
        else {
            if (phase === 0) pinchStart = viewerScale
            else if (phase === 1) viewerScale = pinchStart * scale
        }
    }

    function gridPinchScaled(scale) {
        const ratio = scale / pinchBase
        if (ratio > 1.25) { gridZoomSteps += 1; pinchBase = scale }
        else if (ratio < 0.8) { gridZoomSteps -= 1; pinchBase = scale }
    }

    // --- like PhotoGrid.qml ---------------------------------------------------

    ListView {
        id: list
        x: 0; y: 0; width: 400; height: 600
        model: 100
        delegate: Rectangle { width: 400; height: 60; color: index % 2 ? "grey" : "white" }

        interactive: !root.multiTouch

        PinchHandler {
            target: null
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
            onActiveChanged: if (active) root.pinchBase = 1.0
            onActiveScaleChanged: root.gridPinchScaled(activeScale)
        }

        TapHandler { acceptedDevices: PointerDevice.TouchScreen; onTapped: root.taps++ }
        PointHandler { acceptedDevices: PointerDevice.TouchScreen }
    }

    MouseArea {
        anchors.fill: list
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        hoverEnabled: true
        onPressed: (mouse) => { if (mouse.source !== Qt.MouseEventNotSynthesized) mouse.accepted = false }
    }

    // --- like Viewer.qml ------------------------------------------------------

    Item {
        x: 400; y: 0; width: 400; height: 600

        PinchHandler {
            target: null
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
            onActiveChanged: if (active) root.pinchStart = root.viewerScale
            onActiveScaleChanged: root.viewerScale = root.pinchStart * activeScale
        }

        DragHandler { target: null; enabled: !root.multiTouch }
        TapHandler { }
    }
}
)QML";

class Forwarder : public QObject
{
public:

    explicit Forwarder(QQuickWidget* const view) : QObject(view), m_view(view) { view->installEventFilter(this); }

    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if ((watched == m_view) && Digikam::photosForwardNativeGesture(m_view, event))
        {
            return true;
        }

        // As PhotosContainer::eventFilter().

        const int touchPoints = ((watched == m_view) && !m_delivering) ? Digikam::photosTouchPointCount(event) : -1;

        if (touchPoints >= 0)
        {
            QObject* const root = m_view->rootObject();
            root->setProperty("multiTouch", touchPoints >= 2);

            const Digikam::PhotosTouchPinch::Step step = m_pinch.handle(event);

            if (step.phase != Digikam::PhotosTouchPinch::None)
            {
                QMetaObject::invokeMethod(root, "touchPinch",
                                          Q_ARG(QVariant, int(step.phase)), Q_ARG(QVariant, step.scale),
                                          Q_ARG(QVariant, step.center.x()), Q_ARG(QVariant, step.center.y()));
            }
        }

        if ((watched == m_view) && Digikam::photosDeliverTouch(m_view, event, &m_delivering))
        {
            return true;
        }

        return QObject::eventFilter(watched, event);
    }

private:

    QQuickWidget*              m_view;
    Digikam::PhotosTouchPinch  m_pinch;
    bool                       m_delivering = false;
};

static int failures = 0;

static void check(bool ok, const char* what, double value)
{
    printf("%s  %-60s (%g)\n", ok ? "PASS" : "FAIL", what, value);

    if (!ok)
    {
        ++failures;
    }
}

static void nativePinch(QQuickWidget* view, const QPointF& pos, const QList<qreal>& steps)
{
    const QPointingDevice* const touchpad = QPointingDevice::primaryPointingDevice();
    const QPointF global                  = view->mapToGlobal(pos);

    auto send = [&] (Qt::NativeGestureType type, qreal value)
    {
        QNativeGestureEvent ev(type, touchpad, 2, pos, pos, global, value, QPointF());
        QApplication::sendEvent(view, &ev);
        QApplication::processEvents();
    };

    send(Qt::BeginNativeGesture, 0);

    for (const qreal step : steps)
    {
        send(Qt::ZoomNativeGesture, step);
    }

    send(Qt::EndNativeGesture, 0);
}

static void touchPinch(QQuickWidget* view, QPointingDevice* device, const QPoint& center, int from, int to)
{
    // Two fingers move apart (to > from) or together, horizontally.

    QTest::touchEvent(view, device).press(0, center - QPoint(from, 0)).press(1, center + QPoint(from, 0));
    QApplication::processEvents();

    const int steps = 12;

    for (int i = 1 ; i <= steps ; ++i)
    {
        const int d = from + (to - from) * i / steps;
        QTest::touchEvent(view, device).move(0, center - QPoint(d, 0)).move(1, center + QPoint(d, 0));
        QApplication::processEvents();
        QTest::qWait(16);
    }

    QTest::touchEvent(view, device).release(0, center - QPoint(to, 0)).release(1, center + QPoint(to, 0));
    QApplication::processEvents();
}

static void touchPinchStaggered(QQuickWidget* view, QPointingDevice* device, const QPoint& center, int to)
{
    // The first finger lands and starts scrolling, the second one follows.

    QPoint p0 = center - QPoint(30, 0);
    QTest::touchEvent(view, device).press(0, p0);
    QApplication::processEvents();

    for (int i = 0 ; i < 6 ; ++i)
    {
        p0 += QPoint(0, -8);
        QTest::touchEvent(view, device).move(0, p0);
        QApplication::processEvents();
        QTest::qWait(16);
    }

    QPoint p1 = p0 + QPoint(60, 0);
    QTest::touchEvent(view, device).move(0, p0).press(1, p1);
    QApplication::processEvents();

    for (int i = 1 ; i <= 12 ; ++i)
    {
        const int d = 30 + (to - 30) * i / 12;
        QTest::touchEvent(view, device).move(0, QPoint(center.x() - d, p0.y())).move(1, QPoint(center.x() + d, p0.y()));
        QApplication::processEvents();
        QTest::qWait(16);
    }

    QTest::touchEvent(view, device).release(0, QPoint(center.x() - to, p0.y())).release(1, QPoint(center.x() + to, p0.y()));
    QApplication::processEvents();
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);

    QTemporaryDir dir;
    const QString qmlFile = dir.filePath(QStringLiteral("gesture_test.qml"));
    QFile file(qmlFile);
    file.open(QIODevice::WriteOnly);
    file.write(qmlSource);
    file.close();

    QQuickWidget view;
    view.setResizeMode(QQuickWidget::SizeRootObjectToView);
    view.resize(800, 600);
    view.setSource(QUrl::fromLocalFile(qmlFile));

    if (view.status() != QQuickWidget::Ready)
    {
        qWarning() << view.errors();
        return 2;
    }

    view.show();

    if (!QTest::qWaitForWindowExposed(&view))
    {
        return 2;
    }

    QQuickItem* const root     = view.rootObject();
    Forwarder* const forwarder = new Forwarder(&view);

    // 1. Touchpad pinch over the grid and over the viewer.

    nativePinch(&view, QPointF(200, 300), {0.1, 0.1, 0.1, 0.1, 0.1});
    check(root->property("gridZoomSteps").toDouble() >= 1.0, "touchpad pinch out over the grid zooms", root->property("gridZoomSteps").toDouble());

    nativePinch(&view, QPointF(600, 300), {0.1, 0.1, 0.1});
    check(root->property("viewerScale").toDouble() > 1.2, "touchpad pinch out over the viewer zooms", root->property("viewerScale").toDouble());

    // 2. Two finger touch pinch.

    QPointingDevice* const touch = QTest::createTouchDevice();

    root->setProperty("gridZoomSteps", 0);
    touchPinch(&view, touch, QPoint(200, 300), 30, 140);
    check(root->property("gridZoomSteps").toDouble() >= 1.0, "touch pinch out over the grid zooms", root->property("gridZoomSteps").toDouble());

    root->setProperty("viewerScale", 1.0);
    touchPinch(&view, touch, QPoint(600, 300), 30, 140);
    check(root->property("viewerScale").toDouble() > 1.5, "touch pinch out over the viewer zooms", root->property("viewerScale").toDouble());

    // 3. Second finger lands after the first one started scrolling / dragging.

    root->setProperty("gridZoomSteps", 0);
    touchPinchStaggered(&view, touch, QPoint(200, 300), 160);
    check(root->property("gridZoomSteps").toDouble() >= 1.0, "touch pinch after a scroll started, over the grid", root->property("gridZoomSteps").toDouble());

    root->setProperty("viewerScale", 1.0);
    touchPinchStaggered(&view, touch, QPoint(600, 300), 160);
    check(root->property("viewerScale").toDouble() > 1.5, "touch pinch after a drag started, over the viewer", root->property("viewerScale").toDouble());

    // 4. One finger still taps and scrolls.

    QTest::touchEvent(&view, touch).press(0, QPoint(200, 200));
    QTest::touchEvent(&view, touch).release(0, QPoint(200, 200));
    QApplication::processEvents();
    check(root->property("taps").toInt() == 1, "one finger tap on the grid", root->property("taps").toInt());

    const double before = root->property("listY").toDouble();
    QPoint finger(200, 500);
    QTest::touchEvent(&view, touch).press(0, finger);

    for (int i = 0 ; i < 15 ; ++i)
    {
        finger -= QPoint(0, 20);
        QTest::touchEvent(&view, touch).move(0, finger);
        QApplication::processEvents();
        QTest::qWait(16);
    }

    QTest::touchEvent(&view, touch).release(0, finger);
    QTest::qWait(300);
    check(root->property("listY").toDouble() > before + 100, "one finger drag scrolls the grid", root->property("listY").toDouble() - before);

    // 5. Without forwarding, touchpad pinch does not arrive (documents the Qt behaviour).

    view.removeEventFilter(forwarder);
    root->setProperty("viewerScale", 1.0);
    nativePinch(&view, QPointF(600, 300), {0.1, 0.1, 0.1});
    printf("info  without forwarding, viewer scale after touchpad pinch: %g\n", root->property("viewerScale").toDouble());

    printf("%s\n", failures ? "FAILED" : "ALL PASSED");

    return failures ? 1 : 0;
}
