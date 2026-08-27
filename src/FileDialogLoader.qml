import QtQuick
import QtQuick.Dialogs

// A file dialog that is only instantiated the first time it is needed, so the
// portal is not contacted at startup.
Item {
    id: root

    function open() {
        loader.active = true
        if (loader.item)
            loader.item.open()
    }

    Loader {
        id: loader
        active: false
        sourceComponent: FileDialog {
            title: qsTr("Open a PDF")
            nameFilters: [qsTr("PDF documents (*.pdf)"), qsTr("All files (*)")]
            onAccepted: backend.open(selectedFile)
        }
        onLoaded: item.open()
    }
}
