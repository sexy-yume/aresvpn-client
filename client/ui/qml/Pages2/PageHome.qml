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
                    : qsTr("No rent yet")
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
                    if (!root.hasRent) return qsTr("Add one with your account id, password and its idx")
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

                property int daysLeft: root.hasRent
                    ? AresProfileController.daysLeftForServer(ServersUiController.defaultServerId)
                    : -1

                visible: daysLeft >= 0
                // the sentence comes from the controller so "1 day left" is not "1 days left"
                // in any of the ten languages the .ts files carry
                text: root.hasRent
                    ? AresProfileController.expiryTextForServer(ServersUiController.defaultServerId)
                    : ""
                color: daysLeft === 0 ? AresStyle.color.bad
                                      : (daysLeft <= 7 ? AresStyle.color.warn : AresStyle.color.textMute)
                font.family: AresStyle.font.mono
                font.pixelSize: AresStyle.size.label
                font.letterSpacing: 0.8

                Connections {
                    target: ServersUiController
                    function onDefaultServerIdChanged() {
                        expiryLine.daysLeft = root.hasRent
                            ? AresProfileController.daysLeftForServer(ServersUiController.defaultServerId)
                            : -1
                    }
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
                            text: qsTr("Rents")
                            color: AresStyle.color.text
                            font.family: AresStyle.font.family
                            font.pixelSize: AresStyle.size.body
                        }
                        Text {
                            text: qsTr("Switch, add or remove")
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
                    onClicked: PageController.goToPage(PageEnum.PageAresRents)
                }
            }

            BasicButtonType {
                objectName: "home.connect"
                Layout.fillWidth: true
                Layout.preferredHeight: 52

                enabled: root.hasRent && !root.busy
                text: root.busy ? ConnectionController.connectionStateText
                                : (root.connected ? qsTr("Disconnect") : qsTr("Connect"))

                defaultColor: root.connected ? AresStyle.color.transparent : AresStyle.color.accent
                textColor: root.connected ? AresStyle.color.text : AresStyle.color.accentForeground
                borderWidth: root.connected ? 1 : 0
                borderColor: AresStyle.color.lineStrong

                clickedFunc: function() {
                    if (!root.hasRent) {
                        PageController.goToPage(PageEnum.PageSetupWizardAresLogin)
                        return
                    }
                    ConnectionController.toggleConnection()
                }
            }
        }
    }
}
