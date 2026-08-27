import QtQuick
import QtQuick.Controls.Basic
import Omasign

ApplicationWindow {
    id: window

    width: 1080
    height: 820
    minimumWidth: 720
    minimumHeight: 560
    visible: true
    title: backend.hasDocument ? qsTr("%1 — Omasign").arg(backend.documentName) : qsTr("Omasign")
    color: Theme.background

    // Theme is a singleton with no dependencies of its own; the ground it
    // paints comes from the desktop, via the backend.
    Binding {
        target: Theme
        property: "darkMode"
        value: backend.darkMode
    }

    // Setup runs once. After that the app is only ever the signing view.
    property bool setupDone: backend.configured

    Loader {
        anchors.fill: parent
        sourceComponent: window.setupDone ? signComponent : setupComponent
    }

    Component {
        id: setupComponent
        SetupView {
            onFinished: window.setupDone = true
        }
    }

    Component {
        id: signComponent
        SignView {
            onOpenSettings: settingsSheet.open()
            onOpenRequested: fileDialog.open()
        }
    }

    SettingsSheet {
        id: settingsSheet
        parent: Overlay.overlay
    }

    Toast {
        id: toast
        parent: Overlay.overlay
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Theme.spacingLoose
        onOpenRequested: backend.openLastOutput()
        onRevealRequested: backend.revealLastOutput()
    }

    Connections {
        target: backend

        function onSignedSuccessfully(outputPath) {
            toast.show(qsTr("Signed."), outputPath, false, true)
        }

        function onFailed(message) {
            toast.show(qsTr("Could not sign this document."), message, true, false)
        }
    }

    Shortcut {
        // `sequences` rather than `sequence`: StandardKey.Open maps to more
        // than one chord, and binding just the first one warns.
        sequences: [StandardKey.Open]
        onActivated: fileDialog.open()
    }
    Shortcut {
        sequence: "Ctrl+Q"
        onActivated: Qt.quit()
    }
    Shortcut {
        sequence: "Ctrl+W"
        onActivated: Qt.quit()
    }

    // One dialog for the whole window: the sign view asks for it, and so does
    // Ctrl+O from the setup screen.
    FileDialogLoader { id: fileDialog }
}
