// SPDX-License-Identifier: GPL-2.0-or-later
// Photos mode - adds an inbox folder (see PhotosInboxes).

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Popup {
    id: popup

    property string folder: ""
    property string error:  ""

    readonly property string libraryFolder: (folder.length > 0) ? inboxes.libraryFolderOf(folder) : ""

    anchors.centerIn: parent
    width:            480
    modal:            true
    focus:            true
    padding:          22

    background: Rectangle {
        color:  palette.window
        radius: 14
    }

    onOpened: {
        folder = ""
        error  = ""
        deviceField.text = ""
        moveButton.checked = true
    }

    component Note: Text {
        Layout.fillWidth: true
        wrapMode:         Text.WordWrap
        color:            palette.windowText
        opacity:          0.65
        font.pixelSize:   12
    }

    contentItem: ColumnLayout {
        spacing: 8

        Text {
            text:           qsTr("Add an inbox")
            color:          palette.windowText
            font.pixelSize: 18
            font.bold:      true
        }

        Note {
            text: qsTr("The folder a phone app uploads to, e.g. \u201CPhotoInbox/Anna-iPhone\u201D on the NAS or in "
                       + "the synced folder. Give each phone its own inbox.")
        }

        RowLayout {
            Layout.fillWidth: true

            Text {
                Layout.fillWidth: true
                text:             (popup.folder.length > 0) ? popup.folder : qsTr("No folder chosen")
                elide:            Text.ElideMiddle
                color:            palette.windowText
                opacity:          (popup.folder.length > 0) ? 1.0 : 0.5
            }

            Button {
                text:      qsTr("Choose\u2026")
                onClicked: {
                    const chosen = inboxes.chooseFolder()

                    if (chosen.length > 0) {
                        popup.folder = chosen

                        if (deviceField.text.length === 0)
                            deviceField.text = inboxes.suggestedDevice(chosen)
                    }
                }
            }
        }

        Text {
            Layout.topMargin: 6
            text:             qsTr("Device")
            color:            palette.windowText
            font.bold:        true
        }

        TextField {
            id: deviceField
            Layout.fillWidth: true
            placeholderText:  qsTr("e.g. Anna\u2019s iPhone")
        }

        Text {
            Layout.topMargin: 6
            text:             qsTr("After import")
            color:            palette.windowText
            font.bold:        true
        }

        RadioButton {
            id: moveButton
            text:    qsTr("Move the photos into the library (the inbox empties itself)")
            checked: true
        }

        Note {
            Layout.leftMargin: 28
            text: qsTr("For upload apps which keep the photos on the phone (PhotoSync, Nextcloud, Synology Photos), "
                       + "or Syncthing set to \u201CSend Only\u201D on the phone and \u201CReceive Only\u201D here. "
                       + "A photo leaves the inbox only once its copy in the library is verified.")
        }

        RadioButton {
            id: leaveButton
            text: qsTr("Leave them in the inbox")
        }

        Note {
            Layout.leftMargin: 28
            text: qsTr("For folders which mirror the phone both ways: removing photos there would remove them "
                       + "on the phone too. They are imported once and skipped afterwards.")
        }

        Note {
            visible: (popup.folder.length > 0)
            text:    (popup.libraryFolder.length > 0)
                     ? qsTr("Inside the library folder %1: every computer syncing it knows this inbox, and the "
                            + "inbox is not shown as part of the library.").arg(popup.libraryFolder)
                     : qsTr("Photos go into %1. Other computers locate this folder once (Settings \u2192 Inboxes).")
                           .arg(importer.importFolder)
        }

        Text {
            Layout.fillWidth: true
            visible:          popup.error.length > 0
            text:             popup.error
            wrapMode:         Text.WordWrap
            color:            "#d04040"
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 8

            Item { Layout.fillWidth: true }

            Button {
                text:      qsTr("Cancel")
                onClicked: popup.close()
            }

            Button {
                text:        qsTr("Add inbox")
                highlighted: true
                enabled:     (popup.folder.length > 0) && (deviceField.text.trim().length > 0)
                onClicked: {
                    popup.error = inboxes.addInbox(popup.folder, deviceField.text, moveButton.checked, "")

                    if (popup.error.length === 0)
                        popup.close()
                }
            }
        }
    }
}
