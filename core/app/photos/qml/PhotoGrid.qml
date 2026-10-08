// SPDX-License-Identifier: GPL-2.0-or-later
// Photos mode - date sectioned grid of square tiles separated by a thin seam.
//
// Mouse interaction (one overlay handles it all, tiles are display only):
//   click                 open the photo (or toggle it when a selection exists)
//   click on check circle toggle selection
//   Ctrl + click          toggle selection
//   Shift + click         select range from the last toggled photo
//   drag                  rectangle selection (Ctrl/Shift: add), auto-scrolls
//   double click          open the photo
//   right click           context menu
//   Ctrl + wheel / pinch  zoom (number of columns)

import QtQuick
import QtQuick.Controls.Basic

FocusScope {
    id: gridRoot

    signal openPhoto(int index)

    /// row >= 0: that photo only; row == -1: the selection.
    signal requestTrash(int row)
    signal requestAddToAlbum(int row)

    readonly property int  gap:          2
    // Horizontal pitch of one tile including the seam. Tiles are placed at
    // rounded multiples of it, so the columns fill the width exactly.
    readonly property real pitch:        (list.width + gap) / grid.columns
    readonly property real cell:         pitch - gap
    readonly property int  headerHeight: grid.byMonth ? 52 : 44
    readonly property bool selectionMode: library.selectionCount > 0

    // Zoom levels expressed as number of columns, Photos style.
    readonly property var  columnSteps:  [1, 2, 3, 4, 5, 6, 7, 9, 12, 16, 20, 26]

    property real pinchBase:     1.0
    property int  restoreId:     -1
    property int  restoreIndex:  -1
    property bool scrollToTop:   false
    property int  hoverPhoto:    -1

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

    // Converts a position in the list content item to a y in the visible view.
    function viewY(contentPosition) {
        return list.mapFromItem(list.contentItem, contentPosition.x, contentPosition.y).y
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

    // --- Hit testing (view coordinates of the list) ---------------------------

    function columnAt(x) {
        return Math.max(0, Math.min(grid.columns - 1, Math.floor(x / pitch)))
    }

    function rowAt(y) {
        const row = list.indexAt(1, list.contentY + y)
        if (row >= 0)
            return row
        // Below the last row, or in a gap: clamp.
        return (y < 0) ? 0 : (list.count - 1)
    }

    function photoAtView(x, y) {
        const row = list.indexAt(1, list.contentY + y)
        return (row >= 0) ? grid.photoAt(row, Math.floor(x / pitch)) : -1
    }

    // Is the point on the check circle of the tile under it?
    function onCheckCircle(x, y) {
        const row  = list.indexAt(1, list.contentY + y)
        const item = (row >= 0) ? list.itemAtIndex(row) : null

        if (!item)
            return false

        const size   = (cell >= 80) ? 22 : 16
        const localX = x - Math.round(Math.floor(x / pitch) * pitch)
        const localY = (list.contentY + y) - item.y

        return (localX <= size + 12) && (localY <= size + 12)
    }

    // Keep the scroll position across library reloads (new photos scanned,
    // metadata changed, photos deleted...), and go back to the top when the
    // view changes.

    Connections {
        target: library

        function onAboutToReload() {
            const top = gridRoot.topPhotoIndex()

            // At the top: stay at the top, so newly added (newest) photos show up.
            gridRoot.restoreId    = (gridRoot.scrollToTop || list.atYBeginning) ? -1 : library.idAt(top)
            gridRoot.restoreIndex = top
        }

        function onReloaded() {
            if (gridRoot.scrollToTop || (gridRoot.restoreId < 0)) {
                list.positionViewAtBeginning()
            }
            else {
                let photo = library.rowOfId(gridRoot.restoreId)

                // The photo at the top was removed: stay around the same place.
                if (photo < 0)
                    photo = Math.min(gridRoot.restoreIndex, library.count - 1)

                if (photo >= 0)
                    list.positionViewAtIndex(grid.rowForPhoto(photo), ListView.Beginning)
            }

            gridRoot.scrollToTop = false
        }

        function onFilterChanged() {
            gridRoot.scrollToTop = true
            library.clearSelection()
        }
    }

    // --- Grid -----------------------------------------------------------------

    ListView {
        id: list

        anchors.fill:          parent
        clip:                  true
        focus:                 true
        model:                 grid
        reuseItems:            true

        // Rows within 1.5 screens above and below are instantiated, so their
        // thumbnails are already requested before they scroll into view.
        cacheBuffer:           Math.max(0, Math.round(height * 1.5))

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
            id: scrollBar
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
            height: (rowType === 0) ? gridRoot.headerHeight : Math.round(gridRoot.pitch)

            readonly property bool nearView: (y + height > list.contentY - list.height * 0.5) &&
                                             (y < list.contentY + list.height * 1.5)

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

            Item {
                visible: rowItem.rowType === 1

                Repeater {
                    model: (rowItem.rowType === 1) ? rowItem.count : 0

                    delegate: PhotoTile {
                        required property int index

                        x:             Math.round(index * gridRoot.pitch)
                        width:         Math.round((index + 1) * gridRoot.pitch - gridRoot.gap) - x
                        height:        Math.round(gridRoot.pitch) - gridRoot.gap
                        photoIndex:    rowItem.first + index
                        hovered:       gridRoot.hoverPhoto === photoIndex
                        selectionMode: gridRoot.selectionMode
                        nearView:      rowItem.nearView
                    }
                }
            }
        }

        // Ctrl + wheel (or pinch on touchpads) changes the tile size.
        // Handlers declared in a Flickable live in its content item: their
        // positions are content coordinates, see viewY().

        WheelHandler {
            acceptedModifiers: Qt.ControlModifier
            onWheel: (event) => {
                if (event.angleDelta.y !== 0)
                    gridRoot.zoomStep(event.angleDelta.y > 0 ? -1 : +1, gridRoot.viewY(point.position))
            }
        }

        PinchHandler {
            target: null

            onActiveChanged: gridRoot.pinchBase = 1.0

            onActiveScaleChanged: {
                const ratio = activeScale / gridRoot.pinchBase

                if (ratio > 1.25) {
                    gridRoot.zoomStep(-1, gridRoot.viewY(centroid.position))
                    gridRoot.pinchBase = activeScale
                }
                else if (ratio < 0.8) {
                    gridRoot.zoomStep(+1, gridRoot.viewY(centroid.position))
                    gridRoot.pinchBase = activeScale
                }
            }
        }
    }

    // --- Mouse: clicks, selection, drag-select, context menu ---------------------
    // Wheel events are not handled here and reach the list below.

    MouseArea {
        id: pointer

        property real pressX:       0
        property real pressY:       0      // content coordinates
        property int  pressRow:     -1
        property int  pressColumn:  0
        property int  pressPhoto:   -1
        property bool banding:      false
        property bool suppressClick: false   // the press turned into a drag-select
        property real lastX:        0
        property real lastY:        0      // view coordinates

        anchors.fill:        list
        anchors.rightMargin: scrollBar.visible ? scrollBar.width : 0
        acceptedButtons:     Qt.LeftButton | Qt.RightButton
        hoverEnabled:        true

        function updateBand() {
            const row = gridRoot.rowAt(lastY)
            const col = gridRoot.columnAt(lastX)

            library.updateBandSelection(grid.photosInBlock(pressRow, row, pressColumn, col))
        }

        onPressed: (mouse) => {
            gridRoot.forceActiveFocus()

            pressX      = mouse.x
            pressY      = list.contentY + mouse.y
            pressRow    = gridRoot.rowAt(mouse.y)
            pressColumn = gridRoot.columnAt(mouse.x)
            pressPhoto  = gridRoot.photoAtView(mouse.x, mouse.y)
            banding       = false
            suppressClick = false
            lastX         = mouse.x
            lastY       = mouse.y
        }

        onPositionChanged: (mouse) => {
            lastX = mouse.x
            lastY = mouse.y

            if (!pressed) {
                gridRoot.hoverPhoto = gridRoot.photoAtView(mouse.x, mouse.y)
                return
            }

            if (!(pressedButtons & Qt.LeftButton))
                return

            if (!banding) {
                const dx = mouse.x - pressX
                const dy = (list.contentY + mouse.y) - pressY

                if (Math.sqrt(dx * dx + dy * dy) < 8)
                    return

                banding       = true
                suppressClick = true
                library.beginBandSelection(mouse.modifiers & (Qt.ControlModifier | Qt.ShiftModifier))
            }

            updateBand()
        }

        onReleased: banding = false

        onExited: gridRoot.hoverPhoto = -1

        onClicked: (mouse) => {
            if (suppressClick)
                return

            const photo = pressPhoto

            if (mouse.button === Qt.RightButton) {
                contextMenu.targetRow = photo
                if (photo >= 0)
                    contextMenu.popup()
                return
            }

            if (photo < 0) {
                if (!(mouse.modifiers & (Qt.ControlModifier | Qt.ShiftModifier)))
                    library.clearSelection()
                return
            }

            if (mouse.modifiers & Qt.ShiftModifier)
                library.selectRangeTo(photo)
            else if ((mouse.modifiers & Qt.ControlModifier) || gridRoot.selectionMode ||
                     gridRoot.onCheckCircle(mouse.x, mouse.y))
                library.toggleSelectedAt(photo)
            else
                gridRoot.openPhoto(photo)
        }

        onDoubleClicked: (mouse) => {
            const photo = gridRoot.photoAtView(mouse.x, mouse.y)

            if ((photo < 0) || (mouse.button !== Qt.LeftButton))
                return

            // In selection mode the first click toggled the photo: undo that.
            if (gridRoot.selectionMode || (mouse.modifiers & Qt.ControlModifier))
                library.toggleSelectedAt(photo)

            gridRoot.openPhoto(photo)
        }

        // Rubber band, drawn in view coordinates.

        Rectangle {
            visible:      pointer.banding
            x:            Math.min(pointer.pressX, pointer.lastX)
            y:            Math.min(pointer.pressY - list.contentY, pointer.lastY)
            width:        Math.abs(pointer.lastX - pointer.pressX)
            height:       Math.abs(pointer.lastY - (pointer.pressY - list.contentY))
            color:        Qt.rgba(palette.highlight.r, palette.highlight.g, palette.highlight.b, 0.15)
            border.color: palette.highlight
            border.width: 1
        }

        // Auto-scroll while drag-selecting near (or beyond) the top / bottom edge.

        Timer {
            interval: 16
            repeat:   true
            running:  pointer.banding

            onTriggered: {
                const margin = 48
                let speed    = 0

                if (pointer.lastY < margin)
                    speed = -Math.min(40, (margin - pointer.lastY) / 2)
                else if (pointer.lastY > pointer.height - margin)
                    speed = Math.min(40, (pointer.lastY - (pointer.height - margin)) / 2)

                if (speed === 0)
                    return

                const maxY    = list.originY + list.contentHeight - list.height
                list.contentY = Math.max(list.originY, Math.min(maxY, list.contentY + speed))
                pointer.updateBand()
            }
        }
    }

    // --- Context menu ---------------------------------------------------------------

    Menu {
        id: contextMenu

        property int targetRow: -1

        // Acts on the selection when the clicked photo is part of it,
        // otherwise on the clicked photo only (without changing the selection).
        readonly property bool onSelection: (library.selectionRevision >= 0) && library.isSelectedAt(targetRow)
        readonly property int  itemCount:   onSelection ? library.selectionCount : 1
        readonly property bool allFavorite: onSelection ? library.selectionAllFavorite()
                                                        : library.isFavoriteAt(targetRow)

        MenuItem {
            text:        qsTr("Open")
            onTriggered: gridRoot.openPhoto(contextMenu.targetRow)
        }

        MenuItem {
            text:        contextMenu.allFavorite ? qsTr("Remove from Favorites") : qsTr("Add to Favorites")
            onTriggered: {
                if (contextMenu.onSelection)
                    library.setFavoriteForSelection(!contextMenu.allFavorite)
                else
                    library.toggleFavoriteAt(contextMenu.targetRow)
            }
        }

        MenuItem {
            text:        qsTr("Add to album…")
            onTriggered: gridRoot.requestAddToAlbum(contextMenu.onSelection ? -1 : contextMenu.targetRow)
        }

        MenuItem {
            visible:     library.filter === 2
            height:      visible ? implicitHeight : 0
            text:        qsTr("Remove from this album")
            onTriggered: {
                if (contextMenu.onSelection)
                    library.removeSelectionFromCurrentAlbum()
                else
                    library.removeFromCurrentAlbum(contextMenu.targetRow)
            }
        }

        MenuItem {
            text:        library.isSelectedAt(contextMenu.targetRow) ? qsTr("Deselect") : qsTr("Select")
            onTriggered: library.toggleSelectedAt(contextMenu.targetRow)
        }

        MenuItem {
            visible:     !contextMenu.onSelection
            height:      visible ? implicitHeight : 0
            text:        qsTr("Show in folder")
            onTriggered: photosApp.openContainingFolder(library.filePathAt(contextMenu.targetRow))
        }

        MenuSeparator { }

        MenuItem {
            text:        (contextMenu.itemCount > 1) ? qsTr("Move %1 items to trash").arg(contextMenu.itemCount)
                                                     : qsTr("Move to trash")
            onTriggered: gridRoot.requestTrash(contextMenu.onSelection ? -1 : contextMenu.targetRow)
        }
    }

    // --- Keyboard ---------------------------------------------------------------------

    Keys.onPressed: (event) => {
        if ((event.key === Qt.Key_A) && (event.modifiers & Qt.ControlModifier)) {
            library.selectAll()
            event.accepted = true
        }
        else if ((event.key === Qt.Key_Escape) && gridRoot.selectionMode) {
            library.clearSelection()
            event.accepted = true
        }
        else if ((event.key === Qt.Key_Delete) && gridRoot.selectionMode) {
            gridRoot.requestTrash(-1)
            event.accepted = true
        }
        else if ((event.key === Qt.Key_Plus) || (event.key === Qt.Key_Equal)) {
            gridRoot.zoomStep(-1, list.height / 2)
            event.accepted = true
        }
        else if (event.key === Qt.Key_Minus) {
            gridRoot.zoomStep(+1, list.height / 2)
            event.accepted = true
        }
        else if (event.key === Qt.Key_Home) {
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
