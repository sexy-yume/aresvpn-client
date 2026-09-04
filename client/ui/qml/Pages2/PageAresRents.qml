// AresVPN Client - the rent list (AresProject ROADMAP 18-3h).
// Copyright (c) 2026 AresVPN. Licensed under the GNU General Public License v3.0 (see LICENSE).
//
// There is no Amnezia screen this replaces. Their model is "servers you installed"; ours is
// "rents you hold" - each one an idx, a protocol and a public address, and each one expiring.
// The live one is marked, tapping a row makes it default, and adding one goes to the login.
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
            text: qsTr("Rents")
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
            text: qsTr("Each rent is one public address on one node. Tap to make it the one you connect with.")
            color: AresStyle.color.textMute
            font.family: AresStyle.font.family
            font.pixelSize: AresStyle.size.small
            wrapMode: Text.WordWrap
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: AresStyle.space.lg
            Layout.rightMargin: AresStyle.space.lg
            clip: true
            spacing: AresStyle.space.sm
            model: ServersModel

            ScrollBar.vertical: ScrollBarType {}

            delegate: Rectangle {
                width: list.width
                height: 64
                radius: AresStyle.radius.sm
                color: rowArea.containsMouse ? AresStyle.color.surfaceHi : AresStyle.color.surface
                border.width: 1
                border.color: isDefault ? AresStyle.color.accent : AresStyle.color.line

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: AresStyle.space.md
                    anchors.rightMargin: AresStyle.space.md
                    spacing: AresStyle.space.md

                    // the live marker - the only chromatic thing in the row
                    Rectangle {
                        Layout.preferredWidth: 3
                        Layout.preferredHeight: 34
                        radius: 2
                        color: isDefault && ConnectionController.isConnected
                               ? AresStyle.color.accent : AresStyle.color.lineStrong
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Text {
                            Layout.fillWidth: true
                            text: name
                            color: AresStyle.color.text
                            font.family: AresStyle.font.mono
                            font.pixelSize: AresStyle.size.small
                            elide: Text.ElideRight
                        }

                        Text {
                            Layout.fillWidth: true
                            text: {
                                var a = hostName === undefined ? "" : hostName
                                var p = defaultContainer === undefined ? "" : defaultContainer
                                return (a !== "" && p !== "") ? a + "  ·  " + p : (a !== "" ? a : p)
                            }
                            color: AresStyle.color.textMute
                            font.family: AresStyle.font.mono
                            font.pixelSize: AresStyle.size.label
                            elide: Text.ElideRight
                        }
                    }

                    // days left - the rent's own clock, ahead of the selection state, because an
                    // expired rent is the one thing a customer needs to see without tapping
                    Text {
                        property int daysLeft: AresProfileController.daysLeftForServer(serverId)
                        visible: daysLeft >= 0
                        text: daysLeft === 0 ? qsTr("EXPIRED") : qsTr("%1 d").arg(daysLeft)
                        color: daysLeft === 0 ? AresStyle.color.bad
                                              : (daysLeft <= 7 ? AresStyle.color.warn : AresStyle.color.textMute)
                        font.family: AresStyle.font.mono
                        font.pixelSize: AresStyle.size.label
                    }

                    // state pill
                    Rectangle {
                        visible: isDefault
                        Layout.preferredHeight: 20
                        Layout.preferredWidth: pillText.implicitWidth + 16
                        radius: AresStyle.radius.pill
                        color: ConnectionController.isConnected ? AresStyle.color.accentSoft
                                                                : AresStyle.color.transparent
                        border.width: ConnectionController.isConnected ? 0 : 1
                        border.color: AresStyle.color.lineStrong

                        Text {
                            id: pillText
                            anchors.centerIn: parent
                            text: ConnectionController.isConnected ? qsTr("LIVE") : qsTr("SELECTED")
                            color: ConnectionController.isConnected ? AresStyle.color.accent
                                                                    : AresStyle.color.textDim
                            font.family: AresStyle.font.mono
                            font.pixelSize: 9
                            font.letterSpacing: 0.8
                        }
                    }

                    // remove
                    ImageButtonType {
                        image: "qrc:/images/controls/trash.svg"
                        imageColor: AresStyle.color.textMute
                        implicitWidth: 28
                        implicitHeight: 28
                        onClicked: {
                            if (ConnectionController.isConnected && isDefault) {
                                PageController.showNotificationMessage(
                                    qsTr("Disconnect before removing the rent you are using."))
                                return
                            }
                            removeDialog.pendingIndex = index
                            removeDialog.pendingName = name
                            removeDialog.pendingServerId = serverId
                            removeDialog.open()
                        }
                    }
                }

                MouseArea {
                    id: rowArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (ConnectionController.isConnected) {
                            PageController.showNotificationMessage(
                                qsTr("Disconnect before switching to another rent."))
                            return
                        }
                        // the UI controller persists the choice; ServersModel's own
                        // setDefaultServerId is only its internal sync
                        ServersUiController.setDefaultServerAtIndex(index)
                    }
                }
            }
        }

        // HTTP rents never appear in this list; say so once rather than leave a silent gap (#D180)
        Text {
            Layout.fillWidth: true
            Layout.leftMargin: AresStyle.space.lg
            Layout.rightMargin: AresStyle.space.lg
            Layout.topMargin: AresStyle.space.md
            text: qsTr("An HTTP proxy rent will not appear here - it is used from a browser or curl, "
                     + "with the address on its page in the AresVPN console.")
            color: AresStyle.color.textMute
            font.family: AresStyle.font.family
            font.pixelSize: AresStyle.size.label
            wrapMode: Text.WordWrap
        }

        BasicButtonType {
            Layout.fillWidth: true
            Layout.preferredHeight: 52
            Layout.leftMargin: AresStyle.space.lg
            Layout.rightMargin: AresStyle.space.lg
            Layout.topMargin: AresStyle.space.md
            Layout.bottomMargin: AresStyle.space.xl + PageController.safeAreaBottomMargin

            text: qsTr("Add a rent")
            defaultColor: AresStyle.color.accent
            textColor: AresStyle.color.accentForeground

            clickedFunc: function() { PageController.goToPage(PageEnum.PageSetupWizardAresLogin) }
        }
    }

    // Removing a rent is destructive and the credential is not recoverable from this app - it is
    // re-fetched by logging in again - so it is confirmed, and the confirmation names the rent.
    Dialog {
        id: removeDialog

        property int pendingIndex: -1
        property string pendingName: ""
        property string pendingServerId: ""

        anchors.centerIn: parent
        width: Math.min(parent.width - 2 * AresStyle.space.xl, 360)
        modal: true
        padding: AresStyle.space.lg

        background: Rectangle {
            color: AresStyle.color.surfaceHi
            radius: AresStyle.radius.md
            border.width: 1
            border.color: AresStyle.color.lineStrong
        }

        contentItem: ColumnLayout {
            spacing: AresStyle.space.md

            Text {
                Layout.fillWidth: true
                text: qsTr("Remove %1?").arg(removeDialog.pendingName)
                color: AresStyle.color.text
                font.family: AresStyle.font.family
                font.pixelSize: AresStyle.size.heading
                font.weight: Font.DemiBold
                wrapMode: Text.WordWrap
            }

            Text {
                Layout.fillWidth: true
                text: qsTr("The rent itself is not cancelled - only this device forgets it. "
                         + "Log in with the same idx to get it back.")
                color: AresStyle.color.textDim
                font.family: AresStyle.font.family
                font.pixelSize: AresStyle.size.small
                wrapMode: Text.WordWrap
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: AresStyle.space.sm
                spacing: AresStyle.space.sm

                BasicButtonType {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 44
                    text: qsTr("Keep")
                    defaultColor: AresStyle.color.transparent
                    textColor: AresStyle.color.text
                    borderWidth: 1
                    borderColor: AresStyle.color.lineStrong
                    clickedFunc: function() { removeDialog.close() }
                }

                BasicButtonType {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 44
                    text: qsTr("Remove")
                    defaultColor: AresStyle.color.bad
                    textColor: AresStyle.color.text
                    clickedFunc: function() {
                        if (removeDialog.pendingIndex >= 0) {
                            // the expiry goes with the rent - the id is unreachable once the
                            // server row is gone, so this has to happen first
                            if (removeDialog.pendingServerId !== "") {
                                AresProfileController.forgetRentExpiry(removeDialog.pendingServerId)
                            }
                            ServersUiController.removeServerAtIndex(removeDialog.pendingIndex)
                        }
                        removeDialog.close()
                    }
                }
            }
        }
    }
}
