// AresVPN Client - About, REBUILT (AresProject ROADMAP 18-3h, and 18-3d makes it a release gate).
// Copyright (c) 2026 AresVPN. Licensed under the GNU General Public License v3.0 (see LICENSE).
//
// This was the last screen on the path a customer walks that was still Amnezia's controls in
// Amnezia's style - upstream's ListViewType with a header/delegate/footer arrangement over
// AmneziaStyle. It is rebuilt here for the same reason the rest of the UI was, and it carries one
// duty none of the other screens do.
//
// **GPL-3 SECTION 5's APPROPRIATE LEGAL NOTICE LIVES ON THIS SCREEN.** Section 0 defines what that
// notice must contain, and it is four things: an appropriate copyright notice; a statement that
// there is NO WARRANTY; a statement that the user may redistribute the work under this licence;
// and **how to view a copy of the licence**. The first three were half-present in one sentence and
// the fourth was absent entirely - the licence texts were installed beside the executable and
// nothing in the running program could open them. `PageSettingsLicenses` is the fourth, and the
// three sentences below are the first three, written out rather than implied.
//
// **AND THE "CHECK FOR UPDATES" BUTTON IS GONE, because it was not decoration - it fired.**
// 18-3e guarded the update check at `CoreController::checkForAppUpdates()`, which is the AUTOMATIC
// entry point, and recorded the reason: it is the one non-Premium `GatewayController` user, it
// phones `gw.amnezia.org`, and under the strict kill switch it opens a hole for that gateway's IP.
// This button called `UpdateController::checkForUpdates()` DIRECTLY, one layer below the guard, so
// a customer pressing it on our fork reached Amnezia's gateway with an envelope built against a
// key this fork does not carry - a request that can only fail, from a screen whose job is to state
// facts. Guarding one of two entry points is not a guard. PageSettings already hides the
// "Check for updates automatically" switch for exactly this reason (*a control that does nothing is
// worse than an absent one*); this is the same decision applied to the control that DID something.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import PageEnum 1.0
import Style 1.0

import "./"
import "../Controls2"
import "../Components"

