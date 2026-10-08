// SPDX-License-Identifier: GPL-2.0-or-later
// Photos mode - root item.
//
// Context properties provided by C++:
//   library   : PhotosLibraryModel (flat, date sorted photos of the current view)
//   grid      : PhotosGridModel    (sections and rows for the grid)
//   photosApp : PhotosContainer    (switch interface, open folder...)

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle {
    id: root

    color:  palette.window
    focus:  true

    readonly property color separatorColor: Qt.rgba(palette.windowText.r,
                                                    palette.windowText.g,
                                                    palette.windowText.b, 0.12)
    readonly property bool  selecting:      library.selectionCount > 0

    function requestTrash(row) {
        trashDialog.targetRow = row
        trashDialog.open()
    }

    function requestAddToAlbum(row) {
        albumPopup.targetRow = row
        albumPopup.open()
    }

    RowLayout {
        anchors.fill: parent
        spacing:      0

        Sidebar {
            id: sidebar
            Layout.fillHeight:     true
            Layout.preferredWidth: 210
        }

        Rectangle {
            Layout.fillHeight:     true
            Layout.preferredWidth: 1
            color:                 root.separatorColor
        }

        ColumnLayout {
            Layout.fillWidth:  true
            Layout.fillHeight: true
            spacing:           0

            // --- Title bar / selection bar -----------------------------------

            Rectangle {
                Layout.fillWidth:       true
                Layout.preferredHeight: 56
                color:                  root.selecting ? Qt.rgba(palette.highlight.r, palette.highlight.g,
                                                                 palette.highlight.b, 0.12)
                                                       : "transparent"

                Column {
                    visible:                !root.selecting
                    anchors.left:           parent.left
                    anchors.leftMargin:     16
                    anchors.verticalCenter: parent.verticalCenter
                    spacing:                1

                    Text {
                        text:           library.title
                        color:          palette.windowText
                        font.pixelSize: 20
                        font.bold:      true
                    }

                    Text {
                        text:           library.loading && (library.count === 0)
                                        ? qsTr("Loading…")
                                        : ((library.count === 1) ? qsTr("1 item")
                                                                 : qsTr("%L1 items").arg(library.count))
                        color:          palette.windowText
                        opacity:        0.6
                        font.pixelSize: 12
                    }
                }

                Row {
                    visible:                root.selecting
                    anchors.left:           parent.left
                    anchors.leftMargin:     8
                    anchors.verticalCenter: parent.verticalCenter
                    spacing:                8

                    IconButton {
                        text:      "✕"
                        tooltip:   qsTr("Clear selection")
                        onClicked: library.clearSelection()
                    }

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text:                   (library.selectionCount === 1) ? qsTr("1 selected")
                                                                               : qsTr("%L1 selected").arg(library.selectionCount)
                        color:                  palette.windowText
                        font.pixelSize:         18
                        font.bold:              true
                    }
                }

                Row {
                    anchors.right:          parent.right
                    anchors.rightMargin:    12
                    anchors.verticalCenter: parent.verticalCenter
                    spacing:                4

                    // Selection actions

                    IconButton {
                        visible:   root.selecting
                        readonly property bool allFavorite: (library.selectionRevision >= 0) &&
                                                            (library.revision >= 0) &&
                                                            library.selectionAllFavorite()
                        text:      allFavorite ? "♥" : "♡"
                        textColor: allFavorite ? "#e0455a" : palette.windowText
                        pixelSize: 20
                        tooltip:   allFavorite ? qsTr("Remove from Favorites") : qsTr("Add to Favorites")
                        onClicked: library.setFavoriteForSelection(!allFavorite)
                    }

                    IconButton {
                        visible:   root.selecting
                        text:      qsTr("Add to album")
                        pixelSize: 13
                        onClicked: root.requestAddToAlbum(-1)
                    }

                    IconButton {
                        visible:   root.selecting && (library.filter === 2)
                        text:      qsTr("Remove from album")
                        pixelSize: 13
                        onClicked: library.removeSelectionFromCurrentAlbum()
                    }

                    IconButton {
                        visible:   root.selecting
                        text:      qsTr("Move to trash")
                        pixelSize: 13
                        onClicked: root.requestTrash(-1)
                    }

                    // Zoom

                    IconButton {
                        visible:   !root.selecting
                        text:      "−"
                        tooltip:   qsTr("Smaller tiles")
                        onClicked: photoGrid.zoomStep(+1, photoGrid.height / 2)
                    }

                    IconButton {
                        visible:   !root.selecting
                        text:      "+"
                        tooltip:   qsTr("Larger tiles")
                        onClicked: photoGrid.zoomStep(-1, photoGrid.height / 2)
                    }
                }
            }

            Rectangle {
                Layout.fillWidth:       true
                Layout.preferredHeight: 1
                color:                  root.separatorColor
            }

            PhotoGrid {
                id: photoGrid
                Layout.fillWidth:  true
                Layout.fillHeight: true
                focus:             !viewer.visible

                onOpenPhoto:         (index) => viewer.open(index)
                onRequestTrash:      (row)   => root.requestTrash(row)
                onRequestAddToAlbum: (row)   => root.requestAddToAlbum(row)
            }
        }
    }

    Viewer {
        id: viewer
        anchors.fill: parent
        visible:      false

        onClosed: (index) => {
            photoGrid.ensureVisible(index)
            photoGrid.forceActiveFocus()
        }

        onRequestTrash:      (row) => root.requestTrash(row)
        onRequestAddToAlbum: (row) => root.requestAddToAlbum(row)
    }

    // --- Shared dialogs -------------------------------------------------------------

    AlbumPopup {
        id: albumPopup
        onClosed: (viewer.visible ? viewer : photoGrid).forceActiveFocus()
    }

    Popup {
        id: trashDialog

        property int targetRow: -1
        readonly property int itemCount: (targetRow >= 0) ? 1 : library.selectionCount

        anchors.centerIn: parent
        width:            360
        modal:            true
        focus:            true
        padding:          20

        background: Rectangle {
            color:  palette.window
            radius: 12
        }

        onClosed: (viewer.visible ? viewer : photoGrid).forceActiveFocus()

        function accept() {
            if (targetRow >= 0)
                library.trashAt(targetRow)
            else
                library.trashSelection()

            close()
        }

        Column {
            width:   parent.width
            spacing: 12

            Text {
                width:          parent.width
                wrapMode:       Text.WordWrap
                text:           (trashDialog.itemCount === 1) ? qsTr("Move this photo to the trash?")
                                                              : qsTr("Move %L1 photos to the trash?").arg(trashDialog.itemCount)
                color:          palette.windowText
                font.pixelSize: 16
                font.bold:      true
            }

            Text {
                width:          parent.width
                wrapMode:       Text.WordWrap
                text:           qsTr("They can be restored with Undo, or later from the trash of the classic interface.")
                color:          palette.windowText
                opacity:        0.7
            }

            Row {
                anchors.right: parent.right
                spacing:       8

                Button {
                    text:      qsTr("Cancel")
                    onClicked: trashDialog.close()
                }

                Button {
                    id: trashButton
                    text:      qsTr("Move to trash")
                    focus:     true
                    onClicked: trashDialog.accept()
                    Keys.onReturnPressed: trashDialog.accept()
                    Keys.onEnterPressed:  trashDialog.accept()
                }
            }
        }

        onOpened: trashButton.forceActiveFocus()
    }

    // --- Toast with Undo -------------------------------------------------------------

    Rectangle {
        id: toast

        property string message: ""

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom:           parent.bottom
        anchors.bottomMargin:     24
        width:                    toastRow.implicitWidth + 32
        height:                   44
        radius:                   22
        color:                    Qt.rgba(0.12, 0.12, 0.12, 0.92)
        opacity:                  toastTimer.running ? 1.0 : 0.0
        visible:                  opacity > 0
        z:                        10

        Behavior on opacity {
            NumberAnimation { duration: 180 }
        }

        function show(text) {
            message = text
            toastTimer.restart()
        }

        Timer {
            id: toastTimer
            interval: 8000
        }

        Row {
            id: toastRow
            anchors.centerIn: parent
            spacing:          16

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text:                   toast.message
                color:                  "white"
            }

            IconButton {
                anchors.verticalCenter: parent.verticalCenter
                visible:                library.canUndoTrash
                text:                   qsTr("Undo")
                textColor:              "#8ab4ff"
                pixelSize:              14
                onClicked: {
                    library.undoTrash()
                    toastTimer.stop()
                }
            }
        }
    }

    Connections {
        target: library

        function onTrashed(count) {
            toast.show((count === 1) ? qsTr("Moved 1 photo to the trash")
                                     : qsTr("Moved %L1 photos to the trash").arg(count))
        }
    }
}
