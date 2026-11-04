import QtQuick

Item {
    id: root

    property string label: "BAND"
    property real value: 0.5 // 0.0 a 1.0
    property string displayValue: "0.0 dB"
    property color activeColor: "#3B82F6"

    signal valueChangedByUser(real newValue)

    implicitWidth: 48
    implicitHeight: 110

    Column {
        anchors.fill: parent
        spacing: 4

        // Rótulo superior
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.label
            color: "#8E94A5"
            font.pixelSize: 9
            font.bold: true
            font.letterSpacing: 0.5
        }

        // Valor numérico
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.displayValue
            color: "#E2E8F0"
            font.pixelSize: 10
            font.bold: true
        }

        // Track del slider vertical
        Rectangle {
            id: trackRect
            anchors.horizontalCenter: parent.horizontalCenter
            width: 8
            height: 64
            radius: 4
            color: "#181A22"
            border.color: "#2C2E3C"
            border.width: 1

            // Relleno activo desde el fondo
            Rectangle {
                anchors.bottom: parent.bottom
                anchors.horizontalCenter: parent.horizontalCenter
                width: parent.width
                height: Math.max(4, parent.height * Math.max(0.0, Math.min(1.0, root.value)))
                radius: 4
                color: root.activeColor
            }

            // Perilla táctil (thumb)
            Rectangle {
                id: thumb
                anchors.horizontalCenter: parent.horizontalCenter
                y: (trackRect.height - height) * (1.0 - Math.max(0.0, Math.min(1.0, root.value)))
                width: 18
                height: 12
                radius: 3
                color: dragArea.containsPress ? "#FFFFFF" : "#CBD5E1"
                border.color: "#0F172A"
                border.width: 1

                // Muesca central
                Rectangle {
                    anchors.centerIn: parent
                    width: 8
                    height: 2
                    color: "#334155"
                }
            }

            // Área táctil ampliada
            MouseArea {
                id: dragArea
                anchors.fill: parent
                anchors.margins: -14
                preventStealing: true
                hoverEnabled: true
                cursorShape: Qt.SizeVerCursor

                property real lastY: 0

                onPressed: (mouse) => {
                    lastY = mouse.y
                    if (typeof AudioEngine !== "undefined" && AudioEngine.hapticFeedback) {
                        AudioEngine.hapticFeedback()
                    }
                }

                onPositionChanged: (mouse) => {
                    var deltaY = lastY - mouse.y
                    lastY = mouse.y
                    var deltaValue = deltaY / trackRect.height
                    var newVal = Math.max(0.0, Math.min(1.0, root.value + deltaValue))
                    if (newVal !== root.value) {
                        root.value = newVal
                        root.valueChangedByUser(newVal)
                    }
                }

                onWheel: (wheel) => {
                    var delta = (wheel.angleDelta.y > 0) ? 0.02 : -0.02
                    var newVal = Math.max(0.0, Math.min(1.0, root.value + delta))
                    if (newVal !== root.value) {
                        root.value = newVal
                        root.valueChangedByUser(newVal)
                    }
                }

                onDoubleClicked: {
                    if (typeof AudioEngine !== "undefined" && AudioEngine.hapticFeedback) {
                        AudioEngine.hapticFeedback()
                    }
                    root.value = 0.5
                    root.valueChangedByUser(0.5)
                }
            }
        }
    }
}
