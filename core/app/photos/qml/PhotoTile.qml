// SPDX-License-Identifier: GPL-2.0-or-later
// Photos mode - one square tile of the grid.

import QtQuick

Item {
    id: tile

    property int photoIndex: -1

    signal activated(int photoIndex)

    // Re-evaluated when favorites change (library.revision is bumped).
    readonly property bool favorite: (library.revision >= 0) && library.isFavoriteAt(photoIndex)
    readonly property bool video:    (library.revision >= 0) && library.isVideoAt(photoIndex)

    Rectangle {
        anchors.fill: parent
        color:        Qt.rgba(palette.windowText.r, palette.windowText.g, palette.windowText.b, 0.08)
    }

    Image {
        id: image
        anchors.fill: parent

        // Thumbnails are requested at their stored size once; zooming the grid
        // only rescales the texture on the GPU, it never reloads.
        source:       (library.revision >= 0) ? library.thumbSourceAt(tile.photoIndex) : ""
        asynchronous: true
        cache:        true
        fillMode:     Image.PreserveAspectCrop
        smooth:       true
        mipmap:       true
        opacity:      (status === Image.Ready) ? 1.0 : 0.0

        Behavior on opacity {
            NumberAnimation { duration: 120 }
        }
    }

    Rectangle {
        anchors.fill: parent
        color:        "black"
        opacity:      mouse.containsMouse ? 0.12 : 0.0
    }

    // Video badge

    Rectangle {
        visible:              tile.video
        anchors.right:        parent.right
        anchors.bottom:       parent.bottom
        anchors.margins:      6
        width:                22
        height:               22
        radius:               11
        color:                Qt.rgba(0, 0, 0, 0.55)

        Text {
            anchors.centerIn:       parent
            anchors.horizontalCenterOffset: 1
            text:                   "▶"
            color:                  "white"
            font.pixelSize:         11
        }
    }

    // Favorite badge

    Text {
        visible:            tile.favorite && (tile.width >= 60)
        anchors.left:       parent.left
        anchors.bottom:     parent.bottom
        anchors.margins:    6
        text:               "♥"
        color:              "white"
        style:              Text.Raised
        styleColor:         Qt.rgba(0, 0, 0, 0.5)
        font.pixelSize:     16
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        onClicked:    tile.activated(tile.photoIndex)
    }
}
