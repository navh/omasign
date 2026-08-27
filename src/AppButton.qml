import QtQuick
import QtQuick.Controls.Basic
import Omasign

Button {
    id: control

    property bool primary: false
    property bool danger: false

    implicitHeight: 36
    leftPadding: 16
    rightPadding: 16
    font.family: Theme.fontFamily
    font.pixelSize: 13
    font.weight: Font.Medium

    contentItem: Text {
        text: control.text
        font: control.font
        color: control.primary ? "#ffffff"
             : control.danger ? Theme.accent
             : Theme.text
        opacity: control.enabled ? 1.0 : 0.4
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: Theme.radiusSmall
        color: {
            if (control.primary)
                return control.down ? Qt.darker(Theme.accent, 1.2)
                     : control.hovered ? Qt.lighter(Theme.accent, 1.08)
                     : Theme.accent
            if (control.down)
                return Theme.surfaceAlt
            return control.hovered ? Theme.surfaceAlt : "transparent"
        }
        border.width: control.primary ? 0 : 1
        border.color: control.hovered ? Theme.borderStrong : Theme.border
        opacity: control.enabled ? 1.0 : 0.5

        Behavior on color { ColorAnimation { duration: 110 } }
    }
}
