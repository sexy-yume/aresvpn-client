// AresVPN Client - the login page: account id + password + rent idx -> POST /api/profile.
// Copyright (c) 2026 AresVPN. Licensed under the GNU General Public License v3.0 (see LICENSE).
// Modelled on PageSetupWizardCredentials.qml (the shape every synchronous, busy-indicator-wrapped
// slot in this tree uses); the work is AresProfileController.login().
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import PageEnum 1.0
import Style 1.0

import "./"
import "../Controls2"
import "../Config"
import "../Controls2/TextTypes"

PageType {
    id: root
    objectName: "page:PageSetupWizardAresLogin"

    // #D182: with no rent stored this page IS the first screen, so there is nowhere to go back
    // to and offering the control would be a dead end. Adding a rent from the list still pushes
    // this page, and there the back button is real.
    readonly property bool isFirstRun: PageController.isStartPageVisible()

    BackButtonType {
        id: backButton

        visible: !root.isFirstRun
        height: visible ? implicitHeight : 0

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.topMargin: 20 + PageController.safeAreaTopMargin
    }

    FlickableType {
        id: fl
        anchors.top: backButton.bottom
        anchors.bottom: parent.bottom
        contentHeight: content.height

        ColumnLayout {
            id: content

            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right

            // On a first run this page is the whole app, so it opens with the mark rather than
            // looking like step 2 of a wizard (#D182).
            Image {
                visible: root.isFirstRun
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 24
                Layout.bottomMargin: 8
                Layout.preferredWidth: 64
                Layout.preferredHeight: 64
                source: "qrc:/images/controls/ares.svg"
                sourceSize: Qt.size(64, 64)
                fillMode: Image.PreserveAspectFit
            }

            BaseHeaderType {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.bottomMargin: 16

                headerText: qsTr("AresVPN account")
                descriptionText: qsTr("Your account id and password, and the idx of the rent you want on this device. The idx is the alias you or your reseller gave the rent in the AresVPN console.")
            }

            TextFieldWithHeaderType {
                id: idField
                objectName: "login.id"

                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16

                headerText: qsTr("Account id")
                textField.placeholderText: qsTr("your login")
                textField.inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                checkEmptyText: true
            }

            TextFieldWithHeaderType {
                id: pwField
                objectName: "login.pw"

                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.topMargin: 16

                headerText: qsTr("Password")
                textField.echoMode: TextInput.Password
                checkEmptyText: true
            }

            TextFieldWithHeaderType {
                id: idxField
                objectName: "login.idx"

                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.topMargin: 16

                headerText: qsTr("Rent idx")
                textField.placeholderText: qsTr("for example odin_1")
                textField.inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                checkEmptyText: true
                rightButtonClickedOnEnter: true
                clickedFunc: function() { root.doLogin() }
            }

            WarningType {
                id: errorBox

                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.topMargin: 16

                visible: textString !== ""
                backGroundColor: AmneziaStyle.color.translucentWhite
                iconPath: "qrc:/images/controls/alert-circle.svg"
                textString: ""
            }

            BasicButtonType {
                id: loginButton
                objectName: "login.submit"

                Layout.fillWidth: true
                Layout.topMargin: 32
                Layout.leftMargin: 16
                Layout.rightMargin: 16

                text: AresProfileController.hasSession ? qsTr("Switch to this rent") : qsTr("Sign in")

                // The accent, like every other primary action in this product. Rendered on the
                // real platform and looked at (#L055), this was the ONE button that was not:
                // "Add a rent" on the rent list and "Connect" on the home screen are both accent,
                // and this one - the primary action of the FIRST screen a customer ever sees
                // (#D182 rule 1) - was upstream's white.
                defaultColor: AresStyle.color.accent
                hoveredColor: AresStyle.color.accentSoft
                pressedColor: AresStyle.color.accentSoft
                textColor: AresStyle.color.accentForeground

                clickedFunc: function() { root.doLogin() }
            }

            LabelTextType {
                Layout.fillWidth: true
                Layout.topMargin: 24
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.bottomMargin: 16

                // #D187: the password IS stored now, and saying otherwise would be a false statement to a
                // customer about what is on their own disk. It is the operator's decision
                // (*비밀번호를 기기에 저장한다*), and it is what lets this device follow its idx
                // when the console re-assigns the rent behind it - without which the server
                // side's automatic re-allocation stays half a feature.
                text: qsTr("Signing in stores your account and this rent on this device, encrypted, "
                           + "so the app can follow your idx if the rent behind it changes. "
                           + "Your password goes to %1 over TLS.").arg(AresProfileController.endpoint)
            }
        }
    }

    function doLogin() {
        errorBox.textString = ""
        var id = idField.textField.text.trim()
        var pw = pwField.textField.text
        var idx = idxField.textField.text.trim()
        if (id === "" || pw === "" || idx === "") {
            errorBox.textString = qsTr("All three fields are needed.")
            return
        }
        PageController.showBusyIndicator(true)
        var ok = AresProfileController.login(id, pw, idx)
        PageController.showBusyIndicator(false)
        if (!ok) {
            errorBox.textString = AresProfileController.lastError
            return
        }
        pwField.textField.text = ""
        PageController.showNotificationMessage(qsTr("Signed in - %1").arg(idx))
        PageController.goToPageHome()
    }
}
