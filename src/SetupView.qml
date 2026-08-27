import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Omasign

Item {
    id: root

    signal finished()

    property string errorText: ""

    Flickable {
        anchors.fill: parent
        contentWidth: width
        contentHeight: card.implicitHeight + 2 * Theme.spacingSection
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}

        ColumnLayout {
            id: card
            width: Math.min(620, root.width - 2 * Theme.spacingLoose)
            x: (root.width - width) / 2
            y: Theme.spacingSection
            spacing: Theme.spacingLoose

            // ---- header
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacing

                Image {
                    source: "qrc:/marks/omasign-mark.svg"
                    sourceSize: Qt.size(40, 40)
                    Layout.preferredWidth: 40
                    Layout.preferredHeight: 40
                }

                ColumnLayout {
                    spacing: 2
                    Text {
                        text: qsTr("Omasign")
                        font.family: Theme.fontFamily
                        font.pixelSize: 26
                        font.weight: Font.DemiBold
                        color: Theme.text
                    }
                    Text {
                        text: qsTr("Set up the signature you'll put on documents.")
                        font.family: Theme.fontFamily
                        font.pixelSize: 14
                        color: Theme.textMuted
                    }
                }
                Item { Layout.fillWidth: true }
            }

            // ---- who
            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingTight

                FieldLabel { text: qsTr("Name") }
                AppTextField {
                    id: nameField
                    Layout.fillWidth: true
                    placeholderText: qsTr("The name you sign with")
                    text: backend.suggestedName()
                }

                Item { height: Theme.spacingTight; width: 1 }

                FieldLabel { text: qsTr("Email (optional)") }
                AppTextField {
                    id: emailField
                    Layout.fillWidth: true
                    placeholderText: qsTr("Included in the certificate")
                    text: backend.signerEmail
                }
            }

            // ---- what it looks like
            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.spacing

                FieldLabel { text: qsTr("Signature") }

                SegmentedControl {
                    id: modeControl
                    Layout.preferredWidth: 220
                    options: [qsTr("Typed"), qsTr("Drawn")]
                    currentIndex: backend.useInk ? 1 : 0
                }

                // The pad only appears for the drawn option; the preview below
                // updates either way.
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 180
                    visible: modeControl.currentIndex === 1
                    radius: Theme.radius
                    color: Theme.surface
                    border.width: 1
                    border.color: Theme.border

                    // A ruled line to sign above, the way a paper form has one.
                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.leftMargin: 28
                        anchors.rightMargin: 28
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 42
                        height: 1
                        color: Theme.border
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 14
                        text: qsTr("Draw your signature above the line")
                        visible: pad.empty
                        font.family: Theme.fontFamily
                        font.pixelSize: 12
                        color: Theme.textFaint
                    }

                    SignaturePad {
                        id: pad
                        anchors.fill: parent
                        anchors.margins: 8
                        inkColour: Theme.darkMode ? "#f0f0f2" : "#131313"
                        onStrokesChanged: {
                            if (!empty)
                                backend.captureInk(pad)
                        }
                    }

                    Row {
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 8
                        spacing: 4
                        visible: !pad.empty

                        AppButton {
                            text: qsTr("Undo")
                            implicitHeight: 26
                            font.pixelSize: 12
                            onClicked: pad.undoStroke()
                        }
                        AppButton {
                            text: qsTr("Clear")
                            implicitHeight: 26
                            font.pixelSize: 12
                            onClicked: pad.clear()
                        }
                    }
                }
            }

            // ---- preview
            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingTight

                FieldLabel { text: qsTr("On the page") }

                // Shown over a sheet of paper, with the rule a form would have,
                // so the transparency is visible rather than implied.
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 120
                    radius: Theme.radius
                    color: "#ffffff"
                    border.width: 1
                    border.color: Theme.border
                    clip: true

                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.leftMargin: 24
                        anchors.rightMargin: 24
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.verticalCenterOffset: 30
                        height: 1
                        color: "#c9c7c0"
                    }

                    SignatureAppearance {
                        anchors.centerIn: parent
                        width: Math.min(parent.width - 48, 320)
                        height: width * 0.22
                        signer: backend
                        overrideMode: true
                        preferInk: modeControl.currentIndex === 1
                        nameOverride: nameField.text
                    }
                }
            }

            // ---- commit
            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.spacing

                Text {
                    Layout.fillWidth: true
                    visible: root.errorText !== ""
                    text: root.errorText
                    wrapMode: Text.WordWrap
                    font.family: Theme.fontFamily
                    font.pixelSize: 13
                    color: Theme.warning
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacing

                    AppButton {
                        text: qsTr("Create my signature")
                        primary: true
                        enabled: nameField.text.trim().length > 0
                                 && (modeControl.currentIndex === 0 || !pad.empty)
                        onClicked: {
                            backend.signerName = nameField.text
                            backend.signerEmail = emailField.text
                            var problem = backend.completeSetup(nameField.text,
                                                                emailField.text,
                                                                modeControl.currentIndex === 1)
                            if (problem === "") {
                                root.errorText = ""
                                root.finished()
                            } else {
                                root.errorText = problem
                            }
                        }
                    }
                    Item { Layout.fillWidth: true }
                }

                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: qsTr("Omasign creates a self-signed certificate in your personal key store. "
                             + "Readers will show the signature as valid but the issuer as untrusted, "
                             + "which is what most institutional signatures look like too. To use a "
                             + "certificate from a real authority instead, import it with "
                             + "pk12util and pick it in Settings.")
                    font.family: Theme.fontFamily
                    font.pixelSize: 12
                    lineHeight: 1.35
                    color: Theme.textFaint
                }
            }

            Item { height: Theme.spacingSection; width: 1 }
        }
    }
}
