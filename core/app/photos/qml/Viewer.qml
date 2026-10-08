// SPDX-License-Identifier: GPL-2.0-or-later
// Photos mode - full window photo viewer with GPU zoom and pan.

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Window

FocusScope {
    id: viewer

    property int  index:    -1
    property real zoom:     1.0     // 1.0 = whole photo fits the window
    property real panX:     0.0
    property real panY:     0.0
    property bool chrome:   true
    property real pinchStartZoom: 1.0
    property real pinchEndTime: 0   // ms, see the swipe in the drag handler

    readonly property real maxZoom:     12.0
    readonly property int  previewSize: Math.min(3840, Math.ceil(Math.max(Screen.width, Screen.height)
                                                                 * Screen.devicePixelRatio))
    readonly property bool favorite:    (library.revision >= 0) && library.isFavoriteAt(index)
    readonly property bool video:       (library.revision >= 0) && library.isVideoAt(index)

    signal closed(int index)
    signal requestTrash(int row)
    signal requestAddToAlbum(int row)

    // After a reload (e.g. this photo was moved to the trash) the same index
    // shows the next photo; stay within bounds.
    Connections {
        target: library

        function onReloaded() {
            if (!viewer.visible)
                return

            if (library.count === 0)
                viewer.close()
            else if (viewer.index >= library.count)
                viewer.index = library.count - 1
        }
    }

    function open(i) {
        index   = i
        visible = true
        resetZoom()
        forceActiveFocus()
        showChrome()
    }

    function close() {
        visible = false
        closed(index)
    }

    function go(delta) {
        const next = index + delta
        if ((next >= 0) && (next < library.count)) {
            index = next
            resetZoom()
        }
    }

    function resetZoom() {
        zoom = 1.0
        panX = 0.0
        panY = 0.0
    }

    function clampPan() {
        panX = Math.min(0, Math.max(stage.width  - stage.width  * zoom, panX))
        panY = Math.min(0, Math.max(stage.height - stage.height * zoom, panY))
    }

    // Zoom keeping the stage point (px, py) under the cursor / fingers.
    function zoomAt(newZoom, px, py) {
        newZoom    = Math.max(1.0, Math.min(maxZoom, newZoom))
        const k    = newZoom / zoom
        panX       = px - (px - panX) * k
        panY       = py - (py - panY) * k
        zoom       = newZoom
        clampPan()
    }

    // Touch screen pinch, recognized in C++ (photosApp.touchPinch*): x, y in the scene.
    function touchPinch(phase, scale, sceneX, sceneY) {
        if (phase === 0) {
            pinchStartZoom = zoom
        }
        else if (phase === 1) {
            const p = stage.mapFromItem(null, sceneX, sceneY)
            zoomAt(pinchStartZoom * scale, p.x, p.y)
        }
        else {
            pinchEndTime = Date.now()
        }
    }

    function showChrome() {
        chrome = true
        chromeTimer.restart()
    }

    Rectangle {
        anchors.fill: parent
        color:        "black"
    }

    // --- Photo --------------------------------------------------------------

    Item {
        id: stage
        anchors.fill: parent
        clip:         true

        Item {
            id: photo
            width:  stage.width
            height: stage.height

            transform: [
                Scale     { xScale: viewer.zoom; yScale: viewer.zoom },
                Translate { x: viewer.panX; y: viewer.panY }
            ]

            // 1. Thumbnail: already in memory, shown instantly.
            Image {
                id: thumbLayer
                anchors.fill: parent
                source:       (library.revision >= 0)
                              ? library.thumbSourceAt(viewer.index, photosApp.thumbnailSizes[photosApp.thumbnailSizes.length - 1])
                              : ""
                fillMode:     Image.PreserveAspectFit
                asynchronous: true
                smooth:       true
                visible:      previewLayer.status !== Image.Ready
            }

            // 2. Screen sized preview (embedded preview when large enough).
            Image {
                id: previewLayer
                anchors.fill: parent
                source:       (viewer.index >= 0) && !viewer.video
                              ? library.previewSourceAt(viewer.index, viewer.previewSize) : ""
                fillMode:     Image.PreserveAspectFit
                asynchronous: true
                smooth:       true
                mipmap:       true
                visible:      !fullLayer.visible
            }

            // 3. Full resolution, only once zoomed beyond the preview resolution.
            Image {
                id: fullLayer

                readonly property bool needed: (previewLayer.status === Image.Ready) &&
                                               (previewLayer.paintedWidth * viewer.zoom * Screen.devicePixelRatio
                                                > previewLayer.implicitWidth * 1.05)
                property int loadedFor: -1

                anchors.fill: parent
                fillMode:     Image.PreserveAspectFit
                asynchronous: true
                smooth:       true
                mipmap:       true
                visible:      (status === Image.Ready) && (loadedFor === viewer.index)
                source:       (loadedFor === viewer.index) ? library.previewSourceAt(viewer.index, 0) : ""

                onNeededChanged: {
                    if (needed)
                        loadedFor = viewer.index
                }
            }
        }

        BusyIndicator {
            anchors.centerIn: parent
            running:          viewer.visible && !viewer.video && (previewLayer.status === Image.Loading)
                              && (thumbLayer.status !== Image.Ready)
            visible:          running
        }

        // Video: show the poster frame and hand playback to the system player.
        Rectangle {
            visible:          viewer.video
            anchors.centerIn: parent
            width:            72
            height:           72
            radius:           36
            color:            Qt.rgba(0, 0, 0, 0.55)

            Text {
                anchors.centerIn:               parent
                anchors.horizontalCenterOffset: 3
                text:                           "▶"
                color:                          "white"
                font.pixelSize:                 30
            }

            MouseArea {
                anchors.fill: parent
                cursorShape:  Qt.PointingHandCursor
                onClicked:    photosApp.openExternally(library.filePathAt(viewer.index))
            }
        }

        // Preload neighbours so that next / previous are instant.
        Image {
            visible:      false
            asynchronous: true
            source:       viewer.visible && (viewer.index + 1 < library.count) && !library.isVideoAt(viewer.index + 1)
                          ? library.previewSourceAt(viewer.index + 1, viewer.previewSize) : ""
        }

        Image {
            visible:      false
            asynchronous: true
            source:       viewer.visible && (viewer.index > 0) && !library.isVideoAt(viewer.index - 1)
                          ? library.previewSourceAt(viewer.index - 1, viewer.previewSize) : ""
        }

        // --- Input ---------------------------------------------------------

        WheelHandler {
            onWheel: (event) => {
                const factor = Math.pow(1.0015, event.angleDelta.y)
                viewer.zoomAt(viewer.zoom * factor, point.position.x, point.position.y)
                viewer.showChrome()
            }
        }

        // Touchpad pinch (native gestures, forwarded by PhotosContainer).
        // Touch screen pinch is recognized in C++, see touchPinch().

        PinchHandler {
            target:          null
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad

            onActiveChanged: {
                if (active)
                    viewer.pinchStartZoom = viewer.zoom
            }

            onActiveScaleChanged: viewer.zoomAt(viewer.pinchStartZoom * activeScale,
                                                centroid.position.x, centroid.position.y)
        }

        DragHandler {
            id: drag
            target:  null
            enabled: !photosApp.multiTouch     // two fingers: a pinch, not a pan or swipe
            property real lastX: 0
            property real lastY: 0

            onActiveChanged: {
                if (active) {
                    lastX = centroid.position.x
                    lastY = centroid.position.y
                }
                else if (photosApp.multiTouch || (Date.now() - viewer.pinchEndTime < 400)) {
                    // Released because a pinch started, or the last finger of one: no swipe.
                }
                else if (viewer.zoom <= 1.0) {
                    // Swipe to the next / previous photo when not zoomed.
                    const dx = centroid.position.x - centroid.pressPosition.x
                    if (dx < -80)
                        viewer.go(+1)
                    else if (dx > 80)
                        viewer.go(-1)
                }
            }

            onCentroidChanged: {
                if (!active || (viewer.zoom <= 1.0))
                    return

                viewer.panX += centroid.position.x - lastX
                viewer.panY += centroid.position.y - lastY
                lastX        = centroid.position.x
                lastY        = centroid.position.y
                viewer.clampPan()
            }
        }

        TapHandler {
            onDoubleTapped: (eventPoint) => {
                if (viewer.zoom > 1.0)
                    viewer.resetZoom()
                else
                    viewer.zoomAt(2.5, eventPoint.position.x, eventPoint.position.y)
            }
        }

        HoverHandler {
            onPointChanged: viewer.showChrome()
        }
    }

    // --- Chrome -------------------------------------------------------------

    Timer {
        id: chromeTimer
        interval: 2500
        onTriggered: viewer.chrome = false
    }

    Rectangle {
        id: topBar
        anchors.left:  parent.left
        anchors.right: parent.right
        anchors.top:   parent.top
        height:        56
        opacity:       viewer.chrome ? 1.0 : 0.0

        gradient: Gradient {
            GradientStop { position: 0.0; color: Qt.rgba(0, 0, 0, 0.65) }
            GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, 0.0)  }
        }

        Behavior on opacity {
            NumberAnimation { duration: 200 }
        }

        Row {
            anchors.left:           parent.left
            anchors.leftMargin:     8
            anchors.verticalCenter: parent.verticalCenter
            spacing:                8

            IconButton {
                text:      "‹  " + library.title
                textColor: "white"
                onClicked: viewer.close()
            }

            Column {
                anchors.verticalCenter: parent.verticalCenter

                Text {
                    text:           library.dateTextAt(viewer.index)
                    color:          "white"
                    font.pixelSize: 13
                    font.bold:      true
                }

                Text {
                    text:           library.fileNameAt(viewer.index)
                    color:          "white"
                    opacity:        0.7
                    font.pixelSize: 11
                }
            }
        }

        Row {
            anchors.right:          parent.right
            anchors.rightMargin:    8
            anchors.verticalCenter: parent.verticalCenter
            spacing:                4

            IconButton {
                text:      viewer.favorite ? "♥" : "♡"
                textColor: viewer.favorite ? "#ff5a6e" : "white"
                pixelSize: 20
                onClicked: library.toggleFavoriteAt(viewer.index)
            }

            IconButton {
                text:      qsTr("Add to album")
                textColor: "white"
                pixelSize: 13
                onClicked: viewer.requestAddToAlbum(viewer.index)
            }

            IconButton {
                visible:   library.filter === 2
                text:      qsTr("Remove from album")
                textColor: "white"
                pixelSize: 13
                onClicked: library.removeFromCurrentAlbum(viewer.index)
            }

            IconButton {
                text:      qsTr("Show in folder")
                textColor: "white"
                pixelSize: 13
                onClicked: photosApp.openContainingFolder(library.filePathAt(viewer.index))
            }

            IconButton {
                text:      qsTr("Move to trash")
                textColor: "white"
                pixelSize: 13
                onClicked: viewer.requestTrash(viewer.index)
            }
        }
    }

    // Previous / next arrows

    IconButton {
        anchors.left:           parent.left
        anchors.leftMargin:     12
        anchors.verticalCenter: parent.verticalCenter
        visible:                viewer.chrome && (viewer.index > 0)
        text:                   "‹"
        textColor:              "white"
        pixelSize:              34
        onClicked:              viewer.go(-1)
    }

    IconButton {
        anchors.right:          parent.right
        anchors.rightMargin:    12
        anchors.verticalCenter: parent.verticalCenter
        visible:                viewer.chrome && (viewer.index + 1 < library.count)
        text:                   "›"
        textColor:              "white"
        pixelSize:              34
        onClicked:              viewer.go(+1)
    }

    // --- Keyboard ---------------------------------------------------------------

    Keys.onPressed: (event) => {
        switch (event.key) {
            case Qt.Key_Escape:
            case Qt.Key_Backspace:
                if (viewer.zoom > 1.0)
                    viewer.resetZoom()
                else
                    viewer.close()
                break
            case Qt.Key_Right:
            case Qt.Key_Space:
                viewer.go(+1)
                break
            case Qt.Key_Left:
                viewer.go(-1)
                break
            case Qt.Key_Delete:
                viewer.requestTrash(viewer.index)
                break
            case Qt.Key_F:
            case Qt.Key_L:
                library.toggleFavoriteAt(viewer.index)
                break
            case Qt.Key_Plus:
            case Qt.Key_Equal:
                viewer.zoomAt(viewer.zoom * 1.5, stage.width / 2, stage.height / 2)
                break
            case Qt.Key_Minus:
                viewer.zoomAt(viewer.zoom / 1.5, stage.width / 2, stage.height / 2)
                break
            case Qt.Key_0:
                viewer.resetZoom()
                break
            default:
                return
        }

        event.accepted = true
        viewer.showChrome()
    }
}
