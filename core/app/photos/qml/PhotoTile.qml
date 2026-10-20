// SPDX-License-Identifier: GPL-2.0-or-later
// Photos mode - one square tile of the grid. Pure display: all mouse
// interaction is handled by the grid (selection, drag-select, open).

import QtQuick
import QtQuick.Window

Item {
    id: tile

    property int  photoIndex:    -1
    property bool hovered:       false
    property bool selectionMode: false

    // Set by the grid: the row is on screen or within half a screen of it.
    // Tiles further away (pre-instantiated rows) wait, so that what is on
    // screen loads first; once requested, a thumbnail stays requested.
    property bool nearView:      true
    property bool armed:         nearView

    onNearViewChanged:   if (nearView) armed = true
    onPhotoIndexChanged: {
        armed       = nearView
        shownSource = ""        // never show the previous photo of a recycled tile
    }

    // Smallest served thumbnail size whose short side still covers the tile
    // after the square crop (4:3 photos: short side = 0.75 x long side).
    readonly property real physicalSize: Math.max(width, height) * Screen.devicePixelRatio
    readonly property int  thumbSize: {
        const sizes = photosApp.thumbnailSizes
        for (let i = 0 ; i < sizes.length ; ++i) {
            if (sizes[i] * 0.75 >= physicalSize)
                return sizes[i]
        }
        return sizes[sizes.length - 1]
    }

    readonly property string wantedSource: (armed && (library.revision >= 0))
                                           ? library.thumbSourceAt(photoIndex, thumbSize) : ""

    // The source on screen only switches once the wanted one is loaded: when
    // zooming across a size boundary, the old texture stays until the new one is ready.
    property string shownSource: ""

    // Re-evaluated when favorites / selection change (revisions are bumped).
    readonly property bool favorite: (library.revision >= 0) && library.isFavoriteAt(photoIndex)
    readonly property bool video:    (library.revision >= 0) && library.isVideoAt(photoIndex)
    readonly property bool selected: (library.selectionRevision >= 0) && library.isSelectedAt(photoIndex)

    clip: true

    Rectangle {
        anchors.fill: parent
        color:        tile.selected ? Qt.rgba(palette.highlight.r, palette.highlight.g, palette.highlight.b, 0.25)
                                    : Qt.rgba(palette.windowText.r, palette.windowText.g, palette.windowText.b, 0.08)
    }

    Item {
        id: content

        // Selected tiles shrink a little, Photos style.
        property real inset: tile.selected ? Math.max(4, Math.round(tile.width * 0.07)) : 0

        x:      inset
        y:      inset
        width:  tile.width  - 2 * inset
        height: tile.height - 2 * inset

        Behavior on inset {
            NumberAnimation { duration: 90 }
        }

        Image {
            id: image
            anchors.fill: parent

            // Thumbnails are requested at their stored size once; zooming the grid
            // only rescales the texture on the GPU, it never reloads.
            source:       tile.shownSource
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

        // Loads the wanted size off screen, then makes it the shown one.
        Image {
            visible:      false
            asynchronous: true
            cache:        true
            source:       (tile.wantedSource !== tile.shownSource) ? tile.wantedSource : ""

            onStatusChanged: {
                if (status === Image.Ready)
                    tile.shownSource = source
            }
        }

        Rectangle {
            anchors.fill: parent
            color:        "black"
            opacity:      (tile.hovered && !tile.selected) ? 0.12 : 0.0
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
                anchors.centerIn:               parent
                anchors.horizontalCenterOffset: 1
                text:                           "▶"
                color:                          "white"
                font.pixelSize:                 11
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
    }

    // Check circle: shown on hover, in selection mode, and on selected tiles.
    // Clicking it (top-left corner) toggles the selection, see PhotoGrid.

    Rectangle {
        id: check

        readonly property int size: (tile.width >= 80) ? 22 : 16

        visible:        (tile.width >= 40) && (tile.selected || tile.hovered || tile.selectionMode)
        x:              6
        y:              6
        width:          size
        height:         size
        radius:         size / 2
        color:          tile.selected ? palette.highlight : Qt.rgba(0, 0, 0, 0.25)
        border.color:   "white"
        border.width:   tile.selected ? 0 : 2

        Text {
            anchors.centerIn: parent
            visible:          tile.selected
            text:             "✓"
            color:            palette.highlightedText
            font.pixelSize:   check.size * 0.65
            font.bold:        true
        }
    }
}
