import QtQuick
import QtQuick.Controls

import Style 1.0

// AresVPN Client (AresProject ROADMAP 18-3h): the tab bar is the ONLY chrome a customer sees on
// every screen, and it was still Amnezia's - a goldenApricot selected icon over an onyx bar, which
// is the one warm hue #D183 deliberately kept OUT of this UI so that "connected" and "expiring
// soon" never compete for a meaning. It was found by rendering on the real windows platform and
// LOOKING (#L055): every rebuilt screen sat above an orange home icon.
//
// This type has exactly one user, PageStart.qml's TabBar, so the tokens can move without touching
// anything else. Upstream's property names are kept so a merge lands cleanly.
TabButton {
    id: root

    property string hoveredColor: AresStyle.color.hover
    property string defaultColor: AresStyle.color.textMute
    property string selectedColor: AresStyle.color.accent

    property string image

    property bool isSelected: false

	property bool isFocusable: true

    Keys.onTabPressed: {
        FocusController.nextKeyTabItem()
    }

    Keys.onBacktabPressed: {
        FocusController.previousKeyTabItem()
    }

    Keys.onUpPressed: {
        FocusController.nextKeyUpItem()
    }
    
    Keys.onDownPressed: {
        FocusController.nextKeyDownItem()
    }
    
    Keys.onLeftPressed: {
        FocusController.nextKeyLeftItem()
    }

    Keys.onRightPressed: {
        FocusController.nextKeyRightItem()
    }
    
    property string borderFocusedColor: AmneziaStyle.color.paleGray
    property int borderFocusedWidth: 1

    property var clickedFunc

    hoverEnabled: true

    icon.source: image
    icon.color: isSelected ? selectedColor : defaultColor

    background: Rectangle {
        id: background
        anchors.fill: parent
        color: AmneziaStyle.color.transparent
        radius: 10

        border.color: root.activeFocus ? root.borderFocusedColor : AmneziaStyle.color.transparent
        border.width: root.activeFocus ? root.borderFocusedWidth : 0

    }

    MouseArea {
        anchors.fill: background
        cursorShape: Qt.PointingHandCursor
        enabled: false
    }
    
    Keys.onEnterPressed: {
        if (root.clickedFunc && typeof root.clickedFunc === "function") {
            root.clickedFunc()
        }
    }

    Keys.onReturnPressed: {
        if (root.clickedFunc && typeof root.clickedFunc === "function") {
            root.clickedFunc()
        }
    }

    onClicked: {
        if (root.clickedFunc && typeof root.clickedFunc === "function") {
            root.clickedFunc()
        }
    }
}
