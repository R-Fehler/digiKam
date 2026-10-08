// SPDX-License-Identifier: GPL-2.0-or-later
// Photos mode - full window photo viewer with GPU zoom and pan.

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
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

    // Scrolling through the thumbnail strip: show thumbnails only, decode the
    // preview of the photo the strip stops on.
    readonly property bool scrubbing: filmstrip.scrubbing

    readonly property real maxZoom:     12.0
    readonly property int  previewSize: Math.min(3840, Math.ceil(Math.max(Screen.width, Screen.height)
                                                                 * Screen.devicePixelRatio))
    readonly property bool favorite:    (library.revision >= 0) && library.isFavoriteAt(index)
    readonly property bool video:       (library.revision >= 0) && library.isVideoAt(index)

    signal closed(int index)
    signal requestTrash(int row)
    signal requestAddToAlbum(int row)
    signal requestDeleteForever(int row)

    /// Info panel at the right (button, I key); kept while going through photos.
    property bool infoOpen: false

    readonly property bool trashView:  library.filter === 12
    readonly property bool hiddenView: library.filter === 11

    // Inline video player, when the Qt Multimedia QML module is available.
    readonly property var  videoPlayer: (videoLoader.status === Loader.Ready) ? videoLoader.item : null
    readonly property bool inlineVideo: viewer.video && (videoPlayer !== null) && !videoPlayer.failed

    // Live Photo: the motion plays over the photo (LIVE button).
    readonly property url  liveUrl:     (library.revision >= 0) ? library.liveUrlAt(index) : ""
    property bool          livePlaying: false

    function formatTime(ms) {
        const s = Math.floor(Math.max(0, ms) / 1000)
        const m = Math.floor(s / 60)
        return (m >= 60) ? "%1:%2:%3".arg(Math.floor(m / 60)).arg(String(m % 60).padStart(2, "0")).arg(String(s % 60).padStart(2, "0"))
                         : "%1:%2".arg(m).arg(String(s % 60).padStart(2, "0"))
    }

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
        filmstrip.center()
        prefetchTimer.restart()
        resetZoom()
        forceActiveFocus()
        showChrome()
    }

    function close() {
        visible = false
        photosApp.prefetchPreviews(-1, 0)       // drop the neighbours being decoded
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

    // Neighbours are decoded ahead in C++ (PhotosPreviewLoader), so going to
    // them is instant. Slightly delayed: skipping through photos quickly (keys,
    // filmstrip) does not queue decodes for every photo passed.
    onIndexChanged: {
        livePlaying = false

        if (visible)
            prefetchTimer.restart()
    }
    onScrubbingChanged: if (!scrubbing && visible) prefetchTimer.restart()

    Timer {
        id: prefetchTimer
        interval: 120
        onTriggered: if (viewer.visible && !viewer.scrubbing) photosApp.prefetchPreviews(viewer.index, viewer.previewSize)
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
        anchors.left:   parent.left
        anchors.top:    parent.top
        anchors.bottom: parent.bottom
        anchors.right:  viewer.infoOpen ? infoPanel.left : parent.right
        clip:           true

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

            // 2. Screen sized preview (embedded preview when large enough),
            //    usually already decoded ahead (see prefetchPreviews()). Shown
            //    at about its own size: no mipmaps to generate on each photo.
            Image {
                id: previewLayer
                anchors.fill: parent
                source:       (viewer.index >= 0) && !viewer.video && !viewer.scrubbing
                              ? library.previewSourceAt(viewer.index, viewer.previewSize) : ""
                fillMode:     Image.PreserveAspectFit
                asynchronous: true
                smooth:       true
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

            // 4. Motion of a Live Photo, over the photo while it plays.
            Loader {
                id: liveLoader
                anchors.fill: parent
                active:       viewer.visible && viewer.livePlaying && (viewer.liveUrl.toString().length > 0)
                source:       "VideoPlayer.qml"

                onLoaded: item.source = viewer.liveUrl

                Connections {
                    target: liveLoader.item

                    function onPlayingChanged() {
                        if (liveLoader.item && !liveLoader.item.playing && liveLoader.item.started)
                            viewer.livePlaying = false
                    }

                    function onFailedChanged() {
                        if (liveLoader.item && liveLoader.item.failed)
                            viewer.livePlaying = false
                    }
                }
            }

            // 5. Video, played inline (Qt Multimedia); the thumbnail stays as poster frame.
            Loader {
                id: videoLoader
                anchors.fill: parent
                active:       viewer.visible && viewer.video
                source:       "VideoPlayer.qml"

                onLoaded: item.source = library.fileUrlAt(viewer.index)

                Connections {
                    target: viewer

                    function onIndexChanged() {
                        if (videoLoader.item && viewer.video)
                            videoLoader.item.source = library.fileUrlAt(viewer.index)
                    }
                }
            }
        }

        BusyIndicator {
            anchors.centerIn: parent
            running:          viewer.visible && !viewer.video && (previewLayer.status === Image.Loading)
                              && (thumbLayer.status !== Image.Ready)
            visible:          running
        }

        // Video: big play button while paused. Without inline playback, hand
        // the video to the system player.
        Rectangle {
            visible:          viewer.video && (!viewer.inlineVideo || !viewer.videoPlayer.playing)
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
                onClicked: {
                    if (viewer.inlineVideo)
                        viewer.videoPlayer.toggle()
                    else
                        photosApp.openExternally(library.filePathAt(viewer.index))
                }
            }
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
            onSingleTapped: {
                if (viewer.inlineVideo)
                    viewer.videoPlayer.toggle()
            }

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
        onTriggered: {
            if (filmstrip.hovered || filmstrip.scrubbing)
                restart()
            else
                viewer.chrome = false
        }
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

            Rectangle {
                visible:                viewer.liveUrl.toString().length > 0
                anchors.verticalCenter: parent.verticalCenter
                width:                  liveText.implicitWidth + 16
                height:                 22
                radius:                 11
                color:                  viewer.livePlaying ? "white" : Qt.rgba(1, 1, 1, 0.18)

                Text {
                    id: liveText
                    anchors.centerIn: parent
                    text:             "\u25CE LIVE"
                    color:            viewer.livePlaying ? "black" : "white"
                    font.pixelSize:   11
                    font.bold:        true
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape:  Qt.PointingHandCursor
                    onClicked:    viewer.livePlaying = !viewer.livePlaying
                }
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
                text:      qsTr("Info")
                textColor: "white"
                pixelSize: 13
                opacity:   viewer.infoOpen ? 1.0 : 0.7
                onClicked: viewer.infoOpen = !viewer.infoOpen
            }

            IconButton {
                text:      qsTr("Thumbnails")
                textColor: "white"
                pixelSize: 13
                opacity:   photosApp.filmstrip ? 1.0 : 0.55
                onClicked: photosApp.filmstrip = !photosApp.filmstrip
            }

            // Recently Deleted: restore or delete for good.

            IconButton {
                visible:   viewer.trashView
                text:      qsTr("Restore")
                textColor: "white"
                pixelSize: 13
                onClicked: library.restoreAt(viewer.index)
            }

            IconButton {
                visible:   viewer.trashView
                text:      qsTr("Delete permanently")
                textColor: "#ff8a8a"
                pixelSize: 13
                onClicked: viewer.requestDeleteForever(viewer.index)
            }

            // Everywhere else.

            IconButton {
                visible:   !viewer.trashView
                text:      viewer.favorite ? "♥" : "♡"
                textColor: viewer.favorite ? "#ff5a6e" : "white"
                pixelSize: 20
                onClicked: library.toggleFavoriteAt(viewer.index)
            }

            IconButton {
                visible:   !viewer.trashView
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
                id: moreButton
                visible:   !viewer.trashView
                text:      "\u22EF"
                textColor: "white"
                pixelSize: 18
                tooltip:   qsTr("More")
                onClicked: moreMenu.popup(moreButton, 0, moreButton.height)
            }

            IconButton {
                visible:   !viewer.trashView
                text:      qsTr("Move to trash")
                textColor: "white"
                pixelSize: 13
                onClicked: viewer.requestTrash(viewer.index)
            }
        }

        Menu {
            id: moreMenu

            MenuItem {
                text:        viewer.hiddenView ? qsTr("Unhide") : qsTr("Hide")
                onTriggered: library.setHiddenAt(viewer.index, !viewer.hiddenView)
            }

            MenuItem {
                text:        qsTr("Show in folder")
                onTriggered: photosApp.openContainingFolder(library.filePathAt(viewer.index))
            }

            MenuItem {
                text:        viewer.video ? qsTr("Open in video player") : qsTr("Open with another app")
                onTriggered: photosApp.openExternally(library.filePathAt(viewer.index))
            }
        }
    }

    // Info panel

    InfoPanel {
        id: infoPanel

        anchors.right:  parent.right
        anchors.top:    topBar.bottom
        anchors.bottom: parent.bottom
        width:          Math.min(340, viewer.width * 0.4)
        visible:        viewer.infoOpen
        row:            viewer.index
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
        anchors.right:          stage.right
        anchors.rightMargin:    12
        anchors.verticalCenter: parent.verticalCenter
        visible:                viewer.chrome && (viewer.index + 1 < library.count)
        text:                   "›"
        textColor:              "white"
        pixelSize:              34
        onClicked:              viewer.go(+1)
    }

    // Thumbnail strip, shown with the other controls (toggle: button, T).

    // Video controls, above the thumbnail strip.

    Rectangle {
        id: videoBar

        anchors.left:         stage.left
        anchors.right:        stage.right
        anchors.bottom:       filmstrip.visible ? filmstrip.top : parent.bottom
        anchors.leftMargin:   Math.max(16, stage.width * 0.15)
        anchors.rightMargin:  Math.max(16, stage.width * 0.15)
        anchors.bottomMargin: 8
        height:               40
        radius:               20
        color:                Qt.rgba(0, 0, 0, 0.55)
        visible:              viewer.inlineVideo && viewer.chrome

        RowLayout {
            anchors.fill:        parent
            anchors.leftMargin:  12
            anchors.rightMargin: 14
            spacing:             10

            IconButton {
                text:      (viewer.videoPlayer && viewer.videoPlayer.playing) ? "\u275A\u275A" : "\u25B6"
                textColor: "white"
                pixelSize: 14
                onClicked: viewer.videoPlayer.toggle()
            }

            Text {
                text:           viewer.videoPlayer ? viewer.formatTime(viewer.videoPlayer.position) : ""
                color:          "white"
                font.pixelSize: 12
                font.family:    "monospace"
            }

            Slider {
                id: seekSlider
                Layout.fillWidth: true
                from:             0
                to:               viewer.videoPlayer ? Math.max(1, viewer.videoPlayer.duration) : 1
                value:            (viewer.videoPlayer && !pressed) ? viewer.videoPlayer.position : value
                onMoved:          if (viewer.videoPlayer) viewer.videoPlayer.seek(value)
                onPressedChanged: viewer.showChrome()
            }

            Text {
                text:           viewer.videoPlayer ? viewer.formatTime(viewer.videoPlayer.duration) : ""
                color:          "white"
                opacity:        0.7
                font.pixelSize: 12
                font.family:    "monospace"
            }

            IconButton {
                text:      (viewer.videoPlayer && viewer.videoPlayer.muted) ? qsTr("Unmute") : qsTr("Mute")
                textColor: "white"
                pixelSize: 12
                onClicked: viewer.videoPlayer.muted = !viewer.videoPlayer.muted
            }
        }
    }

    Filmstrip {
        id: filmstrip

        anchors.left:   stage.left
        anchors.right:  stage.right
        anchors.bottom: parent.bottom

        currentIndex:   viewer.index
        opacity:        (photosApp.filmstrip && viewer.chrome) ? 1.0 : 0.0
        visible:        opacity > 0.0

        Behavior on opacity {
            NumberAnimation { duration: 200 }
        }

        onActivated: (i) => {
            viewer.index = i
            viewer.resetZoom()
            viewer.showChrome()
        }

        onScrubbingChanged: viewer.showChrome()
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
            case Qt.Key_Space:
            case Qt.Key_K:
                if (viewer.inlineVideo)
                    viewer.videoPlayer.toggle()
                else
                    viewer.go(+1)
                break
            case Qt.Key_Right:
                viewer.go(+1)
                break
            case Qt.Key_Left:
                viewer.go(-1)
                break
            case Qt.Key_Delete:
                if (viewer.trashView)
                    viewer.requestDeleteForever(viewer.index)
                else
                    viewer.requestTrash(viewer.index)
                break
            case Qt.Key_F:
            case Qt.Key_L:
                if (!viewer.trashView)
                    library.toggleFavoriteAt(viewer.index)
                break
            case Qt.Key_I:
                viewer.infoOpen = !viewer.infoOpen
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
            case Qt.Key_T:
                photosApp.filmstrip = !photosApp.filmstrip
                break
            default:
                return
        }

        event.accepted = true
        viewer.showChrome()
    }
}