PageType {
    id: root
    objectName: "page:PageSettingsAbout"

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

        Flickable {
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: width
            contentHeight: body.implicitHeight
            boundsBehavior: Flickable.StopAtBounds
            clip: true

            ScrollBar.vertical: ScrollBar {}

            ColumnLayout {
                id: body
                width: parent.width
                spacing: AresStyle.space.sm

                Image {
                    source: "qrc:/images/aresBigLogo.png"
                    fillMode: Image.PreserveAspectFit
                    Layout.alignment: Qt.AlignHCenter
                    Layout.topMargin: AresStyle.space.sm
                    Layout.preferredWidth: 168
                    Layout.preferredHeight: 129
                }

                Text {
                    Layout.fillWidth: true
                    Layout.leftMargin: AresStyle.space.lg
                    Layout.rightMargin: AresStyle.space.lg
                    text: qsTr("AresVPN Client")
                    color: AresStyle.color.text
                    font.family: AresStyle.font.family
                    font.pixelSize: AresStyle.size.title
                    font.weight: Font.DemiBold
                    horizontalAlignment: Text.AlignHCenter
                }

                Text {
                    Layout.fillWidth: true
                    Layout.leftMargin: AresStyle.space.lg
                    Layout.rightMargin: AresStyle.space.lg
                    text: SettingsController.getAppVersion()
                    color: AresStyle.color.textMute
                    font.family: AresStyle.font.mono
                    font.pixelSize: AresStyle.size.small
                    horizontalAlignment: Text.AlignHCenter

                    // Upstream's way into the dev console, kept exactly as it was: PageSettings
                    // shows the Dev console link on SettingsController.isDevModeEnabled and this
                    // is the only thing that sets it.
                    MouseArea {
                        property int clickCount: 0
                        anchors.fill: parent
                        onClicked: {
                            if (clickCount > 10) {
                                SettingsController.enableDevMode()
                            } else {
                                clickCount++
                            }
                        }
                    }
                }

                // ---- the Appropriate Legal Notice, GPL-3 sections 0 and 5 --------------------
                Rectangle {
                    Layout.fillWidth: true
                    Layout.leftMargin: AresStyle.space.lg
                    Layout.rightMargin: AresStyle.space.lg
                    Layout.topMargin: AresStyle.space.lg
                    Layout.preferredHeight: notice.implicitHeight + AresStyle.space.lg * 2

                    radius: AresStyle.radius.sm
                    color: AresStyle.color.surface
                    border.width: 1
                    border.color: AresStyle.color.line

                    Text {
                        id: notice
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: AresStyle.space.lg
                        anchors.rightMargin: AresStyle.space.lg

                        // Three sentences, and each one is a clause of the notice rather than
                        // prose: the copyright, the absence of warranty, and the right to
                        // redistribute. The fourth clause - how to view a copy of the licence -
                        // is the row underneath.
                        text: qsTr("Copyright (c) 2026 AresVPN. A fork of Amnezia VPN, copyright (c) "
                                   + "the AmneziaVPN authors. Amnezia and AmneziaVPN are their marks "
                                   + "and this product is not affiliated with them.\n\n"
                                   + "This program comes with ABSOLUTELY NO WARRANTY.\n\n"
                                   + "This is free software, and you are welcome to redistribute it "
                                   + "and to change it under the terms of the GNU General Public "
                                   + "License, version 3.")
                        color: AresStyle.color.textDim
                        font.family: AresStyle.font.family
                        font.pixelSize: AresStyle.size.small
                        wrapMode: Text.Wrap
                    }
                }

                AresRow {
                    objectName: "about.licenses"
                    Layout.fillWidth: true
                    Layout.leftMargin: AresStyle.space.lg
                    Layout.rightMargin: AresStyle.space.lg
                    Layout.topMargin: AresStyle.space.sm

                    title: qsTr("Licences")
                    subtitle: qsTr("The GPL-3, and the terms of everything this program includes")
                    onClicked: PageController.goToPage(PageEnum.PageSettingsLicenses)
                }

                AresRow {
                    objectName: "about.source"
                    Layout.fillWidth: true
                    Layout.leftMargin: AresStyle.space.lg
                    Layout.rightMargin: AresStyle.space.lg

                    // GPL-3 section 6's Corresponding Source. Naming the repository here is the
                    // written offer, and #D178 makes it public no later than the first binary that
                    // leaves our hands.
                    title: qsTr("Source code")
                    subtitle: qsTr("github.com/sexy-yume/aresvpn-client")
                    onClicked: Qt.openUrlExternally("https://github.com/sexy-yume/aresvpn-client")
                }

                Text {
                    Layout.fillWidth: true
                    Layout.leftMargin: AresStyle.space.lg
                    Layout.rightMargin: AresStyle.space.lg
                    Layout.topMargin: AresStyle.space.lg
                    text: qsTr("Contact")
                    color: AresStyle.color.textMute
                    font.family: AresStyle.font.family
                    font.pixelSize: AresStyle.size.label
                }

                Repeater {
                    model: root.contacts

                    delegate: AresRow {
                        required property var modelData
                        required property int index

                        objectName: "about.contact" + index
                        Layout.fillWidth: true
                        Layout.leftMargin: AresStyle.space.lg
                        Layout.rightMargin: AresStyle.space.lg

                        title: modelData.title
                        subtitle: modelData.subtitle
                        onClicked: Qt.openUrlExternally(modelData.url)
                    }
                }

                Text {
                    Layout.fillWidth: true
                    Layout.leftMargin: AresStyle.space.lg
                    Layout.rightMargin: AresStyle.space.lg
                    Layout.topMargin: AresStyle.space.lg
                    Layout.bottomMargin: AresStyle.space.xl + PageController.safeAreaBottomMargin
                    text: qsTr("Privacy Policy")
                    color: AresStyle.color.textDim
                    font.family: AresStyle.font.family
                    font.pixelSize: AresStyle.size.small
                    horizontalAlignment: Text.AlignHCenter

                    MouseArea {
                        objectName: "about.privacy"
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: Qt.openUrlExternally(LanguageUiController.getCurrentSiteUrl("policy"))
                    }
                }
            }
        }
    }

    // Upstream's first row said "Telegram group" and opened a WEB page; the row now says what the
    // link actually is. A contact row that names the wrong medium is the kind of thing a customer
    // reports as broken.
    readonly property var contacts: [
        {
            title: qsTr("Support"),
            subtitle: qsTr("ares-vpn.org/support"),
            url: "https://ares-vpn.org/support"
        },
        {
            title: qsTr("support@ares-vpn.org"),
            subtitle: qsTr("For reviews and bug reports"),
            url: "mailto:support@ares-vpn.org"
        },
        {
            title: qsTr("Website"),
            subtitle: qsTr("ares-vpn.org"),
            url: "https://ares-vpn.org"
        }
    ]
}
