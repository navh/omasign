import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Omasign

Popup {
    id: sheet

    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    padding: 0

    width: Math.min(560, parent ? parent.width - 2 * Theme.spacingLoose : 560)
    height: Math.min(620, parent ? parent.height - 2 * Theme.spacingLoose : 620)
    anchors.centerIn: parent

    background: Rectangle {
        radius: Theme.radiusLarge
        color: Theme.background
        border.width: 1
        border.color: Theme.border
    }

    Overlay.modal: Rectangle {
        color: Qt.rgba(0, 0, 0, Theme.darkMode ? 0.55 : 0.28)
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ---- title bar
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 50
            color: "transparent"

            Text {
                anchors.left: parent.left
                anchors.leftMargin: Theme.spacingLoose
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Settings")
                font.family: Theme.fontFamily
                font.pixelSize: 16
                font.weight: Font.DemiBold
                color: Theme.text
            }

            AppButton {
                anchors.right: parent.right
                anchors.rightMargin: Theme.spacing
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Done")
                onClicked: sheet.close()
            }

            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: Theme.border
            }
        }

        // ---- body
        Flickable {
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: width
            contentHeight: body.implicitHeight + 2 * Theme.spacingLoose
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}

            ColumnLayout {
                id: body
                width: parent.width - 2 * Theme.spacingLoose
                x: Theme.spacingLoose
                y: Theme.spacingLoose
                spacing: Theme.spacingLoose

                // ---- appearance
                FieldLabel { text: qsTr("APPEARANCE") }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 96
                    radius: Theme.radius
                    color: "#ffffff"
                    border.width: 1
                    border.color: Theme.border
                    clip: true

                    SignatureAppearance {
                        anchors.centerIn: parent
                        width: Math.min(parent.width - 32, 300)
                        height: width * 0.22
                        signer: backend
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacing

                    FieldLabel { text: qsTr("Style"); Layout.minimumWidth: 96 }

                    SegmentedControl {
                        Layout.preferredWidth: 200
                        options: [qsTr("Typed"), qsTr("Drawn")]
                        currentIndex: backend.useInk ? 1 : 0
                        onCurrentIndexChanged: {
                            if (currentIndex === 1 && !backend.hasInk) {
                                currentIndex = 0
                                redrawSheet.open()
                            } else {
                                backend.useInk = currentIndex === 1
                            }
                        }
                    }

                    AppButton {
                        text: backend.hasInk ? qsTr("Redraw…") : qsTr("Draw…")
                        onClicked: redrawSheet.open()
                    }

                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacing

                    FieldLabel { text: qsTr("Details"); Layout.minimumWidth: 96 }

                    Switch {
                        checked: backend.showMeta
                        onToggled: backend.showMeta = checked
                    }

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Show the signed-by and date block beside the mark")
                        wrapMode: Text.WordWrap
                        font.family: Theme.fontFamily
                        font.pixelSize: 12
                        color: Theme.textMuted
                    }
                }

                // ---- signature dictionary
                FieldLabel { text: qsTr("RECORDED WITH THE SIGNATURE") }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingTight

                    FieldLabel { text: qsTr("Reason (optional)") }
                    AppTextField {
                        Layout.fillWidth: true
                        text: backend.reason
                        placeholderText: qsTr("I am the author of this document")
                        onEditingFinished: backend.reason = text
                    }

                    Item { height: Theme.spacingTight; width: 1 }

                    FieldLabel { text: qsTr("Location (optional)") }
                    AppTextField {
                        Layout.fillWidth: true
                        text: backend.location
                        placeholderText: qsTr("Delft, NL")
                        onEditingFinished: backend.location = text
                    }
                }

                // ---- certificate
                FieldLabel { text: qsTr("CERTIFICATE") }

                ComboBox {
                    id: certBox
                    Layout.fillWidth: true
                    model: backend.certificates
                    textRole: "nickname"
                    font.family: Theme.fontFamily
                    font.pixelSize: 13

                    Component.onCompleted: syncSelection()
                    onModelChanged: syncSelection()

                    function syncSelection() {
                        for (var i = 0; i < model.length; ++i) {
                            if (model[i].nickname === backend.certNickname) {
                                currentIndex = i
                                return
                            }
                        }
                        currentIndex = -1
                    }

                    onActivated: {
                        if (currentIndex >= 0)
                            backend.certNickname = model[currentIndex].nickname
                    }
                }

                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    lineHeight: 1.35
                    font.family: Theme.fontFamily
                    font.pixelSize: 12
                    color: Theme.textMuted
                    text: {
                        var entry = certBox.currentIndex >= 0
                                    ? backend.certificates[certBox.currentIndex] : null
                        if (!entry)
                            return qsTr("No certificate selected.")
                        var lines = [entry.subject]
                        if (entry.expired)
                            lines.push(qsTr("This certificate has expired."))
                        else if (entry.selfSigned)
                            lines.push(qsTr("Self-signed: readers will report the issuer as untrusted."))
                        else
                            lines.push(qsTr("Issued by %1").arg(entry.issuer))
                        return lines.join("\n")
                    }
                }

                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    lineHeight: 1.35
                    font.family: Theme.fontFamily
                    font.pixelSize: 12
                    color: Theme.textFaint
                    text: qsTr("To use a certificate issued to you by an authority, import it into "
                             + "your key store and it will appear above:\n"
                             + "pk12util -i yourcert.p12 -d sql:$HOME/.pki/nssdb")
                }

                Item { height: Theme.spacing; width: 1 }
            }
        }
    }

    // Drawing a new signature gets its own small sheet so the pad has room.
    Popup {
        id: redrawSheet
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape
        width: Math.min(480, sheet.width - 2 * Theme.spacing)
        height: 300
        anchors.centerIn: parent
        padding: Theme.spacingLoose

        background: Rectangle {
            radius: Theme.radius
            color: Theme.background
            border.width: 1
            border.color: Theme.borderStrong
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: Theme.spacing

            Text {
                text: qsTr("Draw your signature")
                font.family: Theme.fontFamily
                font.pixelSize: 15
                font.weight: Font.DemiBold
                color: Theme.text
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: Theme.radiusSmall
                color: Theme.surface
                border.width: 1
                border.color: Theme.border

                SignaturePad {
                    id: redrawPad
                    anchors.fill: parent
                    anchors.margins: 8
                    inkColour: Theme.darkMode ? "#f0f0f2" : "#131313"
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacing

                AppButton { text: qsTr("Clear"); onClicked: redrawPad.clear() }
                Item { Layout.fillWidth: true }
                AppButton { text: qsTr("Cancel"); onClicked: redrawSheet.close() }
                AppButton {
                    text: qsTr("Use this")
                    primary: true
                    enabled: !redrawPad.empty
                    onClicked: {
                        if (backend.captureInk(redrawPad)) {
                            backend.useInk = true
                            redrawSheet.close()
                        }
                    }
                }
            }
        }

        onOpened: redrawPad.clear()
    }
}
