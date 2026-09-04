// AresVPN Client - one settings row, in this product's language (AresProject ROADMAP 18-3h).
// Copyright (c) 2026 AresVPN. Licensed under the GNU General Public License v3.0 (see LICENSE).
//
// Upstream's LabelWithButtonType and SwitcherType are built on AmneziaStyle and on a ListView
// header/delegate/footer arrangement with hand-placed DividerTypes. Ours is a card: a title, a
// line of explanation, and either a chevron or a switch. One file, used by every settings screen,
// so a row looks the same everywhere and there is one place to change it.
//
// It carries BOTH shapes on purpose rather than two files. A settings row is either a door or a
// toggle, the difference is one property, and splitting it would mean two things to keep in step.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Style 1.0

Rectangle {
    id: root

    // What the row says
    property string title: ""
    property string subtitle: ""

    // A TOGGLE row when `toggleable` is true, a DOOR row otherwise.
    property bool toggleable: false
    property bool checked: false

    signal clicked()
    signal toggled(bool value)

    implicitHeight: subtitle === "" ? 52 : 62
    radius: AresStyle.radius.sm
    color: area.containsMouse ? AresStyle.color.surfaceHi : AresStyle.color.surface
    border.width: 1
    border.color: AresStyle.color.line

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: AresStyle.space.lg
        anchors.rightMargin: AresStyle.space.lg
        spacing: AresStyle.space.md

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 1

            Text {
                Layout.fillWidth: true
                text: root.title
                color: AresStyle.color.text
                font.family: AresStyle.font.family
                font.pixelSize: AresStyle.size.body
                elide: Text.ElideRight
            }

            Text {
                Layout.fillWidth: true
                visible: root.subtitle !== ""
                text: root.subtitle
                color: AresStyle.color.textMute
                font.family: AresStyle.font.family
                font.pixelSize: AresStyle.size.label
                elide: Text.ElideRight
            }
        }

        // the door
        Text {
            visible: !root.toggleable
            text: "›"
            color: AresStyle.color.textDim
            font.pixelSize: AresStyle.size.title
        }

        // the toggle - drawn here rather than borrowed, so the whole screen is one language
        Rectangle {
            id: track
            visible: root.toggleable
            Layout.preferredWidth: 40
            Layout.preferredHeight: 22
            radius: 11
            color: root.checked ? AresStyle.color.accent : AresStyle.color.lineStrong
            Behavior on color { ColorAnimation { duration: 140 } }

            Rectangle {
                width: 16
                height: 16
                radius: 8
                y: 3
                x: root.checked ? track.width - width - 3 : 3
                color: root.checked ? AresStyle.color.accentForeground : AresStyle.color.text
                Behavior on x { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
            }
        }
    }

    MouseArea {
        id: area
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            if (root.toggleable) {
                root.toggled(!root.checked)
            } else {
                root.clicked()
            }
        }
    }
}
