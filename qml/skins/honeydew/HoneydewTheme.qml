pragma Singleton
import QtQuick

QtObject {
    id: root

    // ──────────────────────────────────────────────
    // PALETA FL STUDIO MOBILE / HONEYDEW
    // ──────────────────────────────────────────────
    readonly property color bgMain:         "#131417"
    readonly property color bgSurface:      "#1B1C22"
    readonly property color bgSurfaceLight: "#24262E"
    readonly property color bgCard:         "#1E2027"
    readonly property color bgCardHover:    "#282A33"
    readonly property color borderSubtle:   "#2B2E38"
    readonly property color borderBright:   "#3D4150"

    // Colores de Acento FL Studio
    readonly property color flOrange:       "#FF8500"
    readonly property color flOrangeGlow:   "#40FF8500"
    readonly property color flGreen:        "#2FE575"
    readonly property color flGreenGlow:    "#402FE575"
    readonly property color flRed:          "#FF334B"
    readonly property color flRedGlow:      "#40FF334B"
    readonly property color flCyan:         "#00D2FF"
    readonly property color flPurple:       "#BD10E0"
    readonly property color flYellow:       "#FFD200"

    // Textos
    readonly property color textPrimary:    "#F5F6F8"
    readonly property color textSecondary:  "#9BA1B0"
    readonly property color textMuted:      "#626875"

    // Colores de pistas (Presets vibrantes FL)
    readonly property var trackPalette: [
        "#FF5252", // Rojo coral
        "#FF7043", // Naranja cálido
        "#FFA726", // Ámbar
        "#66BB6A", // Verde hoja
        "#26A69A", // Verde azulado
        "#29B6F6", // Celeste
        "#5C6BC0", // Índigo
        "#AB47BC", // Púrpura
        "#EC407A"  // Rosa fucsia
    ]

    function getTrackColor(index) {
        return trackPalette[index % trackPalette.length]
    }
}
