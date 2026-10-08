// SPDX-License-Identifier: GPL-2.0-or-later
// Photos mode - info panel of the viewer: caption, date, camera, location,
// albums and file details of the photo shown.

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle {
    id: panel

    property int row: -1

    // Refreshed when the photo or its information changes.
    readonly property var info: (library.revision >= 0) && (row >= 0) ? library.infoAt(row) : ({})

    color: Qt.rgba(0.11, 0.11, 0.12, 0.97)

    function commitCaption() {
        if ((row >= 0) && (caption.text !== (info.caption || "")))
            library.setCaptionAt(row, caption.text)
    }

    onRowChanged: caption.text = info.caption || ""
    onInfoChanged: if (!caption.activeFocus) caption.text = info.caption || ""

    component Label: Text {
        Layout.fillWidth: true
        color:            "white"
        opacity:          0.55
        font.pixelSize:   11
        font.bold:        true
        font.capitalization: Font.AllUppercase
        Layout.topMargin: 14
    }

    component Value: Text {
        Layout.fillWidth: true
        visible:          text.length > 0
        color:            "white"
        font.pixelSize:   13
        wrapMode:         Text.Wrap
    }

    component Chip: Rectangle {
        property alias text: chipText.text
        implicitWidth:  chipText.implicitWidth + 16
        implicitHeight: 24
        radius:         12
        color:          Qt.rgba(1, 1, 1, 0.12)

        Text {
            id: chipText
            anchors.centerIn: parent
            color:            "white"
            font.pixelSize:   12
        }
    }

    Flickable {
        id: flick
        anchors.fill:    parent
        anchors.margins: 16
        contentHeight:   column.implicitHeight
        clip:            true
        boundsBehavior:  Flickable.StopAtBounds

        ColumnLayout {
            id: column
            width:   flick.width
            spacing: 3

            // --- Caption --------------------------------------------------------

            TextArea {
                id: caption
                Layout.fillWidth: true
                placeholderText:  qsTr("Add a caption")
                placeholderTextColor: Qt.rgba(1, 1, 1, 0.4)
                color:            "white"
                wrapMode:         TextEdit.Wrap
                font.pixelSize:   14
                readOnly:         library.filter === 12

                background: Rectangle {
                    color:  caption.activeFocus ? Qt.rgba(1, 1, 1, 0.10) : Qt.rgba(1, 1, 1, 0.05)
                    radius: 6
                }

                onActiveFocusChanged: if (!activeFocus) panel.commitCaption()

                Keys.onEscapePressed: (event) => {
                    text = panel.info.caption || ""
                    focus = false
                    event.accepted = true
                }

                Keys.onReturnPressed: (event) => {
                    if (event.modifiers & Qt.ShiftModifier) {
                        event.accepted = false
                        return
                    }

                    panel.commitCaption()
                    focus = false
                    event.accepted = true
                }
            }

            // --- When -------------------------------------------------------------

            Label {
                text: qsTr("Date")
            }

            Value {
                text: panel.info.dateText || ""
            }

            // --- Camera ---------------------------------------------------------------

            Label {
                visible: (panel.info.camera || "").length > 0
                text:    qsTr("Camera")
            }

            Value {
                text:      panel.info.camera || ""
                font.bold: true
            }

            Value {
                text:    panel.info.lens || ""
                opacity: 0.8
            }

            Value {
                text: [panel.info.aperture, panel.info.exposure, panel.info.focal,
                       panel.info.iso ? "ISO " + panel.info.iso : ""].filter((v) => !!v).join("   ")
                opacity: 0.8
            }

            // --- Video ------------------------------------------------------------------

            Label {
                visible: videoDetails.text.length > 0
                text:    qsTr("Video")
            }

            Value {
                id: videoDetails
                text: (panel.info.isVideo === true) ? [panel.info.duration, panel.info.videoCodec, panel.info.frameRate]
                                                      .filter((v) => !!v).join("   ")
                                                    : ""
            }

            // --- Location -----------------------------------------------------------------

            Label {
                visible: panel.info.location !== undefined
                text:    qsTr("Location")
            }

            Value {
                text: (panel.info.places || []).join("\n")
            }

            Value {
                text:    panel.info.location || ""
                opacity: 0.8
            }

            Button {
                visible: panel.info.location !== undefined
                text:    qsTr("Show on map")
                onClicked: Qt.openUrlExternally("https://www.openstreetmap.org/?mlat=%1&mlon=%2#map=15/%1/%2"
                                                .arg(panel.info.latitude).arg(panel.info.longitude))
            }

            // --- Albums and device ---------------------------------------------------------

            Label {
                visible: (panel.info.albums || []).length > 0
                text:    qsTr("Albums")
            }

            Flow {
                Layout.fillWidth: true
                spacing:          6
                visible:          (panel.info.albums || []).length > 0

                Repeater {
                    model: panel.info.albums || []

                    delegate: Chip {
                        required property var modelData
                        text: modelData
                    }
                }
            }

            Label {
                visible: (panel.info.devices || []).length > 0
                text:    qsTr("Imported from")
            }

            Value {
                text: (panel.info.devices || []).join(", ")
            }

            // --- File -------------------------------------------------------------------------

            Label {
                text: qsTr("File")
            }

            Value {
                text:            panel.info.fileName || ""
                font.bold:       true
                wrapMode:        Text.WrapAnywhere
            }

            Value {
                text: [panel.info.dimensions, panel.info.megapixels, panel.info.size, panel.info.format]
                      .filter((v) => !!v).join("   ")
                opacity: 0.8
            }

            Text {
                Layout.fillWidth: true
                visible:          (panel.info.folder || "").length > 0
                text:             panel.info.folder || ""
                color:            "#8ab4ff"
                font.pixelSize:   12
                font.underline:   folderMouse.containsMouse
                wrapMode:         Text.WrapAnywhere

                MouseArea {
                    id: folderMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape:  Qt.PointingHandCursor
                    onClicked:    photosApp.openContainingFolder(library.filePathAt(panel.row))
                }
            }
        }
    }
}
