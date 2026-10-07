// SPDX-License-Identifier: GPL-2.0-or-later
// Photos mode - navigation sidebar.

import QtQuick
import QtQuick.Layouts

Rectangle {
    id: sidebar

    color: Qt.darker(palette.window, 1.04)

    component NavItem: Rectangle {
        id: item

        property string label:    ""
        property string glyph:    ""
        property bool   selected: false

        signal activated()

        width:  ListView.view ? ListView.view.width : parent.width
        height: 34
        radius: 6
        color:  selected ? Qt.rgba(palette.highlight.r, palette.highlight.g, palette.highlight.b, 0.22)
                         : (mouse.containsMouse ? Qt.rgba(palette.windowText.r, palette.windowText.g,
                                                          palette.windowText.b, 0.07)
                                                : "transparent")

        Row {
            anchors.left:           parent.left
            anchors.leftMargin:     10
            anchors.verticalCenter: parent.verticalCenter
            spacing:                10

            Text {
                width:          18
                text:           item.glyph
                color:          palette.windowText
                font.pixelSize: 15
                horizontalAlignment: Text.AlignHCenter
            }

            Text {
                text:           item.label
                color:          palette.windowText
                font.pixelSize: 14
                font.bold:      item.selected
                elide:          Text.ElideRight
                width:          sidebar.width - 70
            }
        }

        MouseArea {
            id: mouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape:  Qt.PointingHandCursor
            onClicked:    item.activated()
        }
    }

    ColumnLayout {
        anchors.fill:    parent
        anchors.margins: 10
        spacing:         2

        Text {
            Layout.bottomMargin: 10
            Layout.leftMargin:   6
            Layout.topMargin:    6
            text:                qsTr("Photos")
            color:               palette.windowText
            font.pixelSize:      18
            font.bold:           true
        }

        NavItem {
            Layout.fillWidth: true
            glyph:            "▦"
            label:            qsTr("Library")
            selected:         library.filter === 0
            onActivated:      library.filter = 0
        }

        NavItem {
            Layout.fillWidth: true
            glyph:            "♥"
            label:            qsTr("Favorites")
            selected:         library.filter === 1
            onActivated:      library.filter = 1
        }

        NavItem {
            Layout.fillWidth: true
            glyph:            "▶"
            label:            qsTr("Videos")
            selected:         library.filter === 3
            onActivated:      library.filter = 3
        }

        Text {
            Layout.topMargin:    18
            Layout.bottomMargin: 4
            Layout.leftMargin:   6
            text:                qsTr("Albums")
            color:               palette.windowText
            opacity:             0.6
            font.pixelSize:      12
            font.bold:           true
        }

        Item {
            Layout.fillWidth:  true
            Layout.fillHeight: true

            ListView {
                id: albumsList
                anchors.fill: parent
                clip:         true
                spacing:      2
                model:        library.albums

                delegate: NavItem {
                    required property var modelData
                    glyph:       "\u25A2"
                    label:       modelData.name
                    selected:    (library.filter === 2) && (library.albumTagId === modelData.tagId)
                    onActivated: library.showAlbum(modelData.tagId)
                }
            }

            Text {
                visible:         albumsList.count === 0
                width:           parent.width
                leftPadding:     6
                rightPadding:    6
                wrapMode:        Text.WordWrap
                text:            qsTr("Open a photo and use \u201CAdd to album\u201D to create one.")
                color:           palette.windowText
                opacity:         0.5
                font.pixelSize:  12
            }
        }

        NavItem {
            Layout.fillWidth: true
            glyph:            "⚙"
            label:            qsTr("Classic interface")
            onActivated:      photosApp.switchToClassic()
        }
    }
}
