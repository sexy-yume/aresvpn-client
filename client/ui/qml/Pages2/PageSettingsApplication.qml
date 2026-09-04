// AresVPN Client - Settings > Application, REBUILT (AresProject ROADMAP 18-3h, #D182).
// Copyright (c) 2026 AresVPN. Licensed under the GNU General Public License v3.0 (see LICENSE).
//
// What the app does to ITSELF - language, startup, notifications, logging - as against Connection,
// which is what the tunnel does to the machine. Every switch here still calls exactly the
// SettingsController method upstream called; only the drawing is ours.
//
// TWO ROWS ARE GONE, each because it could not do anything for this product, and a control that
// does nothing is worse than an absent one - the customer who turns it on believes something will
// happen:
//   News Notification -> the Premium news feed. Its own visibility was
//                        ServersUiController.hasServersFromGatewayApi, false for every rent this
//                        client can hold, so it was ALREADY invisible; it leaves so nobody has to
//                        work that out again.
//   Check for updates -> CoreController::checkForUpdates() returns unconditionally in this fork
//                        (18-3e), because the in-app updater is Amnezia's gateway against a
//                        compiled-in key we do not carry. The SETTING stays, so the row returns
//                        with the AresVPN update channel (18-3d).
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
    objectName: "page:PageSettingsApplication"

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
            text: qsTr("Application")
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
            text: qsTr("What the app does to itself.")
            color: AresStyle.color.textMute
            font.family: AresStyle.font.family
            font.pixelSize: AresStyle.size.small
            wrapMode: Text.WordWrap
        }

        Flickable {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: AresStyle.space.lg
            Layout.rightMargin: AresStyle.space.lg
            contentHeight: rows.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            ColumnLayout {
                id: rows
                width: parent.width
                spacing: AresStyle.space.sm

                AresRow {
                    Layout.fillWidth: true
                    title: qsTr("Language")
                    subtitle: LanguageUiController.currentLanguageName
                    onClicked: selectLanguageDrawer.openTriggered()
                }

                AresRow {
                    Layout.fillWidth: true
                    visible: Qt.platform.os === "android" && !SettingsController.isNotificationPermissionGranted
                    title: qsTr("Enable notifications")
                    subtitle: qsTr("Show the tunnel's state in the status bar")
                    onClicked: SettingsController.requestNotificationPermission()
                }

                AresRow {
                    Layout.fillWidth: true
                    visible: GC.isMobile()
                    toggleable: true
                    title: qsTr("Allow screenshots")
                    checked: SettingsController.isScreenshotsEnabled()
                    onToggled: function(value) {
                        if (value !== SettingsController.isScreenshotsEnabled()) {
                            SettingsController.toggleScreenshotsEnabled(value)
                        }
                        checked = value
                    }
                }

                AresRow {
                    id: autoStartRow
                    Layout.fillWidth: true
                    visible: !GC.isMobile()
                    toggleable: true
                    title: qsTr("Start with the system")
                    subtitle: qsTr("Launch AresVPN when this device starts")
                    checked: SettingsController.autoStartEnabled
                    onToggled: function(value) {
                        if (value !== SettingsController.autoStartEnabled) {
                            SettingsController.toggleAutoStart(value)
                        }
                    }
                }

                AresRow {
                    objectName: "app.autoconnect"
                    Layout.fillWidth: true
                    visible: !GC.isMobile()
                    toggleable: true
                    title: qsTr("Connect on launch")
                    subtitle: qsTr("Bring the rent up as soon as the app opens")
                    checked: SettingsController.isAutoConnectEnabled()
                    onToggled: function(value) {
                        if (value !== SettingsController.isAutoConnectEnabled()) {
                            SettingsController.toggleAutoConnect(value)
                        }
                        // `checked:` above is bound to a FUNCTION CALL, which QML evaluates once
                        // and never again - so without this the row's own state went stale after
                        // one press and the switch was ONE-WAY: on, and never off again. Found by
                        // pressing it twice and reading the setting back (AresProject 18-3h).
                        checked = value
                    }
                }

                AresRow {
                    Layout.fillWidth: true
                    visible: !GC.isMobile()
                    toggleable: true
                    // upstream disables rather than hides this when autostart is off, and that is
                    // right: it explains WHY it does nothing instead of vanishing
                    enabled: SettingsController.autoStartEnabled
                    opacity: enabled ? 1.0 : 0.45
                    title: qsTr("Start minimised")
                    subtitle: qsTr("Only applies when starting with the system")
                    checked: SettingsController.autoStartEnabled && SettingsController.startMinimized
                    onToggled: function(value) {
                        if (value !== SettingsController.startMinimized) {
                            SettingsController.toggleStartMinimized(value)
                        }
                    }
                }

                AresRow {
                    objectName: "app.logging"
                    Layout.fillWidth: true
                    title: qsTr("Logging")
                    subtitle: SettingsController.isLoggingEnabled ? qsTr("Enabled") : qsTr("Disabled")
                    onClicked: PageController.goToPage(PageEnum.PageSettingsLogging)
                }

                // Destructive, so it is separated, named in full, and confirmed by the same
                // question drawer upstream used - the confirmation is not cosmetic here, it
                // deletes every rent this device holds.
                AresRow {
                    Layout.fillWidth: true
                    Layout.topMargin: AresStyle.space.lg
                    Layout.bottomMargin: AresStyle.space.xl + PageController.safeAreaBottomMargin
                    title: qsTr("Reset and remove all data")
                    subtitle: qsTr("Every rent on this device is forgotten")
                    onClicked: {
                        var headerText = qsTr("Reset settings and remove all data from the application?")
                        var descriptionText = qsTr("All settings will be reset to default and every rent this device holds will be forgotten. The rents themselves are not cancelled - log in again with the same idx to get them back.")
                        var yesButtonText = qsTr("Continue")
                        var noButtonText = qsTr("Cancel")

                        var yesButtonFunction = function() {
                            if (ServersUiController.isDefaultServerCurrentlyProcessed() && ConnectionController.isConnected) {
                                PageController.showNotificationMessage(qsTr("Cannot reset settings during active connection"))
                            } else {
                                SettingsController.clearSettings()
                                PageController.goToPageHome()
                            }
                        }
                        var noButtonFunction = function() {}

                        showQuestionDrawer(headerText, descriptionText, yesButtonText, noButtonText, yesButtonFunction, noButtonFunction)
                    }
                }
            }
        }
    }

    SelectLanguageDrawer {
        id: selectLanguageDrawer

        width: root.width
        height: root.height
    }
}
