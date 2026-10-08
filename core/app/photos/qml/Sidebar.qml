// SPDX-License-Identifier: GPL-2.0-or-later
// Photos mode - navigation sidebar.

import QtQuick
import QtQuick.Layouts

Rectangle {
    id: sidebar

    color: Qt.darker(palette.window, 1.04)

    /// "grid" (the photos of library.filter) or "imports" (import history).
    property string page: "grid"

    signal importRequested()
    signal settingsRequested()

    function show(filter) {
        page           = "grid"
        library.filter = filter
    }

    component NavItem: Rectangle {
        id: item

        property string label:    ""
        property string glyph:    ""
        property string detail:   ""
        property bool   selected: false

        signal activated()

        Layout.fillWidth: true
        width:  ListView.view ? ListView.view.width : implicitWidth
        height: 32
        radius: 6
        color:  selected ? Qt.rgba(palette.highlight.r, palette.highlight.g, palette.highlight.b, 0.22)
                         : (mouse.containsMouse ? Qt.rgba(palette.windowText.r, palette.windowText.g,
                                                          palette.windowText.b, 0.07)
                                                : "transparent")

        Row {
            anchors.left:           parent.left
            anchors.leftMargin:     10
            anchors.right:          detailText.left
            anchors.rightMargin:    4
            anchors.verticalCenter: parent.verticalCenter
            spacing:                10

            Text {
                width:               18
                text:                item.glyph
                color:               palette.windowText
                font.pixelSize:      15
                horizontalAlignment: Text.AlignHCenter
            }

            Text {
                text:           item.label
                color:          palette.windowText
                font.pixelSize: 14
                font.bold:      item.selected
                elide:          Text.ElideRight
                width:          parent.width - 28
            }
        }

        Text {
            id: detailText
            anchors.right:          parent.right
            anchors.rightMargin:    10
            anchors.verticalCenter: parent.verticalCenter
            text:                   item.detail
            color:                  palette.windowText
            opacity:                0.45
            font.pixelSize:         11
        }

        MouseArea {
            id: mouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape:  Qt.PointingHandCursor
            onClicked:    item.activated()
        }
    }

    component SectionTitle: Text {
        Layout.topMargin:    16
        Layout.bottomMargin: 2
        Layout.leftMargin:   6
        color:               palette.windowText
        opacity:             0.6
        font.pixelSize:      12
        font.bold:           true
    }

    ColumnLayout {
        anchors.fill:    parent
        anchors.margins: 10
        spacing:         2

        RowLayout {
            Layout.fillWidth:    true
            Layout.bottomMargin: 8
            Layout.topMargin:    4

            Text {
                Layout.leftMargin: 6
                Layout.fillWidth:  true
                text:              qsTr("Photos")
                color:             palette.windowText
                font.pixelSize:    18
                font.bold:         true
            }

            // Import: the main way photos come in.
            Rectangle {
                implicitWidth:  importLabel.implicitWidth + 20
                implicitHeight: 28
                radius:         14
                color:          importMouse.containsMouse ? Qt.lighter(palette.highlight, 1.1) : palette.highlight

                Text {
                    id: importLabel
                    anchors.centerIn: parent
                    text:             qsTr("Import")
                    color:            palette.highlightedText
                    font.pixelSize:   13
                    font.bold:        true
                }

                MouseArea {
                    id: importMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape:  Qt.PointingHandCursor
                    onClicked:    sidebar.importRequested()
                }
            }
        }

        Flickable {
            id: flick
            Layout.fillWidth:  true
            Layout.fillHeight: true
            clip:              true
            contentHeight:     column.implicitHeight
            boundsBehavior:    Flickable.StopAtBounds

            ColumnLayout {
                id: column
                width:   flick.width
                spacing: 2

                NavItem {
                    glyph:       "▦"
                    label:       qsTr("Library")
                    selected:    (sidebar.page === "grid") && (library.filter === 0)
                    onActivated: sidebar.show(0)
                }

                NavItem {
                    glyph:       "♥"
                    label:       qsTr("Favorites")
                    selected:    (sidebar.page === "grid") && (library.filter === 1)
                    onActivated: sidebar.show(1)
                }

                NavItem {
                    glyph:       "▶"
                    label:       qsTr("Videos")
                    selected:    (sidebar.page === "grid") && (library.filter === 3)
                    onActivated: sidebar.show(3)
                }

                // --- Albums -----------------------------------------------------

                SectionTitle {
                    text: qsTr("Albums")
                }

                Repeater {
                    model: library.albums

                    delegate: NavItem {
                        required property var modelData
                        glyph:       "▢"
                        label:       modelData.name
                        selected:    (sidebar.page === "grid") && (library.filter === 2) &&
                                     (library.albumTagId === modelData.tagId)
                        onActivated: {
                            sidebar.page = "grid"
                            library.showAlbum(modelData.tagId)
                        }
                    }
                }

                Text {
                    visible:          library.albums.length === 0
                    Layout.fillWidth: true
                    leftPadding:      6
                    rightPadding:     6
                    wrapMode:         Text.WordWrap
                    text:             qsTr("Open a photo and use “Add to album” to create one.")
                    color:            palette.windowText
                    opacity:          0.5
                    font.pixelSize:   12
                }

                // --- Library folders (digiKam's collections) ---------------------

                SectionTitle {
                    text: qsTr("Folders")
                }

                Repeater {
                    model: libraries.folders

                    delegate: NavItem {
                        required property var modelData

                        glyph:       modelData.network ? "\u2601" : (modelData.removable ? "\u23CF" : "\u25A4")
                        label:       modelData.name
                        detail:      modelData.available ? "" : qsTr("not connected")
                        opacity:     modelData.available ? 1.0 : 0.5
                        selected:    (sidebar.page === "grid") && (library.filter === 13) &&
                                     (library.folderPath === modelData.path)
                        onActivated: {
                            if (!modelData.available)
                                return

                            sidebar.page = "grid"
                            library.showFolder(modelData.path)
                        }
                    }
                }

                // --- Devices: named at import, or the cameras of the photos -----

                SectionTitle {
                    visible: (importer.devices.length + library.devices.length) > 0
                    text:    qsTr("Devices")
                }

                // Named at import (from the import records).
                Repeater {
                    model: importer.devices

                    delegate: NavItem {
                        required property var modelData

                        glyph:       "\u260E"
                        label:       modelData.name
                        detail:      Number(modelData.count).toLocaleString(Qt.locale(), "f", 0)
                        selected:    (sidebar.page === "grid") && (library.filter === 4) &&
                                     (library.filesKey === "device:" + modelData.name)
                        onActivated: {
                            sidebar.page = "grid"
                            importer.showDevice(modelData.name)
                        }
                    }
                }

                // Device tags of earlier versions, and the cameras of the EXIF data.
                Repeater {
                    model: library.devices.filter((d) => (d.kind !== "tag") ||
                                                         !importer.devices.some((n) => n.name === d.name))

                    delegate: NavItem {
                        required property var modelData
                        readonly property bool named: modelData.kind === "tag"

                        glyph:       named ? "\u260E" : "\u25CE"
                        label:       modelData.name
                        detail:      Number(modelData.count).toLocaleString(Qt.locale(), "f", 0)
                        selected:    (sidebar.page === "grid") &&
                                     (named ? ((library.filter === 5) && (library.albumTagId === modelData.tagId))
                                            : ((library.filter === 6) && (library.title === modelData.name)))
                        onActivated: {
                            sidebar.page = "grid"

                            if (named)
                                library.showDeviceTag(modelData.tagId)
                            else
                                library.showCamera(modelData.make, modelData.model)
                        }
                    }
                }

                // --- Media types ------------------------------------------------

                SectionTitle {
                    text: qsTr("Media Types")
                }

                NavItem {
                    glyph:       "▭"
                    label:       qsTr("Screenshots")
                    selected:    (sidebar.page === "grid") && (library.filter === 7)
                    onActivated: sidebar.show(7)
                }

                NavItem {
                    glyph:       "◈"
                    label:       qsTr("Selfies")
                    selected:    (sidebar.page === "grid") && (library.filter === 10)
                    onActivated: sidebar.show(10)
                }

                NavItem {
                    glyph:       "↔"
                    label:       qsTr("Panoramas")
                    selected:    (sidebar.page === "grid") && (library.filter === 9)
                    onActivated: sidebar.show(9)
                }

                NavItem {
                    glyph:       "R"
                    label:       qsTr("RAW")
                    selected:    (sidebar.page === "grid") && (library.filter === 8)
                    onActivated: sidebar.show(8)
                }

                // --- Library management -----------------------------------------

                SectionTitle {
                    text: qsTr("Utilities")
                }

                NavItem {
                    glyph:       "⤓"
                    label:       qsTr("Imports")
                    detail:      importer.history.length > 0 ? String(importer.history.length) : ""
                    selected:    (sidebar.page === "imports") || ((sidebar.page === "grid") && (library.filter === 4))
                    onActivated: sidebar.page = "imports"
                }

                NavItem {
                    glyph:       "◌"
                    label:       qsTr("Hidden")
                    selected:    (sidebar.page === "grid") && (library.filter === 11)
                    onActivated: sidebar.show(11)
                }

                NavItem {
                    glyph:       "♲"
                    label:       qsTr("Recently Deleted")
                    selected:    (sidebar.page === "grid") && (library.filter === 12)
                    onActivated: sidebar.show(12)
                }
            }
        }

        NavItem {
            glyph:       "⚙"
            label:       qsTr("Settings")
            onActivated: sidebar.settingsRequested()
        }

        NavItem {
            glyph:       "⇄"
            label:       qsTr("Classic interface")
            onActivated: photosApp.switchToClassic()
        }
    }
}
