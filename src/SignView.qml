import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Omasign

Item {
    id: root

    signal openSettings()
    signal openRequested()

    // Zoom is expressed as a multiple of "fits the viewport", so the default is
    // always sensible whatever the window size or page dimensions.
    property real zoom: 1.0

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ------------------------------------------------------------ toolbar
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 52
            color: Theme.surface

            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: Theme.border
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.spacing
                anchors.rightMargin: Theme.spacing
                spacing: Theme.spacing

                Image {
                    source: "qrc:/marks/omasign-mark.svg"
                    sourceSize: Qt.size(22, 22)
                    Layout.preferredWidth: 22
                    Layout.preferredHeight: 22
                    opacity: 0.9
                }

                ColumnLayout {
                    spacing: 0
                    Layout.maximumWidth: 320

                    Text {
                        Layout.fillWidth: true
                        text: backend.hasDocument ? backend.documentName : qsTr("Omasign")
                        elide: Text.ElideMiddle
                        font.family: Theme.fontFamily
                        font.pixelSize: 13
                        font.weight: Font.Medium
                        color: Theme.text
                    }
                    Text {
                        Layout.fillWidth: true
                        visible: backend.hasDocument && backend.existingSignatureCount > 0
                        text: backend.existingSignatureCount === 1
                              ? qsTr("1 existing signature")
                              : qsTr("%1 existing signatures").arg(backend.existingSignatureCount)
                        font.family: Theme.fontFamily
                        font.pixelSize: 11
                        color: Theme.textFaint
                    }
                }

                Item { Layout.fillWidth: true }

                // page navigation
                RowLayout {
                    spacing: 2
                    visible: backend.pageCount > 1

                    AppButton {
                        text: "‹"
                        implicitWidth: 30
                        leftPadding: 0
                        rightPadding: 0
                        enabled: backend.currentPage > 0
                        onClicked: backend.currentPage = backend.currentPage - 1
                    }
                    Text {
                        text: qsTr("%1 / %2").arg(backend.currentPage + 1).arg(backend.pageCount)
                        font.family: Theme.fontFamily
                        font.pixelSize: 12
                        color: Theme.textMuted
                        horizontalAlignment: Text.AlignHCenter
                        Layout.minimumWidth: 54
                    }
                    AppButton {
                        text: "›"
                        implicitWidth: 30
                        leftPadding: 0
                        rightPadding: 0
                        enabled: backend.currentPage < backend.pageCount - 1
                        onClicked: backend.currentPage = backend.currentPage + 1
                    }
                }

                RowLayout {
                    spacing: 2
                    visible: backend.hasDocument

                    AppButton {
                        text: "−"
                        implicitWidth: 30
                        leftPadding: 0
                        rightPadding: 0
                        onClicked: root.zoom = Math.max(0.5, root.zoom - 0.25)
                    }
                    AppButton {
                        text: qsTr("Fit")
                        implicitWidth: 44
                        leftPadding: 0
                        rightPadding: 0
                        onClicked: root.zoom = 1.0
                    }
                    AppButton {
                        text: "+"
                        implicitWidth: 30
                        leftPadding: 0
                        rightPadding: 0
                        onClicked: root.zoom = Math.min(5.0, root.zoom + 0.25)
                    }
                }

                AppButton {
                    text: qsTr("Open…")
                    onClicked: root.openRequested()
                }

                AppButton {
                    text: qsTr("Settings")
                    onClicked: root.openSettings()
                }
            }
        }

        // ------------------------------------------------------------- canvas
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            // ---- empty state
            Item {
                anchors.fill: parent
                visible: !backend.hasDocument

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: Theme.spacing
                    width: Math.min(420, parent.width - 2 * Theme.spacingLoose)

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Drop a PDF here")
                        horizontalAlignment: Text.AlignHCenter
                        font.family: Theme.fontFamily
                        font.pixelSize: 18
                        font.weight: Font.Medium
                        color: Theme.text
                    }
                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Then drag a rectangle where the signature goes.")
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        font.family: Theme.fontFamily
                        font.pixelSize: 13
                        color: Theme.textMuted
                    }
                    AppButton {
                        Layout.alignment: Qt.AlignHCenter
                        text: qsTr("Choose a file…")
                        onClicked: root.openRequested()
                    }
                }
            }

            // ---- the page
            Flickable {
                id: flick
                anchors.fill: parent
                visible: backend.hasDocument
                clip: true
                contentWidth: Math.max(width, pageFrame.width + 2 * Theme.spacingLoose)
                contentHeight: Math.max(height, pageFrame.height + 2 * Theme.spacingLoose)
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar {}
                ScrollBar.horizontal: ScrollBar {}

                // Ctrl+wheel zooms, like every other document viewer.
                WheelHandler {
                    acceptedModifiers: Qt.ControlModifier
                    onWheel: function (event) {
                        const step = event.angleDelta.y > 0 ? 1.1 : 1 / 1.1
                        root.zoom = Math.max(0.5, Math.min(5.0, root.zoom * step))
                    }
                }

                Rectangle {
                    id: pageFrame

                    // Fit the page inside the viewport, then apply the zoom.
                    readonly property real aspect: backend.currentPageAspect
                    readonly property real fitWidth: {
                        const availW = flick.width - 2 * Theme.spacingLoose
                        const availH = flick.height - 2 * Theme.spacingLoose
                        if (availW <= 0 || availH <= 0 || aspect <= 0)
                            return 100
                        return Math.min(availW, availH * aspect)
                    }

                    width: Math.max(80, fitWidth * root.zoom)
                    height: width / Math.max(0.01, aspect)
                    x: Math.max(Theme.spacingLoose, (flick.contentWidth - width) / 2)
                    y: Math.max(Theme.spacingLoose, (flick.contentHeight - height) / 2)

                    color: "#ffffff"
                    border.width: 1
                    border.color: Theme.paperEdge

                    Image {
                        id: pageImage
                        anchors.fill: parent
                        anchors.margins: 1
                        asynchronous: true
                        cache: false
                        fillMode: Image.PreserveAspectFit
                        // The revision forces a reload when the document behind
                        // the same page index changes.
                        source: backend.hasDocument
                                ? "image://page/%1/%2".arg(backend.currentPage).arg(backend.renderRevision)
                                : ""
                        sourceSize.width: Math.round(pageFrame.width * Screen.devicePixelRatio)
                    }

                    // ---- drag a rectangle to sign
                    MouseArea {
                        id: placer
                        anchors.fill: parent
                        cursorShape: Qt.CrossCursor
                        hoverEnabled: true

                        property real startX: 0
                        property real startY: 0
                        property bool dragging: false

                        readonly property real rectX: Math.min(startX, mouseX)
                        readonly property real rectY: Math.min(startY, mouseY)
                        readonly property real rectW: Math.abs(mouseX - startX)
                        readonly property real rectH: Math.abs(mouseY - startY)

                        // Ignore stray clicks and micro-drags: a real signature
                        // rectangle is at least a few percent of the page.
                        readonly property bool bigEnough: rectW > width * 0.04 && rectH > height * 0.012

                        onPressed: function (mouse) {
                            startX = mouse.x
                            startY = mouse.y
                            dragging = true
                        }

                        onReleased: {
                            if (dragging && bigEnough) {
                                backend.signRectangle(backend.currentPage,
                                                      rectX / width, rectY / height,
                                                      rectW / width, rectH / height)
                            }
                            dragging = false
                        }

                        onCanceled: dragging = false
                    }

                    // ---- live preview of what will land on the page
                    Item {
                        x: placer.rectX
                        y: placer.rectY
                        width: placer.rectW
                        height: placer.rectH
                        visible: placer.dragging && placer.rectW > 2 && placer.rectH > 2

                        SignatureAppearance {
                            anchors.fill: parent
                            signer: backend
                            opacity: placer.bigEnough ? 1.0 : 0.45
                        }

                        Rectangle {
                            anchors.fill: parent
                            color: placer.bigEnough ? "transparent" : Theme.accentQuiet
                            border.width: 1
                            border.color: Theme.accent
                        }
                    }
                }
            }
        }

        // ------------------------------------------------------------- footer
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 30
            visible: backend.hasDocument
            color: Theme.surface

            Rectangle {
                anchors.top: parent.top
                width: parent.width
                height: 1
                color: Theme.border
            }

            Text {
                anchors.left: parent.left
                anchors.leftMargin: Theme.spacing
                anchors.verticalCenter: parent.verticalCenter
                text: backend.busy
                      ? qsTr("Signing…")
                      : qsTr("Drag a rectangle where the signature goes — signing writes a new, timestamped file.")
                font.family: Theme.fontFamily
                font.pixelSize: 11
                color: Theme.textFaint
            }

            Text {
                anchors.right: parent.right
                anchors.rightMargin: Theme.spacing
                anchors.verticalCenter: parent.verticalCenter
                text: Math.round(root.zoom * 100) + "%"
                font.family: Theme.fontFamily
                font.pixelSize: 11
                color: Theme.textFaint
            }
        }
    }

    // Dropping a file anywhere in the view opens it.
    DropArea {
        anchors.fill: parent
        onDropped: function (drop) {
            if (drop.hasUrls && drop.urls.length > 0)
                backend.open(drop.urls[0])
        }
    }
}
