// SPDX-License-Identifier: GPL-2.0-or-later
// Photos mode - import photos and videos from a phone, a memory card or a
// folder (see PhotosImporter).

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Popup {
    id: sheet

    signal showImport(string importId)

    anchors.centerIn: parent
    width:            460
    modal:            true
    focus:            true
    padding:          22
    closePolicy:      (importer.state === 1 || importer.state === 3) ? Popup.NoAutoClose
                                                                     : (Popup.CloseOnEscape | Popup.CloseOnPressOutside)

    background: Rectangle {
        color:  palette.window
        radius: 14
    }

    onOpened: {
        if (importer.state === 4)
            importer.reset()

        importer.refreshSources()
    }

    onClosed: importer.reset()

    // A phone plugged in while the sheet is open shows up.
    Timer {
        interval: 2500
        repeat:   true
        running:  sheet.visible && (importer.state === 0)
        onTriggered: importer.refreshSources()
    }

    Connections {
        target: importer

        function onStateChanged() {
            if (importer.state === 2)
                deviceField.text = importer.summary.device || ""
        }
    }

    component Title: Text {
        Layout.fillWidth: true
        wrapMode:         Text.WordWrap
        color:            palette.windowText
        font.pixelSize:   18
        font.bold:        true
    }

    component Note: Text {
        Layout.fillWidth: true
        wrapMode:         Text.WordWrap
        color:            palette.windowText
        opacity:          0.65
        font.pixelSize:   13
    }

    component SourceButton: Rectangle {
        id: sourceButton

        property string label: ""
        property string glyph: ""
        property string sub:   ""

        signal clicked()

        Layout.fillWidth: true
        implicitHeight:   52
        radius:           10
        color:            sourceMouse.containsMouse ? Qt.rgba(palette.highlight.r, palette.highlight.g, palette.highlight.b, 0.18)
                                                    : Qt.rgba(palette.windowText.r, palette.windowText.g, palette.windowText.b, 0.06)

        RowLayout {
            anchors.fill:        parent
            anchors.leftMargin:  14
            anchors.rightMargin: 14
            spacing:             12

            Text {
                text:           sourceButton.glyph
                color:          palette.windowText
                font.pixelSize: 20
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing:          0

                Text {
                    Layout.fillWidth: true
                    text:             sourceButton.label
                    elide:            Text.ElideRight
                    color:            palette.windowText
                    font.pixelSize:   14
                    font.bold:        true
                }

                Text {
                    Layout.fillWidth: true
                    visible:          text.length > 0
                    text:             sourceButton.sub
                    elide:            Text.ElideMiddle
                    color:            palette.windowText
                    opacity:          0.55
                    font.pixelSize:   11
                }
            }
        }

        MouseArea {
            id: sourceMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape:  Qt.PointingHandCursor
            onClicked:    sourceButton.clicked()
        }
    }

    // One section per importer state; hidden sections take no space.
    contentItem: ColumnLayout {
        spacing: 0

        // --- 0: choose where from ------------------------------------------------

        ColumnLayout {
            visible: importer.state === 0
            Layout.fillWidth: true
            spacing: 10

            Title {
                text: qsTr("Import photos and videos")
            }

            Note {
                text: importer.importFolder.length > 0
                      ? qsTr("New photos are copied into %1, in a folder per year and month. "
                             + "Photos already in your library are skipped.").arg(importer.importFolder)
                      : qsTr("Add a library folder first (classic interface, Settings → Collections).")
            }

            Repeater {
                model: importer.sources

                delegate: SourceButton {
                    required property var modelData
                    glyph:     "☎"
                    label:     modelData.name
                    sub:       modelData.path
                    onClicked: importer.scan(modelData.path)
                }
            }

            Note {
                visible: importer.sources.length === 0
                text:    qsTr("No phone or memory card found. Connect a phone with a cable "
                              + "(choose “File transfer” or “Allow access to photos” on it), "
                              + "or insert a memory card.")
            }

            SourceButton {
                glyph:     "▤"
                label:     qsTr("Choose a folder…")
                sub:       qsTr("A folder, a drive, or a phone shown in your file manager")
                onClicked: {
                    const folder = importer.chooseFolder()

                    if (folder.length > 0)
                        importer.scan(folder)
                }
            }

            Button {
                Layout.alignment: Qt.AlignRight
                text:             qsTr("Close")
                onClicked:        sheet.close()
            }
        }

        // --- 1: scanning -------------------------------------------------------------

        ColumnLayout {
            visible: importer.state === 1
            Layout.fillWidth: true
            spacing: 12

            Title {
                text: qsTr("Looking for photos…")
            }

            ProgressBar {
                Layout.fillWidth: true
                from:             0
                to:               Math.max(1, importer.total)
                value:            importer.done
                indeterminate:    importer.done === 0
            }

            Note {
                text: (importer.done === 0) ? qsTr("%L1 files found").arg(importer.total)
                                            : qsTr("Checked %L1 of %L2").arg(importer.done).arg(importer.total)
            }

            Button {
                Layout.alignment: Qt.AlignRight
                text:             qsTr("Cancel")
                onClicked:        importer.cancel()
            }
        }

        // --- 2: summary ----------------------------------------------------------------

        ColumnLayout {
            visible: importer.state === 2
            Layout.fillWidth: true
            spacing: 10

            readonly property int newCount:      importer.summary.newCount      || 0
            readonly property int existingCount: importer.summary.existingCount || 0

            Title {
                text: (parent.newCount === 0) ? qsTr("Nothing new to import")
                                              : (parent.newCount === 1) ? qsTr("1 new photo or video")
                                                                        : qsTr("%L1 new photos and videos").arg(parent.newCount)
            }

            Note {
                visible: (importer.summary.dateRange || "").length > 0
                text:    importer.summary.dateRange || ""
            }

            Note {
                visible: parent.existingCount > 0
                text:    (parent.existingCount === 1) ? qsTr("1 is already in your library and will be skipped.")
                                                      : qsTr("%L1 are already in your library and will be skipped.")
                                                            .arg(parent.existingCount)
            }

            Text {
                visible:          parent.newCount > 0
                Layout.topMargin: 6
                text:             qsTr("Device")
                color:            palette.windowText
                font.pixelSize:   13
                font.bold:        true
            }

            TextField {
                id: deviceField
                visible:          parent.newCount > 0
                Layout.fillWidth: true
                placeholderText:  qsTr("e.g. Anna’s phone")
            }

            Note {
                visible: parent.newCount > 0
                text:    qsTr("Give each phone its own name (“Anna’s phone”, “Ben’s phone”): "
                              + "the photos are listed under Devices, even for phones of the same model.")
                font.pixelSize: 11
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 6

                Button {
                    text:      qsTr("Back")
                    onClicked: importer.reset()
                }

                Item { Layout.fillWidth: true }

                Button {
                    visible:   parent.parent.newCount > 0
                    text:      qsTr("Import %L1").arg(parent.parent.newCount)
                    highlighted: true
                    onClicked: importer.start(deviceField.text)
                }
            }
        }

        // --- 3: importing --------------------------------------------------------------

        ColumnLayout {
            visible: importer.state === 3
            Layout.fillWidth: true
            spacing: 12

            Title {
                text: qsTr("Importing…")
            }

            ProgressBar {
                Layout.fillWidth: true
                from:             0
                to:               Math.max(1, importer.total)
                value:            importer.done
            }

            Note {
                text: qsTr("%L1 of %L2").arg(importer.done).arg(importer.total)
            }

            Button {
                Layout.alignment: Qt.AlignRight
                text:             qsTr("Stop")
                onClicked:        importer.cancel()
            }
        }

        // --- 4: done ---------------------------------------------------------------------

        ColumnLayout {
            visible: importer.state === 4
            Layout.fillWidth: true
            spacing: 10

            readonly property int imported: importer.summary.importedCount || 0
            readonly property int failed:   importer.summary.failedCount   || 0

            Title {
                text: (parent.imported === 1) ? qsTr("Imported 1 photo or video")
                                              : qsTr("Imported %L1 photos and videos").arg(parent.imported)
            }

            Note {
                visible: parent.failed > 0
                text:    qsTr("%L1 could not be copied (see the log).").arg(parent.failed)
                color:   "#d04040"
                opacity: 1
            }

            Note {
                text: qsTr("You can find this import, or undo it, under Imports.")
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 6

                Item { Layout.fillWidth: true }

                Button {
                    text:      qsTr("Done")
                    onClicked: sheet.close()
                }

                Button {
                    visible:     (importer.summary.importId || "").length > 0
                    text:        qsTr("Show")
                    highlighted: true
                    onClicked: {
                        const id = importer.summary.importId
                        sheet.close()
                        sheet.showImport(id)
                    }
                }
            }
        }
    }
}
