pragma Singleton

import QtQuick

// One palette, two grounds. `darkMode` is bound to the backend from Main.qml
// rather than read here, so this file stays free of dependencies.
QtObject {
    id: theme

    property bool darkMode: true

    // Signature red: the colour of the mark, and the only colour in the
    // interface that is not a neutral. Used sparingly, on purpose.
    readonly property color accent: darkMode ? "#ef6461" : "#c1121f"
    readonly property color accentQuiet: darkMode ? Qt.rgba(0.94, 0.39, 0.38, 0.16)
                                                  : Qt.rgba(0.76, 0.07, 0.12, 0.10)

    readonly property color background: darkMode ? "#17181b" : "#f7f6f3"
    readonly property color surface: darkMode ? "#1e2024" : "#ffffff"
    readonly property color surfaceAlt: darkMode ? "#25272c" : "#efeeea"
    readonly property color border: darkMode ? "#32353b" : "#e3e1db"
    readonly property color borderStrong: darkMode ? "#44484f" : "#cdcac2"

    readonly property color text: darkMode ? "#e9e9ec" : "#17171a"
    readonly property color textMuted: darkMode ? "#9a9aa2" : "#6b6b70"
    readonly property color textFaint: darkMode ? "#6c6f77" : "#96959a"

    readonly property color warning: darkMode ? "#e0a458" : "#a1671b"
    readonly property color success: darkMode ? "#8fbf7f" : "#3f7a3a"

    // The paper a PDF page is drawn on, and the shadow under it.
    readonly property color paperEdge: darkMode ? "#000000" : "#b8b6ae"

    readonly property string fontFamily: "Inter"

    readonly property int radiusSmall: 6
    readonly property int radius: 10
    readonly property int radiusLarge: 14

    readonly property int spacingTight: 6
    readonly property int spacing: 12
    readonly property int spacingLoose: 20
    readonly property int spacingSection: 32
}
