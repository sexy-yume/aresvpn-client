// AresVPN Client - Settings, REBUILT (AresProject ROADMAP 18-3h, #D178, #D182).
// Copyright (c) 2026 AresVPN. Licensed under the GNU General Public License v3.0 (see LICENSE).
//
// Upstream's index had seven entries because upstream's customer administers a server they
// installed and may hold a Premium subscription. Ours holds RENTS. Three groups are left, and
// #D182 is the test each one had to pass: does it keep "press Connect and it works" true?
//
//   Rents       - the rents on this device. The same screen the home row opens, so there is one
//                 place a rent is added, switched or removed and not two.
//   Connection  - what the tunnel does to this machine: kill switch, split tunnelling, DNS.
//   Application - what the app does to itself: language, autostart, notifications, logging.
//
// REMOVED, each because it is dead for this product rather than because it is untidy:
//   Servers          -> upstream's server list is the SSH/container machinery of a self-hosted
//                       install. Replaced by Rents, which is the same idea for what we sell.
//   News & Notifications -> the Premium news feed. Its own `isVisible` is
//                       `ServersUiController.hasServersFromGatewayApi`, which is false for every
//                       rent this client can hold, so it was already invisible - it comes out of
//                       the list so nobody has to work that out again.
//   Backup           -> exports and re-imports every stored server. A rent is re-fetched with id +
//                       password + idx, which is simpler and cannot restore a config from a file
//                       nobody checked (the hazard #D180 rule 2 names for `vpn://` links).
//
// KEPT deliberately: About, which carries the Appropriate Legal Notice GPL-3 section 5 requires and
// 18-3d makes a release gate - it is not a group and it is not optional. Dev console stays behind
// SettingsController.isDevModeEnabled, exactly as upstream has it.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import PageEnum 1.0
import Style 1.0

import "./"
import "../Controls2"
import "../Controls2/TextTypes"
import "../Config"

PageType {
    id: root
    objectName: "page:PageSettings"

    Rectangle {
        anchors.fill: parent
        color: AresStyle.color.bg
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: PageController.safeAreaTopMargin
        spacing: 0

        BackButtonType {
            id: backButton
            Layout.fillWidth: true
            Layout.topMargin: AresStyle.space.lg
        }

        Text {
            Layout.fillWidth: true
            Layout.leftMargin: AresStyle.space.lg
            Layout.rightMargin: AresStyle.space.lg
            Layout.topMargin: AresStyle.space.sm
            Layout.bottomMargin: AresStyle.space.lg
            text: qsTr("Settings")
            color: AresStyle.color.text
            font.family: AresStyle.font.family
            font.pixelSize: AresStyle.size.title
            font.weight: Font.DemiBold
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.leftMargin: AresStyle.space.lg
            Layout.rightMargin: AresStyle.space.lg
            spacing: AresStyle.space.sm

            Repeater {
                model: root.groups

                delegate: Rectangle {
                    required property var modelData
                    // `required property var modelData` stops QML injecting `index`, so the
                    // objectName below silently became "settings.groupundefined" and nothing
                    // could find the row. Asking for it explicitly is the fix (found by driving
                    // the UI - AresProject 18-3h).
                    required property int index

                    Layout.fillWidth: true
                    Layout.preferredHeight: 60
                    radius: AresStyle.radius.sm
                    color: rowArea.containsMouse ? AresStyle.color.surfaceHi : AresStyle.color.surface
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
                                text: modelData.title
                                color: AresStyle.color.text
                                font.family: AresStyle.font.family
                                font.pixelSize: AresStyle.size.body
                                elide: Text.ElideRight
                            }

                            Text {
                                Layout.fillWidth: true
                                text: modelData.subtitle
                                color: AresStyle.color.textMute
                                font.family: AresStyle.font.family
                                font.pixelSize: AresStyle.size.label
                                elide: Text.ElideRight
                            }
                        }

                        Text {
                            text: "›"
                            color: AresStyle.color.textDim
                            font.pixelSize: AresStyle.size.title
                        }
                    }

                    MouseArea {
                        id: rowArea
                        objectName: "settings.group" + index
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: PageController.goToPage(modelData.page)
                    }
                }
            }
        }

        Item { Layout.fillHeight: true }

        // The version is here rather than buried in About, because it is the first thing an
        // operator asks a customer for and the last thing a customer wants to go looking for.
        Text {
            Layout.fillWidth: true
            Layout.leftMargin: AresStyle.space.lg
            Layout.rightMargin: AresStyle.space.lg
            text: SettingsController.getAppVersion()
            color: AresStyle.color.textMute
            font.family: AresStyle.font.mono
            font.pixelSize: AresStyle.size.label
            horizontalAlignment: Text.AlignHCenter
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: AresStyle.space.lg
            Layout.rightMargin: AresStyle.space.lg
            Layout.topMargin: AresStyle.space.sm
            Layout.bottomMargin: AresStyle.space.xl + PageController.safeAreaBottomMargin
            spacing: AresStyle.space.md

            // GPL-3 section 5's Appropriate Legal Notice lives behind this, and 18-3d makes it a
            // release gate. It is a link rather than a group on purpose.
            Text {
                text: qsTr("About")
                color: AresStyle.color.textDim
                font.family: AresStyle.font.family
                font.pixelSize: AresStyle.size.small
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: PageController.goToPage(PageEnum.PageSettingsAbout)
                }
            }

            Text {
                visible: SettingsController.isDevModeEnabled
                text: qsTr("Dev console")
                color: AresStyle.color.textDim
                font.family: AresStyle.font.family
                font.pixelSize: AresStyle.size.small
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: PageController.goToPage(PageEnum.PageDevMenu)
                }
            }

            Item { Layout.fillWidth: true }

            Text {
                visible: GC.isDesktop()
                text: qsTr("Close application")
                color: AresStyle.color.textMute
                font.family: AresStyle.font.family
                font.pixelSize: AresStyle.size.small
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: PageController.closeApplication()
                }
            }
        }
    }

    readonly property var groups: [
        {
            title: qsTr("Rents"),
            subtitle: qsTr("Add, switch or remove a rent"),
            page: PageEnum.PageAresRents
        },
        {
            title: qsTr("Connection"),
            subtitle: qsTr("Kill switch, split tunnelling and DNS"),
            page: PageEnum.PageSettingsConnection
        },
        {
            title: qsTr("Application"),
            subtitle: qsTr("Language, autostart, notifications and logging"),
            page: PageEnum.PageSettingsApplication
        }
    ]
}
