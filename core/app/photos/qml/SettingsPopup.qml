// SPDX-License-Identifier: GPL-2.0-or-later
// Photos mode - the few settings of Photos mode. Everything else stays in
// the classic interface (Settings menu).

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Popup {
    id: settings

    anchors.centerIn: parent
    width:            500
    modal:            true
    focus:            true
    padding:          22

    signal requestAddFolder()
    signal requestRemoveFolder(int folderId, string name)

    background: Rectangle {
        color:  palette.window
        radius: 14
    }

    component Heading: Text {
        Layout.fillWidth: true
        Layout.topMargin: 10
        color:            palette.windowText
        font.pixelSize:   14
        font.bold:        true
    }

    component Note: Text {
        Layout.fillWidth: true
        wrapMode:         Text.WordWrap
        color:            palette.windowText
        opacity:          0.65
        font.pixelSize:   12
    }

    contentItem: ColumnLayout {
        spacing: 6

        Text {
            text:           qsTr("Settings")
            color:          palette.windowText
            font.pixelSize: 18
            font.bold:      true
        }

        // --- Where information is saved ---------------------------------------------

        Heading {
            text: qsTr("Favorites, albums, captions and people")
        }

        RadioButton {
            id: sidecarsOn
            Layout.fillWidth: true
            text:      qsTr("Save next to the photos, in sidecar files (recommended)")
            checked:   photosApp.sidecars
            onClicked: photosApp.sidecars = true
        }

        Note {
            Layout.leftMargin: 28
            text: qsTr("Each photo gets a small “.xmp” file beside it. The photos themselves are never changed. "
                       + "Other apps (Lightroom, darktable, digiKam) read these files, and they sync with your "
                       + "folders to your other computers.")
        }

        RadioButton {
            Layout.fillWidth: true
            text:      qsTr("Save in the library database only")
            checked:   !photosApp.sidecars
            onClicked: photosApp.sidecars = false
        }

        Note {
            Layout.leftMargin: 28
            text: qsTr("digiKam’s default. Nothing is written next to the photos, but other apps "
                       + "and other computers do not see this information.")
        }

        Note {
            visible: photosApp.sidecarSyncPercent >= 0
            text:    qsTr("Saving existing information to sidecar files… %1%").arg(photosApp.sidecarSyncPercent)
            opacity: 1
            color:   palette.highlight
        }

        // --- Library folders ---------------------------------------------------------

        Heading {
            text: qsTr("Library folders")
        }

        Repeater {
            model: libraries.folders

            delegate: RowLayout {
                required property var modelData

                Layout.fillWidth: true
                spacing:          8

                Text {
                    Layout.fillWidth: true
                    text:             modelData.available ? modelData.path
                                                          : qsTr("%1 (not connected)").arg(modelData.name)
                    elide:            Text.ElideMiddle
                    color:            palette.windowText
                    opacity:          modelData.available ? 1.0 : 0.5
                    font.pixelSize:   13
                }

                Button {
                    text:      qsTr("Remove\u2026")
                    onClicked: settings.requestRemoveFolder(modelData.id, modelData.name)
                }
            }
        }

        Button {
            text:      qsTr("Add a folder\u2026")
            onClicked: settings.requestAddFolder()
        }

        Note {
            text: qsTr("Photos in all these folders are in your library. Folders on external or "
                       + "network drives stay in it while the drive is not connected. "
                       + "Opening a folder (\u201Cdigikam --photos <folder>\u201D) adds it too.")
        }

        // --- Import ----------------------------------------------------------------------

        Heading {
            text: qsTr("Import into")
        }

        ComboBox {
            Layout.fillWidth: true
            model:            importer.libraryFolders
            currentIndex:     importer.libraryFolders.indexOf(importer.importFolder)
            onActivated:      (index) => importer.importFolder = importer.libraryFolders[index]
        }

        Note {
            text: qsTr("Imported photos go into a folder per year and month (2026/10) of this library folder.")
        }

        // --- Viewer -------------------------------------------------------------------------

        Heading {
            text: qsTr("Viewer")
        }

        Switch {
            text:      qsTr("Show the strip of thumbnails under the photo")
            checked:   photosApp.filmstrip
            onToggled: photosApp.filmstrip = checked
        }

        Button {
            Layout.alignment: Qt.AlignRight
            Layout.topMargin: 10
            text:             qsTr("Close")
            onClicked:        settings.close()
        }
    }
}
