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
    readonly property bool  importsPage:    sidebar.page === "imports"
    readonly property bool  selecting:      (library.selectionCount > 0) && !importsPage
    readonly property bool  trashView:      library.filter === 12
    readonly property bool  hiddenView:     library.filter === 11

    // Show the background preparation only when it lasts (quick passes over
    // an already prepared library would just flicker).
    QtObject {
        id: preparingShown
        property bool value: false
    }

    Timer {
        interval: 2000
        running:  photosApp.preparingPercent >= 0
        onTriggered: preparingShown.value = true
    }

    Connections {
        target: photosApp

        function onSignalPreparingChanged() {
            if (photosApp.preparingPercent < 0)
                preparingShown.value = false
        }
    }

    function requestTrash(row) {
        trashDialog.targetRow = row
        trashDialog.open()
    }

    function requestAddToAlbum(row) {
        albumPopup.targetRow = row
        albumPopup.open()
    }

    // Generic confirmation: title, text, button text and what to do.
    function confirm(title, text, actionText, action) {
        confirmDialog.title      = title
        confirmDialog.text       = text
        confirmDialog.actionText = actionText
        confirmDialog.action     = action
        confirmDialog.open()
    }

    function requestDeleteForever(row) {
        const count = (row >= 0) ? 1 : library.selectionCount

        confirm((count === 1) ? qsTr("Delete this photo permanently?")
                              : qsTr("Delete %L1 photos permanently?").arg(count),
                qsTr("This cannot be undone."),
                qsTr("Delete"),
                () => {
                    if (row >= 0)
                        library.deleteForeverAt(row)
                    else
                        library.deleteSelectionForever()
                })
    }

    function requestUndoImport(importId) {
        const count = importer.remainingCount(importId)

        if (count === 0) {
            confirm(qsTr("Nothing to undo"),
                    qsTr("The photos of this import are no longer in your library."),
                    qsTr("OK"), null)
            return
        }

        confirm(qsTr("Undo this import?"),
                (count === 1) ? qsTr("The photo of this import still in your library is moved to Recently Deleted.")
                              : qsTr("The %L1 photos of this import still in your library are moved to Recently Deleted.").arg(count),
                qsTr("Undo import"),
                () => {
                    importer.undoImport(importId)
                    sidebar.page = "imports"
                })
    }

    // --- Opening folders ------------------------------------------------------------

    /// Photo to open in the viewer once its folder is listed (opened from the command line).
    property string pendingOpenFile: ""

    function showFolder(path, file) {
        if (viewer.visible)
            viewer.close()

        sidebar.page    = "grid"
        pendingOpenFile = file || ""
        library.showFolder(path)
        pendingTimer.restart()
    }

    function addFolder(path, file) {
        const error = libraries.addFolder(path)

        if (error.length > 0)
            confirm(qsTr("The folder could not be added"), error, qsTr("OK"), null)
        else
            showFolder(path, file)
    }

    function requestAddFolder() {
        const folder = libraries.chooseFolder()

        if (folder.length > 0)
            open(libraries.checkPath(folder))
    }

    // target: PhotosLibraries::checkPath()
    function open(target) {
        if (target.inLibrary) {
            showFolder(target.path, target.file)
        }
        else if (target.canAdd) {
            confirm(qsTr("Add \u201C%1\u201D to your library?").arg(target.name),
                    qsTr("Its photos and videos appear in Photos, with its subfolders. The files stay where they are; "
                         + "favorites, albums and captions are saved in sidecar files next to them.")
                    + (target.message ? "\n\n" + target.message : ""),
                    qsTr("Add to library"),
                    () => root.addFolder(target.path, target.file))
        }
        else {
            confirm(qsTr("This folder cannot be opened"),
                    target.message || qsTr("It cannot be added to the library."),
                    qsTr("OK"), null)
        }
    }

    function requestRemoveFolder(folderId, name) {
        confirm(qsTr("Remove \u201C%1\u201D from the library?").arg(name),
                qsTr("The files are not deleted. Its photos leave Photos; information saved in sidecar "
                     + "files comes back if you add the folder again."),
                qsTr("Remove"),
                () => {
                    if (library.filter === 13)
                        sidebar.show(0)

                    libraries.removeFolder(folderId)
                })
    }

    // The photo appears once listed; a folder just added is scanned first.
    Timer {
        id: pendingTimer
        interval: 60000
        onTriggered: root.pendingOpenFile = ""
    }

    Connections {
        target: photosApp

        function onOpenRequested(target) {
            root.open(target)
        }
    }

    Connections {
        target: library

        function onReloaded() {
            if ((root.pendingOpenFile.length === 0) || (library.filter !== 13))
                return

            const row = library.rowOfPath(root.pendingOpenFile)

            if (row >= 0) {
                root.pendingOpenFile = ""
                viewer.open(row)
            }
        }
    }

    function showImport(importId) {
        sidebar.page = "grid"
        importer.showImport(importId)
    }

    RowLayout {
        anchors.fill: parent
        spacing:      0

        Sidebar {
            id: sidebar
            Layout.fillHeight:     true
            Layout.preferredWidth: 220

            onImportRequested:   importSheet.open()
            onSettingsRequested: settingsPopup.open()
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
                        text:           root.importsPage ? qsTr("Imports") : library.title
                        color:          palette.windowText
                        font.pixelSize: 20
                        font.bold:      true
                    }

                    Text {
                        text: {
                            if (root.importsPage)
                                return qsTr("When photos came in, and from which device")

                            let line = library.loading && (library.count === 0)
                                       ? qsTr("Loading…")
                                       : ((library.count === 1) ? qsTr("1 item") : qsTr("%L1 items").arg(library.count))

                            if (library.filter === 13)
                                line += " \u00B7 " + library.folderPath
                            else if (library.filter === 12)
                                line += qsTr(" \u00B7 restore them, or delete them permanently")
                            else if (library.filter === 11)
                                line += qsTr(" \u00B7 not shown anywhere else")

                            if (preparingShown.value)
                                line += qsTr(" \u00B7 preparing thumbnails %1%").arg(photosApp.preparingPercent)

                            if (photosApp.sidecarSyncPercent >= 0)
                                line += qsTr(" \u00B7 saving to sidecar files %1%").arg(photosApp.sidecarSyncPercent)

                            return line
                        }
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
                        visible:   root.selecting && root.trashView
                        text:      qsTr("Restore")
                        pixelSize: 13
                        onClicked: library.restoreSelection()
                    }

                    IconButton {
                        visible:   root.selecting && root.trashView
                        text:      qsTr("Delete permanently")
                        pixelSize: 13
                        onClicked: root.requestDeleteForever(-1)
                    }

                    IconButton {
                        visible:   root.selecting && !root.trashView
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
                        visible:   root.selecting && !root.trashView
                        text:      qsTr("Add to album")
                        pixelSize: 13
                        onClicked: root.requestAddToAlbum(-1)
                    }

                    IconButton {
                        visible:   root.selecting && !root.trashView
                        text:      root.hiddenView ? qsTr("Unhide") : qsTr("Hide")
                        pixelSize: 13
                        onClicked: library.setHiddenForSelection(!root.hiddenView)
                    }

                    IconButton {
                        visible:   root.selecting && (library.filter === 2)
                        text:      qsTr("Remove from album")
                        pixelSize: 13
                        onClicked: library.removeSelectionFromCurrentAlbum()
                    }

                    IconButton {
                        visible:   root.selecting && !root.trashView
                        text:      qsTr("Move to trash")
                        pixelSize: 13
                        onClicked: root.requestTrash(-1)
                    }

                    // View actions

                    IconButton {
                        visible:   !root.selecting && !root.importsPage && (library.filter === 4) &&
                                   (library.filesKey.length > 0)
                        text:      qsTr("Undo import\u2026")
                        pixelSize: 13
                        onClicked: root.requestUndoImport(library.filesKey)
                    }

                    IconButton {
                        visible:   !root.selecting && root.trashView && (library.count > 0)
                        text:      qsTr("Empty\u2026")
                        pixelSize: 13
                        onClicked: root.confirm(qsTr("Delete all %L1 photos permanently?").arg(library.count),
                                                qsTr("This cannot be undone."),
                                                qsTr("Empty Recently Deleted"),
                                                () => library.emptyTrash())
                    }

                    // Zoom

                    IconButton {
                        visible:   !root.selecting && !root.importsPage
                        text:      "−"
                        tooltip:   qsTr("Smaller tiles")
                        onClicked: photoGrid.zoomStep(+1, photoGrid.height / 2)
                    }

                    IconButton {
                        visible:   !root.selecting && !root.importsPage
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
                visible:           !root.importsPage
                focus:             !viewer.visible

                onOpenPhoto:            (index) => viewer.open(index)
                onRequestTrash:         (row)   => root.requestTrash(row)
                onRequestAddToAlbum:    (row)   => root.requestAddToAlbum(row)
                onRequestDeleteForever: (row)   => root.requestDeleteForever(row)
            }

            ImportsPage {
                Layout.fillWidth:  true
                Layout.fillHeight: true
                visible:           root.importsPage

                onShowImport:  (importId) => root.showImport(importId)
                onRequestUndo: (importId) => root.requestUndoImport(importId)
            }
        }
    }

    // Touch screen pinch (recognized in C++) goes to whichever view is shown.
    Connections {
        target: photosApp

        function onTouchPinchStarted(x, y) {
            (viewer.visible ? viewer : photoGrid).touchPinch(0, 1.0, x, y)
        }

        function onTouchPinchUpdated(scale, x, y) {
            (viewer.visible ? viewer : photoGrid).touchPinch(1, scale, x, y)
        }

        function onTouchPinchFinished() {
            (viewer.visible ? viewer : photoGrid).touchPinch(2, 1.0, 0, 0)
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

        onRequestTrash:         (row) => root.requestTrash(row)
        onRequestAddToAlbum:    (row) => root.requestAddToAlbum(row)
        onRequestDeleteForever: (row) => root.requestDeleteForever(row)
    }

    // --- Shared dialogs -------------------------------------------------------------

    AlbumPopup {
        id: albumPopup
        onClosed: (viewer.visible ? viewer : photoGrid).forceActiveFocus()
    }

    ImportSheet {
        id: importSheet
        onShowImport: (importId) => root.showImport(importId)
        onClosed:     photoGrid.forceActiveFocus()
    }

    SettingsPopup {
        id: settingsPopup
        onClosed: photoGrid.forceActiveFocus()

        onRequestAddFolder: {
            settingsPopup.close()
            root.requestAddFolder()
        }

        onRequestRemoveFolder: (folderId, name) => {
            settingsPopup.close()
            root.requestRemoveFolder(folderId, name)
        }
    }

    Popup {
        id: confirmDialog

        property string title:      ""
        property string text:       ""
        property string actionText: ""
        property var    action:     null

        anchors.centerIn: parent
        width:            380
        modal:            true
        focus:            true
        padding:          20

        background: Rectangle {
            color:  palette.window
            radius: 12
        }

        onClosed: (viewer.visible ? viewer : photoGrid).forceActiveFocus()

        function accept() {
            const action = confirmDialog.action
            close()

            if (action)
                action()
        }

        Column {
            width:   parent.width
            spacing: 12

            Text {
                width:          parent.width
                wrapMode:       Text.WordWrap
                text:           confirmDialog.title
                color:          palette.windowText
                font.pixelSize: 16
                font.bold:      true
            }

            Text {
                width:    parent.width
                wrapMode: Text.WordWrap
                text:     confirmDialog.text
                color:    palette.windowText
                opacity:  0.7
            }

            Row {
                anchors.right: parent.right
                spacing:       8

                Button {
                    text:      qsTr("Cancel")
                    onClicked: confirmDialog.close()
                }

                Button {
                    id: confirmButton
                    text:      confirmDialog.actionText
                    onClicked: confirmDialog.accept()
                    Keys.onReturnPressed: confirmDialog.accept()
                    Keys.onEnterPressed:  confirmDialog.accept()
                }
            }
        }

        onOpened: confirmButton.forceActiveFocus()
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
