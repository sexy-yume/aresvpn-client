import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Shapes

import PageEnum 1.0
import Style 1.0

import "./"
import "../Controls2"
import "../Controls2/TextTypes"
import "../Config"
import "../Components"

PageType {
    id: root

    property bool isControlsDisabled: false
    property bool isTabBarDisabled: false

    Connections {
        objectName: "pageControllerConnection"

        target: PageController

        function onGoToPageHome() {
            if (!AresProfileController.hasSession) {
                // AresVPN Client (#D187, superseding #D182): the branch is WHETHER THERE IS A
                // SESSION - not whether any server is stored. Those two coincide only by accident,
                // and building on the second is what turned this product into Amnezia's server
                // list with our login bolted onto its import screen.
                tabBar.visible = false
                tabBarStackView.goToTabBarPage(PageEnum.PageSetupWizardAresLogin)
            } else {
                tabBar.visible = true
                tabBar.setCurrentIndex(0)
                tabBarStackView.goToTabBarPage(PageEnum.PageHome)
            }
        }

        function onGoToPageSettings() {
            tabBar.setCurrentIndex(2)
            tabBarStackView.goToTabBarPage(PageEnum.PageSettings)
        }

        function onGoToPageViewConfig() {
            var pagePath = PageController.getPagePath(PageEnum.PageSetupWizardViewConfig)
            tabBarStackView.push(pagePath, { "objectName" : pagePath }, StackView.PushTransition)
        }

        function onGoToShareConnectionPage(headerText, configContentHeaderText, configCaption, configExtension, configFileName) {
            var pagePath = PageController.getPagePath(PageEnum.PageShareConnection)
            tabBarStackView.push(pagePath,
                                 { "objectName" : pagePath,
                                     "headerText" : headerText,
                                     "configContentHeaderText" : configContentHeaderText,
                                     "configCaption" : configCaption,
                                     "configExtension" : configExtension,
                                     "configFileName" : configFileName
                                 },
                                 StackView.PushTransition)
        }

        function onDisableControls(disabled) {
            isControlsDisabled = disabled
        }

        function onDisableTabBar(disabled) {
            isTabBarDisabled = disabled
        }

        function onClosePage() {
            if (tabBarStackView.depth <= 1) {
                PageController.hideWindow()
                return
            }
            tabBarStackView.pop()
        }

        function onGoToPage(page, slide) {
            var pagePath = PageController.getPagePath(page)

            if (slide) {
                tabBarStackView.push(pagePath, { "objectName" : pagePath }, StackView.PushTransition)
            } else {
                tabBarStackView.push(pagePath, { "objectName" : pagePath }, StackView.Immediate)
            }
        }

        function onGoToStartPage() {
            while (tabBarStackView.depth > 1) {
                tabBarStackView.pop()
            }
        }

        function onEscapePressed() {
            if (root.isControlsDisabled || root.isTabBarDisabled) {
                return
            }

            var pageName = tabBarStackView.currentItem.objectName
            if ((pageName === PageController.getPagePath(PageEnum.PageShare)) ||
                    (pageName === PageController.getPagePath(PageEnum.PageSettings)) ||
                    (pageName === PageController.getPagePath(PageEnum.PageSetupWizardConfigSource))) {
                PageController.goToPageHome()
            } else {
                PageController.closePage()
            }
        }
    }

    Connections {
        objectName: "connectionControllerConnections"

        target: ConnectionController

        function onNoInstalledContainers() {
            PageController.setTriggeredByConnectButton(true)

            ServersUiController.setProcessedServerId(ServersUiController.defaultServerId)
            PageController.goToPage(PageEnum.PageSetupWizardEasy)
        }
    }

    Connections {
        objectName: "installControllerConnections"

        target: InstallController

        function onInstallationErrorOccurred(error) {
            PageController.showBusyIndicator(false)

            PageController.showErrorMessage(error)

            var needCloseCurrentPage = false
            var currentPageName = tabBarStackView.currentItem.objectName

            if (currentPageName === PageController.getPagePath(PageEnum.PageSetupWizardInstalling)) {
                needCloseCurrentPage = true
            } else if (currentPageName === PageController.getPagePath(PageEnum.PageDeinstalling)) {
                needCloseCurrentPage = true
            }
            if (needCloseCurrentPage) {
                PageController.closePage()
            }
        }

        function onWrongInstallationUser(message) {
            onInstallationErrorOccurred(message)
        }

        function onUpdateContainerFinished(message, closePage) {
            PageController.showNotificationMessage(message)
            if (closePage) {
                PageController.closePage()
            }
        }

        function onCachedProfileCleared(message) {
            PageController.showNotificationMessage(message)
        }

        function onRemoveServerFinished(finishedMessage) {
            if (!ServersUiController.getServersCount()) {
                PageController.goToPageHome()
            } else {
                PageController.goToStartPage()
                PageController.goToPage(PageEnum.PageSettingsServersList)
            }
            PageController.showNotificationMessage(finishedMessage)
        }

        function onRemoveAllContainersFinished(finishedMessage) {
            if (tabBarStackView.currentItem.objectName === PageController.getPagePath(PageEnum.PageDeinstalling)) {
                PageController.closePage()
            }
            PageController.showNotificationMessage(finishedMessage)
        }

        function onRemoveContainerFinished(finishedMessage) {
            if (tabBarStackView.currentItem.objectName === PageController.getPagePath(PageEnum.PageDeinstalling)) {
                PageController.closePage()
            }
            PageController.closePage()
            PageController.showNotificationMessage(finishedMessage)
        }
    }

    Connections {
        objectName: "importControllerConnections"

        target: ImportController

        function onImportErrorOccurred(error, goToPageHome) {
            PageController.showErrorMessage(error)
        }

        function onRestoreAppConfig(data) {
            PageController.showBusyIndicator(true)
            SettingsController.restoreAppConfigFromData(data)
            PageController.showBusyIndicator(false)
        }
    }

    Connections {
        objectName: "settingsControllerConnections"

        target: SettingsController

        function onLoggingDisableByWatcher() {
            PageController.showNotificationMessage(qsTr("Logging was disabled after 14 days, log files were deleted"))
        }

        function onRestoreBackupFinished() {
            PageController.showNotificationMessage(qsTr("Settings restored from backup file"))
            PageController.goToPageHome()
        }

        function onLoggingStateChanged() {
            if (SettingsController.isLoggingEnabled) {
                var message = qsTr("Logging is enabled. Note that logs will be automatically" +
                                   "disabled after 14 days, and all log files will be deleted.")
                PageController.showNotificationMessage(message)
            }
        }
    }

    Connections {
        target: SubscriptionUiController

        function onErrorOccurred(error) {
            PageController.showErrorMessage(error)
        }
    }

    Connections {
        target: SubscriptionUiController

        function onApiConfigRemoved(message) {
            PageController.showNotificationMessage(message)
        }

        function onApiServerRemoved(message) {
            if (!ServersUiController.getServersCount()) {
                PageController.goToPageHome()
            } else {
                PageController.goToStartPage()
                PageController.goToPage(PageEnum.PageSettingsServersList)
            }
            PageController.showNotificationMessage(message)
        }

        function onInstallServerFromApiFinished(message, preferredDefaultIndex) {
            PageController.goToPageHome()
            PageController.showNotificationMessage(message)
        }

        function onBackgroundPurchaseCompleted(message) {
            PageController.showNotificationMessage(message)
        }

        function onChangeApiCountryFinished(message) {
            PageController.goToPageHome()
            PageController.showNotificationMessage(message)
        }

        function onReloadServerFromApiFinished(message) {
            PageController.goToPageHome()
            PageController.showNotificationMessage(message)
        }
    }

    StackViewType {
        id: tabBarStackView
        objectName: "tabBarStackView"

        anchors.top: parent.top
        anchors.right: parent.right
        anchors.left: parent.left
        anchors.bottom: tabBar.top

        enabled: !root.isControlsDisabled

        function goToTabBarPage(page) {
            var pagePath = PageController.getPagePath(page)
            tabBarStackView.clear(StackView.Immediate)
            tabBarStackView.replace(pagePath, { "objectName" : pagePath }, StackView.Immediate)
        }

        Component.onCompleted: {
            var pagePath
            if (!AresProfileController.hasSession) {
                // AresVPN Client (#D187, the operator's requirement in their own words: *키자마자
                // 로그인이 안되어있으면 로그인화면이 나오고*). NOT SIGNED IN -> the login is the
                // first screen. This used to read `isStartPageVisible()`, which is exactly "zero
                // servers stored" - a different question. A device can hold a stored server and
                // not be signed in (a config imported by hand), and a signed-in device whose rent
                // the console has not answered for yet holds none; the session is the state that
                // decides, and it is the only one.
                tabBar.visible = false
                pagePath = PageController.getPagePath(PageEnum.PageSetupWizardAresLogin)
            } else {
                tabBar.visible = true
                pagePath = PageController.getPagePath(PageEnum.PageHome)
                ServersUiController.setProcessedServerId(ServersUiController.defaultServerId)
            }

            tabBarStackView.push(pagePath, { "objectName" : pagePath })
        }

        Keys.onPressed: function(event) {
            switch (event.key) {
            case Qt.Key_Tab:
            case Qt.Key_Down:
            case Qt.Key_Right:
                FocusController.nextKeyTabItem()
                break
            case Qt.Key_Backtab:
            case Qt.Key_Up:
            case Qt.Key_Left:
                FocusController.previousKeyTabItem()
                break
            default:
                PageController.keyPressEvent(event.key)
                event.accepted = true
            }
        }
    }

    TabBar {
        id: tabBar
        objectName: "tabBar"

        anchors.right: parent.right
        anchors.left: parent.left
        anchors.bottom: parent.bottom

        // Also adjust TabBar position when keyboard appears (Android 14+ workaround)
        anchors.bottomMargin: PageController.imeHeight

        topPadding: 8
        bottomPadding: 8 + PageController.safeAreaBottomMargin
        leftPadding: 96
        rightPadding: 96

        height: visible ? homeTabButton.implicitHeight + tabBar.topPadding + tabBar.bottomPadding : 0

        enabled: !root.isControlsDisabled && !root.isTabBarDisabled

        background: Shape {
            objectName: "backgroundShape"

            width: parent.width
            height: parent.height

            ShapePath {
                startX: 0
                startY: 0

                PathLine { x: width; y: 0 }
                PathLine { x: width; y: tabBar.height - 1 }
                PathLine { x: 0; y: tabBar.height - 1 }
                PathLine { x: 0; y: 0 }

                // AresVPN Client (18-3h): the bar itself in our tokens, so the chrome under every
                // rebuilt screen belongs to the same design as the screen.
                strokeWidth: 1
                strokeColor: AresStyle.color.line
                fillColor: AresStyle.color.bg
            }
        }

        TabImageButtonType {
            id: homeTabButton
            objectName: "homeTabButton"

            isSelected: tabBar.currentIndex === 0
            image: "qrc:/images/controls/home.svg"
            clickedFunc: function () {
                tabBarStackView.goToTabBarPage(PageEnum.PageHome)
                ServersUiController.setProcessedServerId(ServersUiController.defaultServerId)
                tabBar.currentIndex = 0
            }
        }

        TabImageButtonType {
            id: shareTabButton
            objectName: "shareTabButton"

            Connections {
                target: ServersModel

                function onModelReset() {
                    if (!SettingsController.isOnTv()) {
                        var hasServerWithWriteAccess = ServersUiController.hasServerWithWriteAccess()
                        shareTabButton.visible = hasServerWithWriteAccess
                        shareTabButton.width = hasServerWithWriteAccess ? undefined : 0
                    }
                }
            }

            visible: !SettingsController.isOnTv() && ServersUiController.hasServerWithWriteAccess()
            width: !SettingsController.isOnTv() && ServersUiController.hasServerWithWriteAccess() ? undefined : 0

            isSelected: tabBar.currentIndex === 1
            image: "qrc:/images/controls/share-2.svg"
            clickedFunc: function () {
                tabBarStackView.goToTabBarPage(PageEnum.PageShare)
                tabBar.currentIndex = 1
            }
        }

        TabImageButtonType {
            id: settingsTabButton
            objectName: "settingsTabButton"

            isSelected: tabBar.currentIndex === 2
            image: (ServersUiController.hasServersFromGatewayApi && NewsModel.hasUnread && SettingsController.isNewsNotificationsEnabled()) ? "qrc:/images/controls/settings-news.svg" : "qrc:/images/controls/settings.svg"
            Binding {
                target: settingsTabButton
                property: "defaultColor"
                value: "transparent"
                when: (ServersUiController.hasServersFromGatewayApi && NewsModel.hasUnread)
            }
            clickedFunc: function () {
                tabBarStackView.goToTabBarPage(PageEnum.PageSettings)
                tabBar.currentIndex = 2
            }
        }

        // THE "+" TAB IS GONE (AresVPN Client, #D187). Under a session model there is nothing to
        // add: this device holds ONE session, and changing rent means changing the session's idx,
        // which is a different act with a different screen. The button first pointed at upstream's
        // config-source wizard (file / QR / text key / SSH install) and was then repointed at the
        // login as "add a rent" - both were the same misreading, and the operator named it:
        // *지금처럼 + 버튼 눌러서 id pw idx입력해서 설정만 가져와서 렌트목록에 추가하는게 아님*.
    }
}
