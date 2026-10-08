// SPDX-License-Identifier: GPL-2.0-or-later
// Photos mode - scrollable strip of small thumbnails under the full screen
// photo (iOS / Samsung Gallery style). The current photo stays centered:
// scrolling the strip scrubs through the photos, a tap jumps to one.

import QtQuick

Item {
    id: strip

    /// Photo shown by the viewer (row in the library).
    property int  currentIndex: -1

    /// The user is scrolling the strip: the viewer shows thumbnails only meanwhile.
    readonly property bool scrubbing: list.userMoving
    readonly property bool hovered:   hover.hovered

    /// The user picked this photo (scrolled to it or tapped it).
    signal activated(int index)

    readonly property int cell:     44
    readonly property int pitch:    cell + 2

    height: cell + 28

    onCurrentIndexChanged: {
        if (!list.userMoving && (list.currentIndex !== currentIndex))
            list.currentIndex = currentIndex
    }

    // Opening the viewer: jump there, no animation.
    function center() {
        list.currentIndex = currentIndex
        list.positionViewAtIndex(currentIndex, ListView.Center)
    }

    Rectangle {
        anchors.fill: parent

        gradient: Gradient {
            GradientStop { position: 0.0; color: Qt.rgba(0, 0, 0, 0.0)  }
            GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, 0.6)  }
        }
    }

    HoverHandler {
        id: hover
    }

    ListView {
        id: list

        property bool userMoving: false

        anchors.left:           parent.left
        anchors.right:          parent.right
        anchors.verticalCenter: parent.verticalCenter
        height:                 strip.cell + 12

        orientation:            ListView.Horizontal
        model:                  library.count
        spacing:                strip.pitch - strip.cell
        reuseItems:             true
        cacheBuffer:            strip.pitch * 10
        boundsBehavior:         Flickable.StopAtBounds
        flickDeceleration:      3000

        // The current item stays in the middle; flicking ends on a photo.
        highlightRangeMode:      ListView.StrictlyEnforceRange
        preferredHighlightBegin: width / 2 - strip.cell / 2
        preferredHighlightEnd:   width / 2 + strip.cell / 2
        highlightMoveDuration:   160

        onMovementStarted: userMoving = true
        onMovementEnded:   userMoving = false

        onCurrentIndexChanged: {
            if (userMoving && (currentIndex !== strip.currentIndex))
                strip.activated(currentIndex)
        }

        delegate: Item {
            id: cellItem

            required property int index

            readonly property bool current: ListView.isCurrentItem

            width:  strip.cell
            height: strip.cell
            anchors.verticalCenter: parent ? parent.verticalCenter : undefined
            z:      current ? 1 : 0
            scale:  current ? 1.22 : 1.0

            Behavior on scale {
                NumberAnimation { duration: 120 }
            }

            Rectangle {
                anchors.fill: parent
                color:        Qt.rgba(1, 1, 1, 0.12)
                radius:       3
            }

            // Smallest served thumbnail size: already in memory most of the time.
            // Mipmapped like the grid tiles: they share the cached textures.
            Image {
                anchors.fill: parent
                source:       (library.revision >= 0) ? library.thumbSourceAt(cellItem.index, photosApp.thumbnailSizes[0]) : ""
                fillMode:     Image.PreserveAspectCrop
                asynchronous: true
                cache:        true
                smooth:       true
                mipmap:       true
                opacity:      cellItem.current ? 1.0 : 0.75
            }

            Text {
                visible:            (library.revision >= 0) && library.isVideoAt(cellItem.index)
                anchors.right:      parent.right
                anchors.bottom:     parent.bottom
                anchors.margins:    2
                text:               "▶"
                color:              "white"
                style:              Text.Raised
                styleColor:         Qt.rgba(0, 0, 0, 0.6)
                font.pixelSize:     10
            }

            Rectangle {
                anchors.fill: parent
                visible:      cellItem.current
                color:        "transparent"
                border.color: "white"
                border.width: 2
                radius:       3
            }

            TapHandler {
                onTapped: strip.activated(cellItem.index)
            }
        }
    }
}
