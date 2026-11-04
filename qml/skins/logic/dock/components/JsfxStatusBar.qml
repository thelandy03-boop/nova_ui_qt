import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

/**
 * Barra de estado y consola inferior del editor JSFX.
 * Muestra el estado de compilación y mensajes del motor DSP.
 */
Rectangle {
    id: root

    property string statusMessage: "Compiled successfully"
    property bool hasError: false

    Layout.fillWidth: true
    Layout.preferredHeight: (Qt.platform.os === "android") ? 36 : 30
    color: "#121316"
    border.color: "#1E2028"
    border.width: 1

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        spacing: 10

        // 1. Etiqueta / Insignia CONSOLA
        Row {
            spacing: 6
            Layout.alignment: Qt.AlignVCenter

            Canvas {
                width: 12; height: 10
                anchors.verticalCenter: parent.verticalCenter
                onPaint: {
                    var ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    ctx.strokeStyle = "#8B949E"
                    ctx.lineWidth = 1.3
                    // Chevron prompt >
                    ctx.beginPath()
                    ctx.moveTo(1.5, 1.5)
                    ctx.lineTo(5.5, 5)
                    ctx.lineTo(1.5, 8.5)
                    ctx.stroke()
                    // Guión bajo _
                    ctx.beginPath()
                    ctx.moveTo(7.5, 8.5)
                    ctx.lineTo(11.5, 8.5)
                    ctx.stroke()
                }
            }

            Text {
                text: "CONSOLA"
                color: "#7D8590"
                font.pixelSize: 10
                font.bold: true
                font.family: "Segoe UI, Inter, Sans-serif"
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        // Separador
        Rectangle {
            width: 1; height: 14
            color: "#252832"
            Layout.alignment: Qt.AlignVCenter
        }

        // 2. Píldora de estado (éxito o error)
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 20
            Layout.alignment: Qt.AlignVCenter
            color: root.hasError ? "#281216" : "#0E2216"
            border.color: root.hasError ? "#5E1E26" : "#1B472C"
            border.width: 1
            radius: 3

            Row {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 7

                // Indicador luminoso
                Rectangle {
                    width: 6; height: 6; radius: 3
                    anchors.verticalCenter: parent.verticalCenter
                    color: root.hasError ? "#F85149" : "#3FB950"
                }

                // Mensaje de la consola
                Text {
                    width: parent.width - 20
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.statusMessage
                    color: root.hasError ? "#FFA39E" : "#7EE787"
                    font.pixelSize: 11
                    font.family: "Monospace, Consolas, 'Courier New'"
                    elide: Text.ElideRight
                }
            }
        }

        // 3. Monitor de estado del motor DSP en tiempo real
        Row {
            spacing: 6
            Layout.alignment: Qt.AlignVCenter
            visible: root.width > 480

            Rectangle {
                width: 6; height: 6; radius: 3
                color: "#3FB950"
                anchors.verticalCenter: parent.verticalCenter
            }

            Text {
                text: "EEL2 Engine"
                color: "#5C6370"
                font.pixelSize: 10
                font.family: "Monospace, Consolas, Courier New"
                font.bold: true
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }
}
