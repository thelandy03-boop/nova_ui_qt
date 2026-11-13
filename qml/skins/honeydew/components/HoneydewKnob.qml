import QtQuick
import "../"

Item {
    id: root

    property string label: "KNOB"
    property real from: 0.0
    property real to: 1.0
    property real value: 0.5
    property color accentColor: HoneydewTheme.flOrange
    property string suffix: ""
    property bool isBipolar: false

    signal valueModified(real newVal)

    width: 56
    height: 72

    Column {
        anchors.centerIn: parent
        spacing: 4

        // Disco y Arco del Knob
        Item {
            width: 44
            height: 44
            anchors.horizontalCenter: parent.horizontalCenter

            Canvas {
                id: knobCanvas
                anchors.fill: parent
                antialiasing: true

                onPaint: {
                    var ctx = getContext("2d");
                    ctx.reset();

                    var cx = width / 2;
                    var cy = height / 2;
                    var radius = width / 2 - 4;
                    var startAngle = 0.75 * Math.PI;
                    var endAngle = 2.25 * Math.PI;
                    var totalAngle = endAngle - startAngle;

                    var norm = (root.value - root.from) / Math.max(0.0001, root.to - root.from);
                    norm = Math.max(0.0, Math.min(1.0, norm));
                    var currentAngle = startAngle + totalAngle * norm;

                    // 1. Pista de fondo (Arco inactivo)
                    ctx.beginPath();
                    ctx.arc(cx, cy, radius, startAngle, endAngle, false);
                    ctx.lineWidth = 3.5;
                    ctx.strokeStyle = "#2D303B";
                    ctx.lineCap = "round";
                    ctx.stroke();

                    // 2. Arco activo con color neón
                    ctx.beginPath();
                    if (root.isBipolar) {
                        var midAngle = startAngle + totalAngle * 0.5;
                        if (currentAngle >= midAngle) {
                            ctx.arc(cx, cy, radius, midAngle, currentAngle, false);
                        } else {
                            ctx.arc(cx, cy, radius, currentAngle, midAngle, false);
                        }
                    } else {
                        ctx.arc(cx, cy, radius, startAngle, currentAngle, false);
                    }
                    ctx.lineWidth = 3.5;
                    ctx.strokeStyle = root.accentColor;
                    ctx.lineCap = "round";
                    ctx.stroke();

                    // 3. Disco central
                    ctx.beginPath();
                    ctx.arc(cx, cy, radius - 5, 0, 2 * Math.PI, false);
                    ctx.fillStyle = mouseArea.pressed ? "#2C2E38" : "#1F2128";
                    ctx.fill();
                    ctx.lineWidth = 1.0;
                    ctx.strokeStyle = "#383C48";
                    ctx.stroke();

                    // 4. Marcador de aguja
                    ctx.save();
                    ctx.translate(cx, cy);
                    ctx.rotate(currentAngle);
                    ctx.beginPath();
                    ctx.moveTo(radius - 12, 0);
                    ctx.lineTo(radius - 6, 0);
                    ctx.lineWidth = 2.0;
                    ctx.strokeStyle = "#FFFFFF";
                    ctx.lineCap = "round";
                    ctx.stroke();
                    ctx.restore();
                }
            }

            MouseArea {
                id: mouseArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.SizeVerCursor

                property real startY: 0
                property real startVal: 0

                onPressed: (mouse) => {
                    startY = mouse.y;
                    startVal = root.value;
                }

                onPositionChanged: (mouse) => {
                    if (pressed) {
                        var dy = startY - mouse.y;
                        var step = (root.to - root.from) / 120.0;
                        var newVal = Math.max(root.from, Math.min(root.to, startVal + dy * step));
                        root.value = newVal;
                        root.valueModified(newVal);
                        knobCanvas.requestPaint();
                    }
                }

                onWheel: (wheel) => {
                    var step = (root.to - root.from) / 30.0;
                    var newVal = Math.max(root.from, Math.min(root.to, root.value + (wheel.angleDelta.y > 0 ? step : -step)));
                    root.value = newVal;
                    root.valueModified(newVal);
                    knobCanvas.requestPaint();
                }
            }
        }

        // Etiqueta de Texto y Valor
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.label
            color: HoneydewTheme.textSecondary
            font.pixelSize: 9
            font.bold: true
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: (root.value >= 0 && root.isBipolar ? "+" : "") + root.value.toFixed(1) + root.suffix
            color: mouseArea.pressed ? root.accentColor : HoneydewTheme.textPrimary
            font.pixelSize: 8
            font.bold: true
        }
    }

    onValueChanged: knobCanvas.requestPaint()
}
