// SPDX-License-Identifier: GPL-2.0-or-later
// Photos mode - "Add to album" popup, for one photo or for the selection.

import QtQuick
import QtQuick.Controls.Basic

Popup {
    id: albumPopup

    /// row >= 0: that photo only; row == -1: the selection.
    property int targetRow: -1

    anchors.centerIn: parent
    width:            320
    modal:            true
    focus:            true
    padding:          16

    background: Rectangle {
        color:  palette.window
        radius: 10
    }

    onOpened: {
        albumName.text = ""
        albumName.forceActiveFocus()
    }

    function add(name) {
        const done = (targetRow >= 0) ? library.addToAlbum(targetRow, name)
                                      : library.addSelectionToAlbum(name)
        if (done)
            close()
    }

    Column {
        width:   parent.width
        spacing: 10

        Text {
            text:           (albumPopup.targetRow >= 0) || (library.selectionCount <= 1)
                            ? qsTr("Add to album")
                            : qsTr("Add %L1 photos to album").arg(library.selectionCount)
            color:          palette.windowText
            font.pixelSize: 16
            font.bold:      true
        }

        Repeater {
            model: library.albums

            delegate: Button {
                required property var modelData
                width:     parent.width
                text:      modelData.name
                onClicked: albumPopup.add(modelData.name)
            }
        }

        TextField {
            id: albumName
            width:            parent.width
            placeholderText:  qsTr("New album name")
            onAccepted:       albumPopup.add(text)
        }

        Button {
            width:     parent.width
            text:      qsTr("Create album")
            enabled:   albumName.text.trim().length > 0
            onClicked: albumPopup.add(albumName.text)
        }
    }
}
