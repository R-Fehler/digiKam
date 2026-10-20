// SPDX-License-Identifier: GPL-2.0-or-later
// Photos mode - warns when the library database is in a shared or synced
// folder, and moves it to this computer at the next start.

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle {
    id: banner

    property var  check:     libraries.databaseCheck()
    property bool dismissed: false

    readonly property bool problem: (check.problem || "").length > 0
    readonly property bool pending: (check.pending || "").length > 0

    function refresh() {
        check = libraries.databaseCheck()
    }

    visible:          (problem || pending) && !dismissed
    implicitHeight:   visible ? row.implicitHeight + 20 : 0
    color:            Qt.rgba(0.95, 0.65, 0.15, 0.18)

    Connections {
        target: libraries

        function onFoldersChanged() {
            banner.refresh()
        }
    }

    RowLayout {
        id: row
        anchors.left:           parent.left
        anchors.right:          parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin:     16
        anchors.rightMargin:    12
        spacing:                12

        Text {
            Layout.fillWidth: true
            wrapMode:         Text.WordWrap
            color:            palette.windowText
            font.pixelSize:   13
            text: {
                if (banner.pending)
                    return qsTr("The library database moves to %1 when Photos starts next time.").arg(banner.check.pending)

                const where = (banner.check.problem === "library") ? qsTr("inside a library folder")
                            : (banner.check.problem === "network") ? qsTr("on a network share")
                            : qsTr("in a folder synced by %1").arg(banner.check.tool)

                return qsTr("The library database (%1) is %2. If another computer opens it through a share or a sync, "
                            + "it can be damaged: each computer should keep its own database, the photos and their "
                            + "sidecar files are what is shared.").arg(banner.check.path).arg(where)
            }
        }

        Button {
            visible:   !banner.pending
            text:      qsTr("Move it to this computer")
            onClicked: {
                libraries.requestDatabaseMove(banner.check.suggested)
                banner.refresh()
            }
        }

        Button {
            visible:   banner.pending
            text:      qsTr("Quit Photos now")
            onClicked: photosApp.quitApplication()
        }

        Button {
            visible:   banner.pending
            text:      qsTr("Cancel the move")
            onClicked: {
                libraries.requestDatabaseMove("")
                banner.refresh()
            }
        }

        IconButton {
            text:      "✕"
            tooltip:   qsTr("Not now")
            onClicked: banner.dismissed = true
        }
    }
}
