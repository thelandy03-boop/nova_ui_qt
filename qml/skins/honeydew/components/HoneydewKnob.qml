import QtQuick
import "../"

Item {
    id: root

    property string label: "KNOB"
    property real from: 0.0
    property real to: 1.0
    property real value: 0.5
    property color accentColor: HoneydewTheme.accent
    property string suffix: ""
    property bool isBipolar: false

    signal valueModified(real newVal)

    width: 56
    height: 72

    // Normalización 0..1 del valor
    readonly property real normValue: Math.max(0.0, Math.min(1.0, (value - from) / Math.max(0.0001, to - from)))

    // Animación de pulso/swell al presionar/soltar (arc.rs)
    property real swell: 1.0

    ParallelAnimation {
        id: swellAnim
        running: false
        NumberAnimation {
            target: root
            property: "swell"
            to: 1.08
            duration: 90
            easing.type: Easing.OutQuad
        }
    }
    ParallelAnimation {
        id: releaseAnim
        running: false
        NumberAnimation {
            target: root
            property: "swell"
            to: 1.0
            duration: 180
            easing.type: Easing.OutBounce
        }
    }

    Column {
        anchors.centerIn: parent
        spacing: 3

        // Etiqueta superior
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.label
            font.pixelSize: 9
            font.weight: Font.Medium
            color: mouseArea.containsMouse ? HoneydewTheme.text : Qt.rgba(HoneydewTheme.text.r, HoneydewTheme.text.g, HoneydewTheme.text.b, 0.7)
        }

        // Anillo de diseño Honeydew (widgets/knob.rs)
        Item {
            id: knobContainer
            width: 44
            height: 44
            anchors.horizontalCenter: parent.horizontalCenter
            scale: root.swell

            Canvas {
                id: knobCanvas
                anchors.fill: parent
                antialiasing: true

                Connections {
                    target: root
                    function onNormValueChanged() { knobCanvas.requestPaint() }
                    function onAccentColorChanged() { knobCanvas.requestPaint() }
                }

                onPaint: {
                    var ctx = getContext("2d");
                    ctx.reset();

                    var d = Math.min(width, height);
                    var cx = width / 2;
                    var cy = height / 2;
                    var v = root.normValue;
                    var t = 1.5; // hairline thickness
                    var s_max = Math.min(0.05 * d, 2.0 * t);
                    var growth = root.isBipolar ? Math.abs(2.0 * v - 1.0) : v;
                    var s = t + (s_max - t) * growth;

                    // Radios exactos de widgets/knob.rs:
                    // outer: 0.48 D - s
                    // inner: 0.48 D - 3 s
                    var outer_r = 0.48 * d - s;
                    var inner_r = 0.48 * d - 3.0 * s;

                    // Ángulo del punto indicador (dot_angle en Rust: (0.05 + 0.9 * v) * TAU desde abajo)
                    var angle = (0.05 + 0.9 * v) * 2.0 * Math.PI + (0.5 * Math.PI); // offset a down
                    var sin_a = Math.sin(angle);
                    var cos_a = Math.cos(angle);
                    var dot_dist = 0.48 * d - 2.0 * s;
                    var dot_x = cx + dot_dist * cos_a;
                    var dot_y = cy + dot_dist * sin_a;

                    var dot_r = 1.5 * s_max;
                    var halo_r = 1.5 * s_max + 2.0 * t;

                    // 1. Disco exterior (color de acento / canal)
                    ctx.beginPath();
                    ctx.arc(cx, cy, outer_r, 0, 2.0 * Math.PI, false);
                    ctx.fillStyle = root.accentColor;
                    ctx.fill();

                    // 2. Disco interior (moduleFill)
                    ctx.beginPath();
                    ctx.arc(cx, cy, inner_r, 0, 2.0 * Math.PI, false);
                    ctx.fillStyle = HoneydewTheme.moduleFill;
                    ctx.fill();

                    // Iluminación suave al estar presionado / hovered (WASH = 0.05 en Rust)
                    if (mouseArea.pressed || mouseArea.containsMouse) {
                        ctx.beginPath();
                        ctx.arc(cx, cy, inner_r, 0, 2.0 * Math.PI, false);
                        ctx.fillStyle = Qt.rgba(root.accentColor.r, root.accentColor.g, root.accentColor.b, 0.08);
                        ctx.fill();
                    }

                    // 3. Halo del punto indicador (moduleFill)
                    var grow = (mouseArea.pressed || mouseArea.containsMouse) ? 1.15 : 1.0;
                    ctx.beginPath();
                    ctx.arc(dot_x, dot_y, grow * halo_r, 0, 2.0 * Math.PI, false);
                    ctx.fillStyle = HoneydewTheme.moduleFill;
                    ctx.fill();

                    // 4. Punto indicador (color de canal)
                    ctx.beginPath();
                    ctx.arc(dot_x, dot_y, grow * dot_r, 0, 2.0 * Math.PI, false);
                    ctx.fillStyle = root.accentColor;
                    ctx.fill();
                }
            }

            // Valor en el centro del knob
            Text {
                anchors.centerIn: parent
                text: {
                    var val = root.value;
                    if (root.suffix === "%") return Math.round(val * 100) + "%";
                    if (root.suffix === "dB") return (val > 0 ? "+" : "") + val.toFixed(1) + "dB";
                    if (Math.abs(val) < 10) return val.toFixed(2) + root.suffix;
                    return Math.round(val) + root.suffix;
                }
                font.pixelSize: 8
                font.bold: true
                color: HoneydewTheme.text
            }

            MouseArea {
                id: mouseArea
                anchors.fill: parent
                hoverEnabled: true
                preventStealing: true

                property real startY: 0
                property real startVal: 0

                onPressed: (mouse) => {
                    startY = mouse.y;
                    startVal = root.value;
                    swellAnim.restart();
                    knobCanvas.requestPaint();
                }

                onReleased: {
                    releaseAnim.restart();
                    knobCanvas.requestPaint();
                }

                onPositionChanged: (mouse) => {
                    if (pressed) {
                        var deltaY = startY - mouse.y;
                        var range = root.to - root.from;
                        var step = (deltaY / 120.0) * range;
                        var newVal = Math.max(root.from, Math.min(root.to, startVal + step));
                        if (newVal !== root.value) {
                            root.value = newVal;
                            root.valueModified(newVal);
                        }
                    }
                }

                onWheel: (wheel) => {
                    var range = root.to - root.from;
                    var step = (wheel.angleDelta.y / 120.0) * 0.05 * range;
                    var newVal = Math.max(root.from, Math.min(root.to, root.value + step));
                    if (newVal !== root.value) {
                        root.value = newVal;
                        root.valueModified(newVal);
                    }
                }
            }
        }
    }
}
