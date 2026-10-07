// SPDX-License-Identifier: GPL-2.0-or-later
// Photos mode - root item.
//
// Context properties provided by C++:
//   library   : PhotosLibraryModel (flat, date sorted photos of the current view)
//   grid      : PhotosGridModel    (sections and rows for the grid)
//   photosApp : PhotosContainer    (switch interface, open folder...)

import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root

    color:  palette.window
    focus:  true

    readonly property color separatorColor: Qt.rgba(palette.windowText.r,
                                                    palette.windowText.g,
                                                    palette.windowText.b, 0.12)

    RowLayout {
        anchors.fill: parent
        spacing:      0

        Sidebar {
            id: sidebar
            Layout.fillHeight:     true
            Layout.preferredWidth: 210
        }

        Rectangle {
            Layout.fillHeight:     true
            Layout.preferredWidth: 1
            color:                 root.separatorColor
        }

        ColumnLayout {
            Layout.fillWidth:  true
            Layout.fillHeight: true
            spacing:           0

            // --- Title bar --------------------------------------------------

            Item {
                Layout.fillWidth:       true
                Layout.preferredHeight: 56

                Column {
                    anchors.left:           parent.left
                    anchors.leftMargin:     16
                    anchors.verticalCenter: parent.verticalCenter
                    spacing:                1

                    Text {
                        text:           library.title
                        color:          palette.windowText
                        font.pixelSize: 20
                        font.bold:      true
                    }

                    Text {
                        text:           library.loading && (library.count === 0)
                                        ? qsTr("Loading…")
                                        : qsTr("%L1 items").arg(library.count)
                        color:          palette.windowText
                        opacity:        0.6
                        font.pixelSize: 12
                    }
                }

                Row {
                    anchors.right:          parent.right
                    anchors.rightMargin:    12
                    anchors.verticalCenter: parent.verticalCenter
                    spacing:                4

                    IconButton {
                        text:      "−"
                        tooltip:   qsTr("Smaller tiles")
                        onClicked: photoGrid.zoomStep(+1, photoGrid.height / 2)
                    }

                    IconButton {
                        text:      "+"
                        tooltip:   qsTr("Larger tiles")
                        onClicked: photoGrid.zoomStep(-1, photoGrid.height / 2)
                    }
                }
            }

            Rectangle {
                Layout.fillWidth:       true
                Layout.preferredHeight: 1
                color:                  root.separatorColor
            }

            PhotoGrid {
                id: photoGrid
                Layout.fillWidth:  true
                Layout.fillHeight: true
                focus:             !viewer.visible

                onOpenPhoto: (index) => viewer.open(index)
            }
        }
    }

    Viewer {
        id: viewer
        anchors.fill: parent
        visible:      false

        onClosed: (index) => {
            photoGrid.ensureVisible(index)
            photoGrid.forceActiveFocus()
        }
    }
}
