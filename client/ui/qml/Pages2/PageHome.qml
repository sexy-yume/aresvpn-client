// AresVPN Client - the connect surface, REBUILT (AresProject ROADMAP 18-3h, #D178).
// Copyright (c) 2026 AresVPN. Licensed under the GNU General Public License v3.0 (see LICENSE).
//
// This replaces upstream's home screen rather than decorating it. Amnezia's was built around a
// SERVER you installed and administer - protocol pickers, container drawers, a share sheet, an
// API subscription panel. Ours is built around a RENT: one public address, one credential, an
// expiry. The address is what the customer bought, so the address is what the screen leads with.
//
// The boundary is #D178: everything here is client/ui/. Nothing under client/core, client/daemon,
// client/platforms or service/ is reshaped for it.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import PageEnum 1.0
import Style 1.0

import "./"
import "../Controls2"
import "../Controls2/TextTypes"

PageType {
    id: root
    objectName: "page:PageHome"

    readonly property bool connected: ConnectionController.isConnected
    readonly property bool busy: ConnectionController.isConnectionInProgress
    readonly property bool hasRent: ServersUiController.defaultServerId !== ""
    // Plan 28: below the console's min_version this build refuses to connect, and says why.
    readonly property bool updateRequired: AresProfileController.updateRequired

    Rectangle {
        anchors.fill: parent
        color: AresStyle.color.bg
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: PageController.safeAreaTopMargin
        spacing: 0

        // ------------------------------------------------------------------ top bar
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: AresStyle.space.lg
            Layout.rightMargin: AresStyle.space.lg
            Layout.topMargin: AresStyle.space.lg
            spacing: AresStyle.space.sm

            Image {
                source: "qrc:/images/controls/ares.svg"
                sourceSize: Qt.size(22, 22)
                Layout.preferredWidth: 22
                Layout.preferredHeight: 22
                fillMode: Image.PreserveAspectFit
            }

            Text {
                text: "AresVPN"
                color: AresStyle.color.text
                font.family: AresStyle.font.family
                font.pixelSize: AresStyle.size.heading
                font.weight: Font.DemiBold
            }

            Item { Layout.fillWidth: true }

            ImageButtonType {
                objectName: "home.settings"
                image: "qrc:/images/controls/settings.svg"
                imageColor: AresStyle.color.textDim
                implicitWidth: 34
                implicitHeight: 34
                onClicked: PageController.goToPage(PageEnum.PageSettings)
            }
        }

        Item { Layout.fillHeight: true }

        // ------------------------------------------------------------------ the dial
        ColumnLayout {
            Layout.alignment: Qt.AlignHCenter
            Layout.fillWidth: true
            Layout.leftMargin: AresStyle.space.xl
            Layout.rightMargin: AresStyle.space.xl
            spacing: AresStyle.space.md

            Image {
                Layout.alignment: Qt.AlignHCenter
                source: "qrc:/images/controls/ares.svg"
                sourceSize: Qt.size(96, 96)
                Layout.preferredWidth: 96
                Layout.preferredHeight: 96
                fillMode: Image.PreserveAspectFit
                opacity: root.connected ? 1.0 : 0.32
                Behavior on opacity { NumberAnimation { duration: 220 } }
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: AresStyle.space.sm
                text: ConnectionController.connectionStateText.toUpperCase()
                color: root.connected ? AresStyle.color.accent
                                      : (root.busy ? AresStyle.color.warn : AresStyle.color.textMute)
                font.family: AresStyle.font.mono
                font.pixelSize: AresStyle.size.label
                font.letterSpacing: 1.6
            }

            // the rented public address - the thing the customer actually bought
            Text {
                Layout.alignment: Qt.AlignHCenter
                Layout.fillWidth: true
                // the ADDRESS alone. Upstream's collapsed description is "<protocol> | <address>",
                // and at display size that elided the address - the one thing the customer bought -
                // behind a protocol name the line below already carries. Seen by rendering it.
                text: root.hasRent
                    ? AresProfileController.addressOnly(ServersUiController.defaultServerDescriptionCollapsed)
                    : qsTr("Not signed in")
                color: AresStyle.color.text
                font.family: AresStyle.font.mono
                font.pixelSize: AresStyle.size.display
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                Layout.fillWidth: true
                text: {
                    if (!root.hasRent) return qsTr("Sign in with your account id, password and rent idx")
                    var idx = ServersUiController.defaultServerName
                    var proto = ServersUiController.defaultServerDefaultContainerName
                    return proto === "" ? idx : idx + "  ·  " + proto
                }
                color: AresStyle.color.textDim
                font.family: AresStyle.font.family
                font.pixelSize: AresStyle.size.small
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            // A rent expires. Nothing upstream has a place for this; for us it is the difference
            // between a working credential and a dead one, so it is on the first screen.
            Text {
                id: expiryLine
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: AresStyle.space.xs

                // expiryRevision is read so the line re-evaluates when a fetch stores an expiry and
                // once an hour; reading only the server id left it frozen on a renewal or past the
                // date itself while the app stayed open (plan 28).
                property int daysLeft: {
                    AresProfileController.expiryRevision
                    return root.hasRent
                        ? AresProfileController.daysLeftForServer(ServersUiController.defaultServerId)
                        : -1
                }

                visible: daysLeft >= 0
                // the sentence comes from the controller so "1 day left" is not "1 days left"
                // in any of the ten languages the .ts files carry
                text: {
                    AresProfileController.expiryRevision
                    return root.hasRent
                        ? AresProfileController.expiryTextForServer(ServersUiController.defaultServerId)
                        : ""
                }
                color: daysLeft === 0 ? AresStyle.color.bad
                                      : (daysLeft <= 7 ? AresStyle.color.warn : AresStyle.color.textMute)
                font.family: AresStyle.font.mono
                font.pixelSize: AresStyle.size.label
                font.letterSpacing: 0.8
            }

            // What the background refresh could not do (plan 28): "sign in again" after the console
            // refused the stored sign-in - tapping it opens the sign-in - or "no rent on this idx".
            Text {
                objectName: "home.sessionProblem"
                Layout.alignment: Qt.AlignHCenter
                Layout.fillWidth: true
                Layout.topMargin: AresStyle.space.sm
                visible: text !== ""
                text: AresProfileController.sessionProblem
                color: AresStyle.color.warn
                font.family: AresStyle.font.family
                font.pixelSize: AresStyle.size.small
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap

                MouseArea {
                    anchors.fill: parent
                    enabled: AresProfileController.signInNeeded
                    cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                    onClicked: PageController.goToPage(PageEnum.PageSetupWizardAresLogin)
                }
            }
        }

        Item { Layout.fillHeight: true }

        // ------------------------------------------------------------------ actions
        ColumnLayout {
            Layout.fillWidth: true
            Layout.leftMargin: AresStyle.space.lg
            Layout.rightMargin: AresStyle.space.lg
            Layout.bottomMargin: AresStyle.space.xl + PageController.safeAreaBottomMargin
            spacing: AresStyle.space.md

            // The console's update manifest (plan 28). Hidden while it serves none.
            Rectangle {
                objectName: "home.update"
                visible: AresProfileController.updateAvailable || root.updateRequired
                Layout.fillWidth: true
                Layout.preferredHeight: updateText.implicitHeight + AresStyle.space.md * 2
                radius: AresStyle.radius.sm
                color: updateArea.containsMouse ? AresStyle.color.surfaceHi : AresStyle.color.surface
                border.width: 1
                border.color: root.updateRequired ? AresStyle.color.bad : AresStyle.color.accent

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: AresStyle.space.lg
                    anchors.rightMargin: AresStyle.space.lg
                    spacing: AresStyle.space.md

                    Text {
                        id: updateText
                        Layout.fillWidth: true
                        text: root.updateRequired
                              ? qsTr("This version is no longer supported. Install version %1 to connect again.")
                                    .arg(AresProfileController.updateVersion)
                              : qsTr("Version %1 is available.").arg(AresProfileController.updateVersion)
                        color: AresStyle.color.text
                        font.family: AresStyle.font.family
                        font.pixelSize: AresStyle.size.small
                        wrapMode: Text.WordWrap
                    }

                    Text {
                        text: qsTr("Download")
                        color: root.updateRequired ? AresStyle.color.bad : AresStyle.color.accent
                        font.family: AresStyle.font.family
                        font.pixelSize: AresStyle.size.small
                        font.weight: Font.DemiBold
                    }
                }

                MouseArea {
                    id: updateArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: AresProfileController.openUpdatePage()
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 56
                radius: AresStyle.radius.sm
                color: rentsArea.containsMouse ? AresStyle.color.surfaceHi : AresStyle.color.surface
                border.width: 1
                border.color: AresStyle.color.line

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: AresStyle.space.lg
                    anchors.rightMargin: AresStyle.space.lg
                    spacing: AresStyle.space.md

                    ColumnLayout {
                        spacing: 1
                        Text {
                            text: qsTr("Account")
                            color: AresStyle.color.text
                            font.family: AresStyle.font.family
                            font.pixelSize: AresStyle.size.body
                        }
                        Text {
                            text: AresProfileController.sessionIdx !== ""
                                  ? qsTr("idx %1 - change it, or sign out").arg(AresProfileController.sessionIdx)
                                  : qsTr("Sign in to use a rent")
                            color: AresStyle.color.textMute
                            font.family: AresStyle.font.family
                            font.pixelSize: AresStyle.size.label
                        }
                    }

                    Item { Layout.fillWidth: true }

                    Text {
                        text: "›"
                        color: AresStyle.color.textDim
                        font.pixelSize: AresStyle.size.title
                    }
                }

                MouseArea {
                    id: rentsArea
                    objectName: "home.rents"
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: PageController.goToPage(PageEnum.PageAresSession)
                }
            }

            BasicButtonType {
                objectName: "home.connect"
                Layout.fillWidth: true
                Layout.preferredHeight: 52

                // A live or starting tunnel can always be closed - a connection stuck in Connecting is
                // cancelled here, as upstream's ConnectButton allows (H-9); opening one needs a rent.
                // Below min_version the open itself is refused, with a message, in
                // ConnectionUiController::openConnection, which every path reaches (H2-5).
                enabled: root.busy || root.connected || root.hasRent
                text: root.busy ? qsTr("Cancel")
                                : (root.connected ? qsTr("Disconnect") : qsTr("Connect"))

                defaultColor: root.connected ? AresStyle.color.transparent : AresStyle.color.accent
                textColor: root.connected ? AresStyle.color.text : AresStyle.color.accentForeground
                borderWidth: root.connected ? 1 : 0
                borderColor: AresStyle.color.lineStrong

                clickedFunc: function() {
                    if (!root.hasRent && !root.busy && !root.connected) {
                        PageController.goToPage(PageEnum.PageSetupWizardAresLogin)
                        return
                    }
                    ConnectionController.toggleConnection()
                }
            }
        }
    }
}
