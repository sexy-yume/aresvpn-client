// AresVPN Client - THE SESSION (AresProject ROADMAP 18-3h, #D187).
// Copyright (c) 2026 AresVPN. Licensed under the GNU General Public License v3.0 (see LICENSE).
//
// THIS SCREEN REPLACES THE RENT LIST, and the difference is the whole product.
//
// What was here before was `PageAresRents`: a list you added rents to by logging in, with one of
// them selected. The operator saw it and said what it actually was:
//
//   저런걸 바란게 아님 ... 지금처럼 + 버튼 눌러서 id pw idx입력해서 설정만 가져와서 렌트목록에
//   추가하는게 아님. 그냥 로그인한 그 세팅이 계속 유지가 되는거고 idx를 바꾸던 id pw idx를 다 바꾸던
//   하는식으로 아예 세션을 바꾸는거임
//
// They are right: a list with a login on the front is Amnezia's model wearing our fields. This
// device holds ONE session - `id` + `pw` + `idx` - and is bound to the **idx**, not to a rent.
// Whatever rent carries that idx right now is the rent this device uses, which is why the two
// actions here are CHANGE THE IDX and SIGN IN AS SOMEBODY ELSE, and why neither of them is "add".
//
// The rent shown is therefore a FACT, not a choice: there is nothing to pick, and the only reason
// it is drawn at all is so a customer can read their address back to support.
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
    objectName: "page:PageAresSession"

    property string serverId: AresProfileController.sessionServerId
    property bool busy: false

    function refresh() {
        if (root.busy) {
            return
        }
        root.busy = true
        PageController.showBusyIndicator(true)
        var changed = AresProfileController.refreshNow()
        PageController.showBusyIndicator(false)
        root.busy = false
        if (changed) {
            PageController.showNotificationMessage(qsTr("Your rent changed - this device has the new one."))
        } else if (AresProfileController.lastError !== "") {
            PageController.showNotificationMessage(AresProfileController.lastError)
        } else {
            PageController.showNotificationMessage(qsTr("Already up to date."))
        }
    }

    Rectangle {
        anchors.fill: parent
        color: AresStyle.color.bg
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: PageController.safeAreaTopMargin
        spacing: 0

        BackButtonType {
            Layout.fillWidth: true
            Layout.topMargin: AresStyle.space.lg
        }

        Text {
            Layout.fillWidth: true
            Layout.leftMargin: AresStyle.space.lg
            Layout.rightMargin: AresStyle.space.lg
            Layout.topMargin: AresStyle.space.sm
            text: qsTr("Account")
            color: AresStyle.color.text
            font.family: AresStyle.font.family
            font.pixelSize: AresStyle.size.title
            font.weight: Font.DemiBold
        }

        Text {
            Layout.fillWidth: true
            Layout.leftMargin: AresStyle.space.lg
            Layout.rightMargin: AresStyle.space.lg
            Layout.topMargin: AresStyle.space.xs
            text: qsTr("This device is signed in to one account and bound to one idx. Whichever rent "
                       + "carries that idx is the rent it uses - if it changes, this app follows it.")
            color: AresStyle.color.textMute
            font.family: AresStyle.font.family
            font.pixelSize: AresStyle.size.small
            wrapMode: Text.Wrap
        }

        // ---- who, and which idx ---------------------------------------------------------------
        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: AresStyle.space.lg
            Layout.rightMargin: AresStyle.space.lg
            Layout.topMargin: AresStyle.space.lg
            Layout.preferredHeight: who.implicitHeight + AresStyle.space.lg * 2

            radius: AresStyle.radius.sm
            color: AresStyle.color.surface
            border.width: 1
            border.color: AresStyle.color.line

            ColumnLayout {
                id: who
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: AresStyle.space.lg
                anchors.rightMargin: AresStyle.space.lg
                spacing: AresStyle.space.md

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: qsTr("Account")
                        color: AresStyle.color.textMute
                        font.family: AresStyle.font.family
                        font.pixelSize: AresStyle.size.label
                    }
                    Item { Layout.fillWidth: true }
                    Text {
                        objectName: "session.account"
                        text: AresProfileController.sessionAccountId
                        color: AresStyle.color.text
                        font.family: AresStyle.font.mono
                        font.pixelSize: AresStyle.size.body
                        elide: Text.ElideRight
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: qsTr("Rent idx")
                        color: AresStyle.color.textMute
                        font.family: AresStyle.font.family
                        font.pixelSize: AresStyle.size.label
                    }
                    Item { Layout.fillWidth: true }
                    Text {
                        objectName: "session.idx"
                        text: AresProfileController.sessionIdx
                        color: AresStyle.color.accent
                        font.family: AresStyle.font.mono
                        font.pixelSize: AresStyle.size.body
                        elide: Text.ElideRight
                    }
                }
            }
        }

        // ---- what that idx currently points at ------------------------------------------------
        Text {
            Layout.fillWidth: true
            Layout.leftMargin: AresStyle.space.lg
            Layout.rightMargin: AresStyle.space.lg
            Layout.topMargin: AresStyle.space.lg
            text: qsTr("The rent behind it right now")
            color: AresStyle.color.textMute
            font.family: AresStyle.font.family
            font.pixelSize: AresStyle.size.label
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: AresStyle.space.lg
            Layout.rightMargin: AresStyle.space.lg
            Layout.topMargin: AresStyle.space.sm
            Layout.preferredHeight: rent.implicitHeight + AresStyle.space.lg * 2

            radius: AresStyle.radius.sm
            color: AresStyle.color.surface
            border.width: 1
            border.color: AresStyle.color.line

            ColumnLayout {
                id: rent
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: AresStyle.space.lg
                anchors.rightMargin: AresStyle.space.lg
                spacing: 2

                Text {
                    objectName: "session.address"
                    Layout.fillWidth: true
                    text: AresProfileController.addressOnly(ServersUiController.defaultServerDescriptionCollapsed)
                    color: AresStyle.color.text
                    font.family: AresStyle.font.mono
                    font.pixelSize: AresStyle.size.heading
                    elide: Text.ElideRight
                }

                Text {
                    Layout.fillWidth: true
                    text: {
                        var days = AresProfileController.expiryTextForServer(root.serverId)
                        return days === "" ? qsTr("no expiry recorded") : days
                    }
                    color: AresStyle.color.textDim
                    font.family: AresStyle.font.family
                    font.pixelSize: AresStyle.size.small
                    elide: Text.ElideRight
                }
            }
        }

        Item { Layout.fillHeight: true }

        // ---- the three things a customer can actually do ---------------------------------------
        AresRow {
            objectName: "session.refresh"
            Layout.fillWidth: true
            Layout.leftMargin: AresStyle.space.lg
            Layout.rightMargin: AresStyle.space.lg

            title: qsTr("Check now")
            subtitle: qsTr("The app checks on its own; this asks straight away")
            onClicked: root.refresh()
        }

        AresRow {
            objectName: "session.switch"
            Layout.fillWidth: true
            Layout.leftMargin: AresStyle.space.lg
            Layout.rightMargin: AresStyle.space.lg
            Layout.topMargin: AresStyle.space.sm

            // Changing the idx and signing in as somebody else are the SAME act - a new session
            // displaces the old one - so they are one row rather than two (#D187).
            title: qsTr("Use a different rent")
            subtitle: qsTr("Sign in with another idx, or another account")
            onClicked: PageController.goToPage(PageEnum.PageSetupWizardAresLogin)
        }

        AresRow {
            objectName: "session.signout"
            Layout.fillWidth: true
            Layout.leftMargin: AresStyle.space.lg
            Layout.rightMargin: AresStyle.space.lg
            Layout.topMargin: AresStyle.space.sm
            Layout.bottomMargin: AresStyle.space.xl + PageController.safeAreaBottomMargin

            title: qsTr("Sign out")
            subtitle: qsTr("This device forgets the account and the rent")
            onClicked: signOutDialog.open()
        }
    }

    // Signing out removes the rent from this device, so it is confirmed and the confirmation says
    // what does NOT happen - the rent itself is untouched and signing back in brings it straight
    // back. A destructive-sounding action with an unstated consequence is the one a customer will
    // not press when they should, and will press when they should not.
    //
    // Same shape as the removal dialog this screen replaces, deliberately: one confirmation
    // pattern in this product, not two.
    Dialog {
        id: signOutDialog

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
                text: qsTr("Sign out of %1?").arg(AresProfileController.sessionAccountId)
                color: AresStyle.color.text
                font.family: AresStyle.font.family
                font.pixelSize: AresStyle.size.heading
                font.weight: Font.DemiBold
                wrapMode: Text.WordWrap
            }

            Text {
                Layout.fillWidth: true
                text: qsTr("This device forgets your account and removes the rent it holds. The rent "
                         + "itself is not cancelled - sign in with the same idx to get it back.")
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
                    objectName: "session.signOutKeep"
                    text: qsTr("Stay signed in")
                    defaultColor: AresStyle.color.transparent
                    textColor: AresStyle.color.text
                    borderWidth: 1
                    borderColor: AresStyle.color.lineStrong
                    clickedFunc: function() { signOutDialog.close() }
                }

                BasicButtonType {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 44
                    objectName: "session.signOutConfirm"
                    text: qsTr("Sign out")
                    defaultColor: AresStyle.color.bad
                    textColor: AresStyle.color.text
                    clickedFunc: function() {
                        signOutDialog.close()
                        AresProfileController.logout()
                        PageController.goToPageHome()
                    }
                }
            }
        }
    }
}
