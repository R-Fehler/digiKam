// SPDX-License-Identifier: GPL-2.0-or-later
// Photos mode - date sectioned grid of square tiles separated by a thin seam.

import QtQuick
import QtQuick.Controls.Basic

FocusScope {
    id: gridRoot

    signal openPhoto(int index)

    readonly property int  gap:          2
    readonly property real cell:         Math.floor((list.width - (grid.columns - 1) * gap) / grid.columns)
    readonly property int  headerHeight: grid.byMonth ? 52 : 44

    // Zoom levels expressed as number of columns, Photos style.
    readonly property var  columnSteps:  [1, 2, 3, 4, 5, 6, 7, 9, 12, 16, 20, 26]

    property real pinchBase:     1.0
    property int  restoreId:     -1
    property bool scrollToTop:   false

    // --- Zoom -----------------------------------------------------------------

    function nearestStep(columns) {
        let best = 0
        for (let i = 0 ; i < columnSteps.length ; ++i) {
            if (Math.abs(columnSteps[i] - columns) < Math.abs(columnSteps[best] - columns))
                best = i
        }
        return best
    }

    // direction: +1 = more columns (smaller tiles), -1 = fewer columns (larger tiles)
    // anchorY:   vertical position in the view that should stay on the same photo
    function zoomStep(direction, anchorY) {
        const current = nearestStep(grid.columns)
        const next    = Math.max(0, Math.min(columnSteps.length - 1, current + direction))

        if (columnSteps[next] === grid.columns)
            return

        // Remember which photo is under the anchor point.

        const row      = list.indexAt(10, list.contentY + anchorY)
        const photo    = (row >= 0) ? grid.photoForRow(row) : -1
        const oldCell  = cell

        grid.columns = columnSteps[next]

        if (photo >= 0) {
            list.positionViewAtIndex(grid.rowForPhoto(photo), ListView.Beginning)
            list.contentY = Math.max(list.originY, list.contentY - anchorY + cell / 2)
            list.returnToBounds()
        }

        // Short scale animation from the old tile size: gives the feel
        // of zooming without re-rendering anything.

        zoomScale.origin.x = list.width / 2
        zoomScale.origin.y = anchorY
        zoomAnimation.from = oldCell / cell
        zoomAnimation.restart()
    }

    function ensureVisible(photoIndex) {
        const row = grid.rowForPhoto(photoIndex)
        if (row >= 0)
            list.positionViewAtIndex(row, ListView.Contain)
    }

    function topPhotoIndex() {
        const row = list.indexAt(10, list.contentY + headerHeight)
        return (row >= 0) ? grid.photoForRow(row) : -1
    }

    // Keep the scroll position across library reloads (new photos scanned,
    // metadata changed...), and go back to the top when the view changes.

    Connections {
        target: library

        function onAboutToReload() {
            gridRoot.restoreId = gridRoot.scrollToTop ? -1 : library.idAt(gridRoot.topPhotoIndex())
        }

        function onReloaded() {
            if (gridRoot.scrollToTop || (gridRoot.restoreId < 0)) {
                list.positionViewAtBeginning()
            }
            else {
                const photo = library.rowOfId(gridRoot.restoreId)
                if (photo >= 0)
                    list.positionViewAtIndex(grid.rowForPhoto(photo), ListView.Beginning)
            }

            gridRoot.scrollToTop = false
        }

        function onFilterChanged() {
            gridRoot.scrollToTop = true
        }
    }

    // --- Grid -----------------------------------------------------------------

    ListView {
        id: list

        anchors.fill:          parent
        anchors.leftMargin:    0
        clip:                  true
        focus:                 true
        model:                 grid
        reuseItems:            true
        cacheBuffer:           Math.max(0, height)
        boundsBehavior:        Flickable.StopAtBounds
        flickDeceleration:     4000
        maximumFlickVelocity:  8000
        pixelAligned:          true

        transform: Scale {
            id: zoomScale
            xScale: 1.0
            yScale: xScale
        }

        NumberAnimation {
            id:          zoomAnimation
            target:      zoomScale
            property:    "xScale"
            to:          1.0
            duration:    180
            easing.type: Easing.OutCubic
        }

        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AsNeeded
        }

        delegate: Item {
            id: rowItem

            required property int    index
            required property int    rowType
            required property int    first
            required property int    count
            required property string title

            width:  list.width
            height: (rowType === 0) ? gridRoot.headerHeight : (gridRoot.cell + gridRoot.gap)

            Text {
                visible:              rowItem.rowType === 0
                anchors.left:         parent.left
                anchors.leftMargin:   16
                anchors.bottom:       parent.bottom
                anchors.bottomMargin: 10
                text:                 rowItem.title
                color:                palette.windowText
                font.pixelSize:       grid.byMonth ? 18 : 15
                font.bold:            true
            }

            Row {
                visible: rowItem.rowType === 1
                spacing: gridRoot.gap

                Repeater {
                    model: (rowItem.rowType === 1) ? rowItem.count : 0

                    delegate: PhotoTile {
                        required property int index

                        width:       gridRoot.cell
                        height:      gridRoot.cell
                        photoIndex:  rowItem.first + index

                        onActivated: (photoIndex) => gridRoot.openPhoto(photoIndex)
                    }
                }
            }
        }

        // Ctrl + wheel (or pinch on touchpads) changes the tile size.

        WheelHandler {
            acceptedModifiers: Qt.ControlModifier
            onWheel: (event) => {
                if (event.angleDelta.y !== 0)
                    gridRoot.zoomStep(event.angleDelta.y > 0 ? -1 : +1, point.position.y)
            }
        }

        PinchHandler {
            target: null

            onActiveChanged: gridRoot.pinchBase = 1.0

            onActiveScaleChanged: {
                const ratio = activeScale / gridRoot.pinchBase

                if (ratio > 1.25) {
                    gridRoot.zoomStep(-1, centroid.position.y)
                    gridRoot.pinchBase = activeScale
                }
                else if (ratio < 0.8) {
                    gridRoot.zoomStep(+1, centroid.position.y)
                    gridRoot.pinchBase = activeScale
                }
            }
        }
    }

    Shortcut {
        sequences: [StandardKey.ZoomIn, "Ctrl+="]
        onActivated: gridRoot.zoomStep(-1, list.height / 2)
    }

    Shortcut {
        sequences: [StandardKey.ZoomOut]
        onActivated: gridRoot.zoomStep(+1, list.height / 2)
    }

    Keys.onPressed: (event) => {
        if (event.key === Qt.Key_Home) {
            list.positionViewAtBeginning()
            event.accepted = true
        }
        else if (event.key === Qt.Key_End) {
            list.positionViewAtEnd()
            event.accepted = true
        }
        else if (event.key === Qt.Key_PageDown) {
            list.contentY = Math.min(list.contentY + list.height * 0.9,
                                     list.originY + list.contentHeight - list.height)
            event.accepted = true
        }
        else if (event.key === Qt.Key_PageUp) {
            list.contentY = Math.max(list.originY, list.contentY - list.height * 0.9)
            event.accepted = true
        }
    }

    // --- Empty / loading states ------------------------------------------------

    Column {
        anchors.centerIn: parent
        spacing:          8
        visible:          (library.count === 0) && !library.loading

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text:                     qsTr("No photos here yet")
            color:                    palette.windowText
            font.pixelSize:           18
            font.bold:                true
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text:                     (library.filter === 1)
                                      ? qsTr("Mark photos with ♥ to see them here.")
                                      : qsTr("Photos in your digiKam collections appear here.")
            color:                    palette.windowText
            opacity:                  0.6
        }
    }

    BusyIndicator {
        anchors.centerIn: parent
        running:          library.loading && (library.count === 0)
        visible:          running
    }
}
