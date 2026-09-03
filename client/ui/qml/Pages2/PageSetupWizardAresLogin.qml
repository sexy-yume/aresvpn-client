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

    BackButtonType {
        id: backButton

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

                Layout.fillWidth: true
                Layout.topMargin: 32
                Layout.leftMargin: 16
                Layout.rightMargin: 16

                text: qsTr("Add this rent")

                clickedFunc: function() { root.doLogin() }
            }

            LabelTextType {
                Layout.fillWidth: true
                Layout.topMargin: 24
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.bottomMargin: 16

                text: qsTr("Your password is sent once, over TLS, to %1 and is not stored on this device. The rent's configuration is.").arg(AresProfileController.endpoint)
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
        PageController.showNotificationMessage(qsTr("Rent %1 added").arg(idx))
        PageController.goToPageHome()
    }
}
