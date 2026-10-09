// SPDX-License-Identifier: GPL-2.0-or-later
// Photos mode - history of the imports, newest first: when, from which
// device, how many photos; show them or undo an import.

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Item {
    id: page

    signal showImport(string importId)
    signal requestUndo(string importId)

    ListView {
        id: list
        anchors.fill:    parent
        anchors.margins: 16
        spacing:         8
        clip:            true
        model:           importer.history

        // Inboxes first: what is waiting, what came in last.
        header: Column {
            width:   list.width
            spacing: 8

            Repeater {
                model: inboxes.inboxes

                delegate: Rectangle {
                    required property var modelData

                    width:  list.width
                    height: inboxRow.implicitHeight + 20
                    radius: 10
                    color:  Qt.rgba(palette.highlight.r, palette.highlight.g, palette.highlight.b, 0.10)

                    RowLayout {
                        id: inboxRow
                        anchors.left:           parent.left
                        anchors.right:          parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin:     16
                        anchors.rightMargin:    12
                        spacing:                12

                        Text {
                            text:           "\u2913"
                            color:          palette.windowText
                            font.pixelSize: 20
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing:          1

                            Text {
                                Layout.fillWidth: true
                                text:             qsTr("Inbox \u00B7 %1").arg(modelData.device)
                                elide:            Text.ElideRight
                                color:            palette.windowText
                                font.pixelSize:   14
                                font.bold:        true
                            }

                            Text {
                                Layout.fillWidth: true
                                text:             modelData.status
                                                  + (modelData.lastImport ? qsTr(" \u00B7 last import %1").arg(modelData.lastImport) : "")
                                elide:            Text.ElideRight
                                color:            palette.windowText
                                opacity:          0.75
                                font.pixelSize:   12
                            }
                        }

                        Button {
                            text:      qsTr("Check now")
                            onClicked: inboxes.checkNow()
                        }
                    }
                }
            }

            Text {
                width:               list.width
                height:              (list.count === 0) ? 80 : 4
                visible:             list.count === 0
                verticalAlignment:   Text.AlignVCenter
                horizontalAlignment: Text.AlignHCenter
                wrapMode:            Text.WordWrap
                text:                qsTr("No imports yet. Use \u201CImport\u201D at the top of the sidebar, or add an inbox in Settings.")
                color:               palette.windowText
                opacity:             0.6
            }
        }

        delegate: Rectangle {
            id: card

            required property var modelData

            width:  list.width
            height: row.implicitHeight + 24
            radius: 10
            color:  cardMouse.containsMouse ? Qt.rgba(palette.highlight.r, palette.highlight.g, palette.highlight.b, 0.10)
                                            : Qt.rgba(palette.windowText.r, palette.windowText.g, palette.windowText.b, 0.05)
            opacity: modelData.undone ? 0.6 : 1.0

            MouseArea {
                id: cardMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape:  Qt.PointingHandCursor
                onClicked:    page.showImport(card.modelData.id)
            }

            RowLayout {
                id: row
                anchors.left:           parent.left
                anchors.right:          parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin:     16
                anchors.rightMargin:    12
                spacing:                12

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing:          2

                    Text {
                        Layout.fillWidth: true
                        text:             card.modelData.dateText
                        elide:            Text.ElideRight
                        color:            palette.windowText
                        font.pixelSize:   15
                        font.bold:        true
                    }

                    Text {
                        Layout.fillWidth: true
                        elide:            Text.ElideRight
                        text: {
                            const count = (card.modelData.count === 1) ? qsTr("1 photo or video")
                                                                       : qsTr("%L1 photos and videos").arg(card.modelData.count)
                            const parts = [count]

                            if (card.modelData.device)
                                parts.push(qsTr("from %1").arg(card.modelData.device))

                            if (card.modelData.undone)
                                parts.push(qsTr("undone"))

                            return parts.join(" · ")
                        }
                        color:          palette.windowText
                        opacity:        0.75
                        font.pixelSize: 13
                    }

                    Text {
                        Layout.fillWidth: true
                        elide:            Text.ElideMiddle
                        text:             (card.modelData.computer ? card.modelData.computer + ": " : "") + card.modelData.source
                        color:            palette.windowText
                        opacity:          0.45
                        font.pixelSize:   11
                    }
                }

                Button {
                    text:      qsTr("Show")
                    onClicked: page.showImport(card.modelData.id)
                }

                Button {
                    visible:   !card.modelData.undone
                    text:      qsTr("Undo…")
                    onClicked: page.requestUndo(card.modelData.id)
                }
            }
        }
    }
}
