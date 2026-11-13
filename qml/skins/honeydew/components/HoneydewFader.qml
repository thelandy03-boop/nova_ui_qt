import QtQuick
import "../"

Item {
    id: root

    property real value: 0.85312 // Unity gain (0 dB) por defecto
    property real from: 0.0
    property real to: 1.25
    property color channelColor: HoneydewTheme.accent
    property string label: "VOL"

    signal valueModified(real newVal)

    width: 44
    height: 120

    // Normalización 0..1 del valor
    readonly property real normValue: Math.max(0.0, Math.min(1.0, (value - from) / Math.max(0.0001, to - from)))

    // Proporciones exactas de fader.rs
    // UNITY en fader.rs: 0.85312 (marca de 0 dB)
    readonly property real unityNorm: 0.85312

    Column {
        anchors.fill: parent
        spacing: 4

        // Etiqueta superior
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.label
            font.pixelSize: 9
            font.bold: true
            color: mouseArea.containsMouse ? HoneydewTheme.text : Qt.rgba(HoneydewTheme.text.r, HoneydewTheme.text.g, HoneydewTheme.text.b, 0.7)
        }

        // Pista y tapa del Fader
        Item {
            id: faderBody
            width: parent.width
            height: parent.height - 18
            anchors.horizontalCenter: parent.horizontalCenter

            // 1. Pista central (track)
            Rectangle {
                id: trackRect
                width: 6
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.topMargin: 12
                anchors.bottomMargin: 12
                anchors.horizontalCenter: parent.horizontalCenter
                radius: 3
                color: HoneydewTheme.controlFill
                border.color: HoneydewTheme.menuFill
                border.width: 1

                // Relleno activo desde el fondo hasta el valor actual
                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: parent.height * root.normValue
                    radius: 3
                    color: root.channelColor
                }

                // Ticks de Unity Gain (0 dB) a los lados
                Rectangle {
                    width: 6
                    height: 2
                    anchors.right: parent.left
                    anchors.rightMargin: 2
                    y: trackRect.height * (1.0 - root.unityNorm)
                    color: HoneydewTheme.menuFill
                }
                Rectangle {
                    width: 6
                    height: 2
                    anchors.left: parent.right
                    anchors.leftMargin: 2
                    y: trackRect.height * (1.0 - root.unityNorm)
                    color: HoneydewTheme.menuFill
                }
            }

            // 2. Tapa del Fader (Cap exacta de fader.rs)
            // moduleFill con inset de channelColor y línea central menuFill
            Rectangle {
                id: capRect
                width: 32
                height: 18
                radius: 3
                anchors.horizontalCenter: trackRect.horizontalCenter
                y: trackRect.y + trackRect.height * (1.0 - root.normValue) - height / 2
                color: HoneydewTheme.moduleFill
                border.color: mouseArea.pressed ? HoneydewTheme.text : HoneydewTheme.menuFill
                border.width: 1

                // Recuadro interior con color de canal (cap_inner)
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 2
                    radius: 2
                    color: Qt.rgba(root.channelColor.r, root.channelColor.g, root.channelColor.b, 0.25)
                    border.color: root.channelColor
                    border.width: 1

                    // Línea horizontal central en menuFill (cap_line)
                    Rectangle {
                        anchors.centerIn: parent
                        width: parent.width - 6
                        height: 2
                        color: mouseArea.pressed ? "#FFFFFF" : HoneydewTheme.menuFill
                    }
                }
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
                }

                onDoubleClicked: {
                    // Reset a Unity Gain (fader.rs: double tap resets to default)
                    var defaultVal = root.from + (root.to - root.from) * root.unityNorm;
                    root.value = defaultVal;
                    root.valueModified(defaultVal);
                }

                onPositionChanged: (mouse) => {
                    if (pressed) {
                        var trackH = trackRect.height;
                        var deltaY = startY - mouse.y;
                        var range = root.to - root.from;
                        var step = (deltaY / trackH) * range;
                        var newVal = Math.max(root.from, Math.min(root.to, startVal + step));
                        if (newVal !== root.value) {
                            root.value = newVal;
                            root.valueModified(newVal);
                        }
                    }
                }
            }
        }
    }
}
