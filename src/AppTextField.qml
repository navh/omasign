import QtQuick
import QtQuick.Controls.Basic
import Omasign

TextField {
    id: control

    implicitHeight: 38
    leftPadding: 12
    rightPadding: 12
    font.family: Theme.fontFamily
    font.pixelSize: 14
    color: Theme.text
    placeholderTextColor: Theme.textFaint
    selectionColor: Theme.accent
    selectedTextColor: "#ffffff"

    background: Rectangle {
        radius: Theme.radiusSmall
        color: Theme.surface
        border.width: 1
        border.color: control.activeFocus ? Theme.accent : Theme.border

        Behavior on border.color { ColorAnimation { duration: 120 } }
    }
}
