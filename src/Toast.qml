import QtQuick
import QtQuick.Layouts
import Omasign

// The one piece of feedback that matters: what was written, and how to get to
// it. Sits over the page rather than interrupting with a dialog.
Rectangle {
    id: toast

    property bool isError: false
    property string message: ""
    property string detail: ""
    property bool showActions: false

    signal openRequested()
    signal revealRequested()

    width: Math.min(560, parent ? parent.width - 2 * Theme.spacingLoose : 560)
    implicitHeight: layout.implicitHeight + 2 * Theme.spacing
    radius: Theme.radius
    color: Theme.surface
    border.width: 1
    border.color: isError ? Theme.warning : Theme.border

    opacity: 0
    visible: opacity > 0
    Behavior on opacity { NumberAnimation { duration: 180 } }

    function show(text, detailText, error, actions) {
        message = text
        detail = detailText || ""
        isError = error === true
        showActions = actions === true
        opacity = 1
        hideTimer.restart()
    }

    function hide() {
        opacity = 0
        hideTimer.stop()
    }

    Timer {
        id: hideTimer
        interval: toast.isError ? 9000 : 12000
        onTriggered: toast.hide()
    }

    RowLayout {
        id: layout
        anchors.fill: parent
        anchors.margins: Theme.spacing
        spacing: Theme.spacing

        Rectangle {
            Layout.alignment: Qt.AlignTop
            Layout.topMargin: 3
            width: 8
            height: 8
            radius: 4
            color: toast.isError ? Theme.warning : Theme.success
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            Text {
                Layout.fillWidth: true
                text: toast.message
                wrapMode: Text.WordWrap
                font.family: Theme.fontFamily
                font.pixelSize: 13
                font.weight: Font.Medium
                color: Theme.text
            }
            Text {
                Layout.fillWidth: true
                visible: toast.detail !== ""
                text: toast.detail
                wrapMode: Text.WrapAnywhere
                font.family: Theme.fontFamily
                font.pixelSize: 12
                color: Theme.textMuted
            }
        }

        AppButton {
            visible: toast.showActions
            text: qsTr("Open")
            onClicked: toast.openRequested()
        }
        AppButton {
            visible: toast.showActions
            text: qsTr("Folder")
            onClicked: toast.revealRequested()
        }
        AppButton {
            text: "✕"
            implicitWidth: 30
            leftPadding: 0
            rightPadding: 0
            onClicked: toast.hide()
        }
    }
}
