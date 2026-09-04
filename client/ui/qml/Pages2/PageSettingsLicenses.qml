// AresVPN Client - the licence texts, readable FROM THE APPLICATION (AresProject ROADMAP 18-3d).
// Copyright (c) 2026 AresVPN. Licensed under the GNU General Public License v3.0 (see LICENSE).
//
// WHY THIS SCREEN EXISTS, and it is not tidiness. GPL-3 section 5 requires an interactive user
// interface to display an Appropriate Legal Notice, and section 0 defines that notice as one that
// tells the user, among other things, **how to view a copy of this License**. Until now the four
// texts were installed beside the executable and nothing in the product could open them, which
// satisfies the duty to REPRODUCE them and not the duty to let a user READ them. 18-3d listed this
// as the first of its three remaining decisions; the decision is that the texts travel inside the
// binary (client/licenses.qrc) and this page is where they open.
//
// FOUR TEXTS, and each is here for a different obligation:
//   GPL-3.0            - ours. The licence this program is under (#D177 rule 2).
//   THIRD_PARTY        - the attribution list. GPL-3 does not require it; the components' own
//                        licences do, and several are MIT/BSD/MPL whose whole condition is that
//                        the notice travels with the binary.
//   OFL PT Root UI     - #D180 rule 4. The bundled UI font ships under the SIL Open Font License
//                        and reproducing the licence is a condition of using it at all.
//   Wintun EULA        - #D175. A PROPRIETARY agreement that binds us whatever OUR licence is; the
//                        upstream recipe discards the text and we put it back.
//
// It is one page in two states rather than two pages: a list of the four, and a reader. A reader
// needs the whole screen for a 36 KB legal text, and a second PageEnum entry for what is really a
// selection would be a page a customer can land on with nothing chosen.
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
    objectName: "page:PageSettingsLicenses"

    // -1 is the list; 0..3 is the reader showing that document.
    property int selected: -1

    readonly property var documents: [
        {
            title: qsTr("GNU General Public License v3"),
            subtitle: qsTr("The licence this program is released under"),
            path: ":/licenses/GPL-3.0.txt"
        },
        {
            title: qsTr("Third-party components"),
            subtitle: qsTr("What this program includes, and under what terms"),
            path: ":/licenses/THIRD_PARTY_LICENSES.md"
        },
        {
            title: qsTr("SIL Open Font License - PT Root UI"),
            subtitle: qsTr("The licence of the typeface this interface is set in"),
            path: ":/licenses/OFL-PT-Root-UI.txt"
        },
        {
            title: qsTr("Wintun prebuilt binaries licence"),
            subtitle: qsTr("The terms of the Windows tunnel driver this product ships"),
            path: ":/licenses/wintun-prebuilt-binaries-license.txt"
        }
    ]

    // Loaded on demand and cached, so opening this screen costs nothing and re-opening a document
    // costs nothing twice.
    property var loaded: ({})

    function documentText(index) {
        var path = root.documents[index].path
        if (root.loaded[path] !== undefined) {
            return root.loaded[path]
        }

        // Read from the EMBEDDED resource through SystemController, which is already a QML
        // context property. XMLHttpRequest on the same qrc: URL was tried first and returned an
        // empty string under -platform offscreen while the resource was demonstrably in the
        // binary; the slot's own comment records that measurement. Both live in client/ui/, which
        // is where #D178 says our changes belong.
        var text = SystemController.readBundledLicence(path)

        // #L012: a step that reports success may have done nothing. An empty reader would look
        // like a licence with no text rather than like a failure, so say which it is - and say it
        // in a form a support conversation can act on.
        if (text === "") {
            text = qsTr("This licence text could not be read from the application (%1).\n\n"
                        + "The same texts are installed beside the program's executable, and the "
                        + "complete source of this program is at "
                        + "https://github.com/sexy-yume/aresvpn-client").arg(path)
        }

        root.loaded[path] = text
        return text
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
            id: backButton
            Layout.fillWidth: true
            Layout.topMargin: AresStyle.space.lg

            // In the reader, back means "back to the four", not "leave the screen". Only the list
            // state closes the page.
            backButtonFunction: function() {
                if (root.selected >= 0) {
                    root.selected = -1
                } else {
                    PageController.closePage()
                }
            }
        }

        Text {
            Layout.fillWidth: true
            Layout.leftMargin: AresStyle.space.lg
            Layout.rightMargin: AresStyle.space.lg
            Layout.topMargin: AresStyle.space.sm
            text: root.selected < 0 ? qsTr("Licences") : root.documents[root.selected].title
            color: AresStyle.color.text
            font.family: AresStyle.font.family
            font.pixelSize: AresStyle.size.title
            font.weight: Font.DemiBold
            wrapMode: Text.Wrap
        }

        // ---- the list state -----------------------------------------------------------------
        ColumnLayout {
            Layout.fillWidth: true
            Layout.leftMargin: AresStyle.space.lg
            Layout.rightMargin: AresStyle.space.lg
            Layout.topMargin: AresStyle.space.lg
            spacing: AresStyle.space.sm
            visible: root.selected < 0

            Text {
                Layout.fillWidth: true
                Layout.bottomMargin: AresStyle.space.sm
                text: qsTr("AresVPN Client is free software. You may redistribute it and change it "
                           + "under the terms of the GNU General Public License, version 3. It comes "
                           + "with ABSOLUTELY NO WARRANTY.")
                color: AresStyle.color.textDim
                font.family: AresStyle.font.family
                font.pixelSize: AresStyle.size.small
                wrapMode: Text.Wrap
            }

            Repeater {
                model: root.documents

                delegate: AresRow {
                    required property var modelData
                    // Asked for explicitly: a `required property var modelData` alone stops QML
                    // injecting `index`, and the objectName then reads "licenses.docundefined" -
                    // measured on PageSettings, where it made every row unfindable.
                    required property int index

                    objectName: "licenses.doc" + index
                    Layout.fillWidth: true

                    title: modelData.title
                    subtitle: modelData.subtitle

                    // Set the SELECTION and nothing else. The reader's text is BOUND to it, so
                    // the same screen can be reached without a press - which is what lets the
                    // render harness open a document deterministically instead of depending on a
                    // synthetic click landing (#L056: the walkthrough is flaky and may not be
                    // cited as a gate).
                    onClicked: {
                        root.selected = index
                        readerView.contentY = 0
                    }
                }
            }
        }

        // ---- the reader state ---------------------------------------------------------------
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: AresStyle.space.lg
            Layout.rightMargin: AresStyle.space.lg
            Layout.topMargin: AresStyle.space.lg
            Layout.bottomMargin: AresStyle.space.lg + PageController.safeAreaBottomMargin
            visible: root.selected >= 0

            radius: AresStyle.radius.sm
            color: AresStyle.color.surface
            border.width: 1
            border.color: AresStyle.color.line
            clip: true

            Flickable {
                id: readerView
                objectName: "licenses.reader"
                anchors.fill: parent
                anchors.margins: AresStyle.space.lg
                contentWidth: width
                contentHeight: reader.implicitHeight
                boundsBehavior: Flickable.StopAtBounds

                ScrollBar.vertical: ScrollBar {}

                TextEdit {
                    id: reader
                    objectName: "licenses.text"
                    width: readerView.width
                    // Bound, not assigned: setting `root.selected` is all it takes to open a
                    // document, from a click or from the harness.
                    text: root.selected >= 0 ? root.documentText(root.selected) : ""
                    readOnly: true
                    selectByMouse: true
                    // A legal text is WRAPPED, never elided: a licence with its right-hand edge cut
                    // off is not a licence anyone has been shown (#L055).
                    wrapMode: TextEdit.Wrap
                    // Deliberately plain text. THIRD_PARTY_LICENSES.md is Markdown and rendering it
                    // would let a link or an image in the notice reach the network from a screen
                    // whose whole job is to state facts.
                    textFormat: TextEdit.PlainText
                    color: AresStyle.color.textDim
                    selectionColor: AresStyle.color.accentSoft
                    selectedTextColor: AresStyle.color.text
                    font.family: AresStyle.font.mono
                    font.pixelSize: AresStyle.size.small
                }
            }
        }

        Item {
            Layout.fillHeight: true
            visible: root.selected < 0
        }
    }
}
