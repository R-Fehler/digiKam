// SPDX-License-Identifier: GPL-2.0-or-later
// Photos mode - small flat button showing a text glyph.

import QtQuick

Rectangle {
    id: button

    property alias  text:        label.text
    property string tooltip:     ""
    property color  textColor:   palette.windowText
    property color  hoverColor:  Qt.rgba(textColor.r, textColor.g, textColor.b, 0.12)
    property bool   checked:     false
    property int    pixelSize:   16

    signal clicked()

    implicitWidth:  Math.max(32, label.implicitWidth + 16)
    implicitHeight: 32
    radius:         6
    color:          mouse.containsMouse || checked ? hoverColor : "transparent"

    Text {
        id: label
        anchors.centerIn: parent
        color:            button.textColor
        font.pixelSize:   button.pixelSize
    }

    MouseArea {
        id: mouse
        anchors.fill:    parent
        hoverEnabled:    true
        cursorShape:     Qt.PointingHandCursor
        onClicked:       button.clicked()
    }
}
