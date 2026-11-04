import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

/**
 * Consola / Visor de logs simple para el editor DSP JSFX.
 * Muestra el registro de mensajes y estado de compilación del motor de audio
 * de forma limpia y directa con marca de tiempo [HH:MM:SS].
 */
Rectangle {
    id: root

    property string statusMessage: "Compiled successfully"
    property bool hasError: false

    Layout.fillWidth: true
    Layout.preferredHeight: (Qt.platform.os === "android") ? 36 : 28

    color: "#0F1115"
    border.color: "#1A1D24"
    border.width: 1

    property string currentTimeString: ""

    function updateTime() {
        var now = new Date()
        var pad = (n) => (n < 10 ? "0" + n : n)
        currentTimeString = pad(now.getHours()) + ":" + pad(now.getMinutes()) + ":" + pad(now.getSeconds())
    }

    Component.onCompleted: {
        updateTime()
    }

    onStatusMessageChanged: {
        updateTime()
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 10
        spacing: 8

        // 1. Etiqueta / Insignia LOGS
        Row {
            spacing: 6
            Layout.alignment: Qt.AlignVCenter

            Canvas {
                width: 11; height: 10
                anchors.verticalCenter: parent.verticalCenter
                onPaint: {
                    var ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    ctx.strokeStyle = root.hasError ? "#F85149" : "#58A6FF"
                    ctx.lineWidth = 1.3
                    ctx.beginPath()
                    ctx.moveTo(1.5, 1.5)
                    ctx.lineTo(5.5, 5)
                    ctx.lineTo(1.5, 8.5)
                    ctx.stroke()
                    ctx.beginPath()
                    ctx.moveTo(7.5, 8.5)
                    ctx.lineTo(10.5, 8.5)
                    ctx.stroke()
                }
                Connections {
                    target: root
                    function onHasErrorChanged() { parent.requestPaint() }
                }
            }

            Text {
                text: "LOGS"
                color: "#8B949E"
                font.pixelSize: 10
                font.bold: true
                font.family: "Segoe UI, Inter, Sans-serif"
                anchors.verticalCenter: parent.verticalCenter
            }

            // Led luminoso de estado
            Rectangle {
                width: 6; height: 6; radius: 3
                color: root.hasError ? "#F85149" : "#3FB950"
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        // Separador vertical tenue
        Rectangle {
            width: 1; height: 14
            color: "#252832"
            Layout.alignment: Qt.AlignVCenter
        }

        // 2. Línea de log activa (Timestamp + Estado + Mensaje)
        Row {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            spacing: 6

            Text {
                text: root.currentTimeString ? ("[" + root.currentTimeString + "]") : ""
                color: "#484F58"
                font.pixelSize: 11
                font.family: (Qt.platform.os === "android") ? "monospace" : "JetBrains Mono, Consolas, Courier New"
                anchors.verticalCenter: parent.verticalCenter
                visible: root.statusMessage.length > 0
            }

            Text {
                text: root.hasError ? "[ERROR]" : "[OK]"
                color: root.hasError ? "#F85149" : "#3FB950"
                font.pixelSize: 11
                font.bold: true
                font.family: (Qt.platform.os === "android") ? "monospace" : "JetBrains Mono, Consolas, Courier New"
                anchors.verticalCenter: parent.verticalCenter
                visible: root.statusMessage.length > 0
            }

            Text {
                width: parent.width - 160
                text: root.statusMessage.length > 0 ? root.statusMessage : "Esperando compilación..."
                color: root.statusMessage.length === 0 ? "#555C6E" : (root.hasError ? "#FFA39E" : "#C9D1D9")
                font.pixelSize: 11
                font.family: (Qt.platform.os === "android") ? "monospace" : "JetBrains Mono, Consolas, Courier New"
                elide: Text.ElideRight
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        // 3. Botón Limpiar log
        Rectangle {
            width: 44; height: 18
            radius: 3
            color: clearLogMa.containsMouse ? "#21262D" : "transparent"
            border.color: clearLogMa.containsMouse ? "#30363D" : "transparent"
            border.width: 1
            Layout.alignment: Qt.AlignVCenter

            Text {
                anchors.centerIn: parent
                text: "Limpiar"
                color: clearLogMa.containsMouse ? "#C9D1D9" : "#6E7681"
                font.pixelSize: 10
                font.family: "Segoe UI, Inter, Sans-serif"
            }

            MouseArea {
                id: clearLogMa
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    root.statusMessage = ""
                    root.hasError = false
                }
            }
        }
    }
}
