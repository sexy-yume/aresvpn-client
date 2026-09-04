// AresVPN Client - Settings > Connection, REBUILT (AresProject ROADMAP 18-3h, #D182).
// Copyright (c) 2026 AresVPN. Licensed under the GNU General Public License v3.0 (see LICENSE).
//
// What the tunnel does to THIS machine: what it resolves with, what it carries, and what happens
// when it drops. Every row here is real for a rent.
//
// ONE ROW CAME OUT, and it was measured before it was removed rather than after. Upstream's first
// switch is "Use AresDNS - if AresDNS is installed on the server", and its value reaches exactly
// one decision: selfHostedAdminServerConfig.cpp's
//     d1 = (isAmneziaDnsEnabled && dnsOnServer) ? amneziaDnsIp : primaryDns
// `dnsOnServer` is true only for a self-hosted server carrying that container. A rent is an
// imported configuration and has none, so the switch could never change anything here - it was a
// control that did nothing, which is worse than an absent one (the same argument that hid the
// update toggle in Settings > Application). Its setting is untouched, so it returns the day a
// node offers DNS.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import PageEnum 1.0
import Style 1.0

import "./"
import "../Controls2"
import "../Components"
import "../Config"

PageType {
    id: root

    readonly property bool appSplitTunnelingAvailable: Qt.platform.os === "windows" || Qt.platform.os === "android"

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
            text: qsTr("Connection")
            color: AresStyle.color.text
            font.family: AresStyle.font.family
            font.pixelSize: AresStyle.size.title
            font.weight: Font.DemiBold
        }

        Text {
            Layout.fillWidth: true
            Layout.leftMargin: AresStyle.space.lg
            Layout.rightMargin: AresStyle.space.lg
            Layout.topMargin: 2
            Layout.bottomMargin: AresStyle.space.lg
            text: qsTr("What the tunnel does to this machine.")
            color: AresStyle.color.textMute
            font.family: AresStyle.font.family
            font.pixelSize: AresStyle.size.small
            wrapMode: Text.WordWrap
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.leftMargin: AresStyle.space.lg
            Layout.rightMargin: AresStyle.space.lg
            spacing: AresStyle.space.sm

            AresRow {
                Layout.fillWidth: true
                title: qsTr("DNS servers")
                subtitle: qsTr("Which resolver this device uses while connected")
                onClicked: PageController.goToPage(PageEnum.PageSettingsDns)
            }

            AresRow {
                Layout.fillWidth: true
                title: qsTr("Split tunnelling by site")
                subtitle: qsTr("Choose which sites go through the rent")
                onClicked: PageController.goToPage(PageEnum.PageSettingsSplitTunneling)
            }

            AresRow {
                Layout.fillWidth: true
                visible: root.appSplitTunnelingAvailable
                title: qsTr("Split tunnelling by app")
                subtitle: qsTr("Choose which applications go through the rent")
                onClicked: PageController.goToPage(PageEnum.PageSettingsAppSplitTunneling)
            }

            AresRow {
                Layout.fillWidth: true
                visible: GC.isDesktop()
                title: qsTr("Kill switch")
                subtitle: qsTr("Cut this device off the network if the tunnel drops")
                onClicked: PageController.goToPage(PageEnum.PageSettingsKillSwitch)
            }
        }

        Item { Layout.fillHeight: true }
    }
}
