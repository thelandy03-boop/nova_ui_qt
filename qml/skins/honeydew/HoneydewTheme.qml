pragma Singleton
import QtQuick

QtObject {
    id: root

    // ──────────────────────────────────────────────
    // MÉTRICAS Y UNIDAD 'U' EXACTA DE HONEYDEW
    // (1 U = base de diseño calculada en Rust metrics.rs)
    // ──────────────────────────────────────────────
    readonly property real u: 32.0

    // Alturas y anchos canónicos en U
    readonly property real transportH:          1.75 * u   // 56 px
    readonly property real rulerH:              1.65 * u   // 52.8 px
    readonly property real rowH:                2.00 * u   // 64 px
    readonly property real collapsedRowH:       1.25 * u   // 40 px
    readonly property real headerWCollapsed:    1.50 * u   // 48 px
    readonly property real headerWExpanded:    13.50 * u   // 432 px
    readonly property real beatW:               1.20 * u   // 38.4 px (zoom 0)
    readonly property real slotW:               1.75 * u   // 56 px (transport slot)
    readonly property real ctrlSlotW:           4.00 * u   // 128 px
    readonly property real panelButtonW:        1.50 * u   // 48 px (logo/rack button)
    readonly property real cornerW:             2.10 * u   // 67.2 px
    readonly property real addD:                2.00 * u   // 64 px (+ add button)
    readonly property real stripH:              3.00 * u   // 96 px (keyboard strip)

    // ──────────────────────────────────────────────
    // PALETA CANÓNICA DE HONEYDEW (theme.rs - Classic Gray)
    // ──────────────────────────────────────────────
    readonly property color background:         "#21272B"
    readonly property color controlFill:        "#31373B"
    readonly property color moduleFill:         "#394046"
    readonly property color menuFill:           "#434C51"
    readonly property color text:               "#C7CEDA"
    readonly property color textOnAccent:       "#21272B"
    readonly property color accent:             "#FB8B3F" // Naranja icónico Honeydew
    readonly property color recordColor:        "#FF0000" // Rojo fijo de grabación

    // Teclas de piano (theme.rs)
    readonly property color whiteKey:           "#C7CEDA"
    readonly property color blackKey:           "#21272B"
    readonly property color whiteKeyDisabled:   "#9FA6B0"
    readonly property color blackKeyDisabled:   "#717981"

    // Factores de opacidad (theme.rs y main_screen.rs)
    readonly property real disabledAlpha:       0.30
    readonly property real dividerAlpha:        0.18
    readonly property real verticalDividerAlpha:0.12
    readonly property real scaleAlpha:          0.60
    readonly property real hoverGain:           1.25

    // Aliases para compatibilidad con código existente
    readonly property color bgMain:             background
    readonly property color bgSurface:          controlFill
    readonly property color bgSurfaceLight:     moduleFill
    readonly property color bgCard:             moduleFill
    readonly property color bgCardHover:        menuFill
    readonly property color borderSubtle:       "#2A3136"
    readonly property color borderBright:       menuFill
    readonly property color flOrange:           accent
    readonly property color textPrimary:        text
    readonly property color textSecondary:      "#9FA6B0"
    readonly property color textMuted:          "#717981"

    // ──────────────────────────────────────────────
    // CÁLCULO MATEMÁTICO DE COLOR DE CANAL (theme.rs)
    // channel_color(h, theme) = hsl( (0.83 + 0.73 * h) % 1.0, 0.90, 0.63 )
    // ──────────────────────────────────────────────
    function hslToRgb(h, s, l) {
        if (s <= 0.0) {
            return Qt.rgba(l, l, l, 1.0);
        }
        let q = l < 0.5 ? l * (1.0 + s) : l + s - l * s;
        let p = 2.0 * l - q;
        function channel(t) {
            let tc = t;
            if (tc < 0.0) tc += 1.0;
            if (tc > 1.0) tc -= 1.0;
            if (tc < 1.0 / 6.0) return p + (q - p) * 6.0 * tc;
            if (tc < 0.5) return q;
            if (tc < 2.0 / 3.0) return p + (q - p) * (2.0 / 3.0 - tc) * 6.0;
            return p;
        }
        return Qt.rgba(
            Math.min(1.0, Math.max(0.0, channel(h + 1.0 / 3.0))),
            Math.min(1.0, Math.max(0.0, channel(h))),
            Math.min(1.0, Math.max(0.0, channel(h - 1.0 / 3.0))),
            1.0
        );
    }

    function channelColor(index) {
        let h = ((index * 0.1618) % 1.0 + 1.0) % 1.0;
        let hue = ((0.83 + 0.73 * h) % 1.0 + 1.0) % 1.0;
        return hslToRgb(hue, 0.90, 0.63);
    }

    function getTrackColor(index) {
        return channelColor(index);
    }
}
