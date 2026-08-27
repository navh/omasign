import QtQuick
import Omasign

Rectangle {
    id: root

    property var options: []          // list of strings
    property int currentIndex: 0

    implicitWidth: row.implicitWidth + 8
    implicitHeight: 34
    radius: Theme.radiusSmall
    color: Theme.surfaceAlt
    border.width: 1
    border.color: Theme.border

    Rectangle {
        id: highlight
        radius: Theme.radiusSmall - 1
        color: Theme.surface
        border.width: 1
        border.color: Theme.borderStrong
        // Derived, not measured off a delegate: Repeater's own item is also a
        // child of the row, so indexing into children is not dependable.
        width: (root.width - 8) / Math.max(1, root.options.length)
        height: root.height - 8
        y: 4
        x: 4 + root.currentIndex * width

        Behavior on x { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
    }

    Row {
        id: row
        anchors.fill: parent
        anchors.margins: 4

        Repeater {
            model: root.options
            delegate: Item {
                required property int index
                required property string modelData

                width: (root.width - 8) / Math.max(1, root.options.length)
                height: row.height

                Text {
                    anchors.centerIn: parent
                    text: parent.modelData
                    font.family: Theme.fontFamily
                    font.pixelSize: 13
                    font.weight: root.currentIndex === parent.index ? Font.Medium : Font.Normal
                    color: root.currentIndex === parent.index ? Theme.text : Theme.textMuted

                    Behavior on color { ColorAnimation { duration: 120 } }
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.currentIndex = parent.index
                }
            }
        }
    }
}
