import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

/**
 * Consola / Visor de logs del editor DSP JSFX con diseño de estudio profesional.
 * Presenta una cápsula de estado con micro-insignias dinámicas de éxito o error,
 * marcas de tiempo precisas y botón de limpieza con diseño moderno.
 */
Rectangle {
    id: root

    property string statusMessage: "Compiled successfully"
    property bool hasError: false

    Layout.fillWidth: true
    Layout.preferredHeight: (Qt.platform.os === "android") ? 36 : 30

    color: "#0D0E12"
    border.color: "#181A22"
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
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        spacing: 8

        // 1. Insignia LOGS (Estilo Chip satinado)
        Rectangle {
            Layout.preferredHeight: 20
            Layout.preferredWidth: 64
            Layout.alignment: Qt.AlignVCenter
            radius: 3
            color: "#151821"
            border.color: "#212634"
            border.width: 1

            Row {
                anchors.centerIn: parent
                spacing: 5

                // Icono vectorial terminal / log
                Canvas {
                    width: 9; height: 9
                    anchors.verticalCenter: parent.verticalCenter
                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.clearRect(0, 0, width, height)
                        ctx.strokeStyle = root.hasError ? "#F85149" : "#58A6FF"
                        ctx.lineWidth = 1.3
                        // Prompt >
                        ctx.beginPath()
                        ctx.moveTo(1, 1.5)
                        ctx.lineTo(4.5, 4.5)
                        ctx.lineTo(1, 7.5)
                        ctx.stroke()
                        // Guión _
                        ctx.beginPath()
                        ctx.moveTo(6, 7.5)
                        ctx.lineTo(8.5, 7.5)
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
                    font.pixelSize: 9
                    font.bold: true
                    font.family: "Segoe UI, Inter, Sans-serif"
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }

        // 2. Cápsula de estado del motor DSP (Log Capsule)
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 22
            Layout.alignment: Qt.AlignVCenter
            radius: 4
            color: root.statusMessage.length === 0 
                   ? "#12141A" 
                   : (root.hasError ? "#221114" : "#0B1C13")
            border.color: root.statusMessage.length === 0 
                          ? "#1B1F28" 
                          : (root.hasError ? "#5E1E26" : "#184529")
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 6

                // Micro-insignia con icono de estado
                Rectangle {
                    width: 13; height: 13; radius: 6.5
                    Layout.alignment: Qt.AlignVCenter
                    color: root.statusMessage.length === 0 
                           ? "#212634" 
                           : (root.hasError ? "#3D1418" : "#133520")
                    border.color: root.statusMessage.length === 0 
                                  ? "#303648" 
                                  : (root.hasError ? "#F85149" : "#3FB950")
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: root.statusMessage.length === 0 ? "•" : (root.hasError ? "✕" : "✓")
                        color: root.statusMessage.length === 0 ? "#7D8590" : (root.hasError ? "#F85149" : "#3FB950")
                        font.pixelSize: 8
                        font.bold: true
                    }
                }

                // Marca de tiempo con tipografía monoespaciada
                Text {
                    visible: root.statusMessage.length > 0 && root.currentTimeString.length > 0
                    text: "[" + root.currentTimeString + "]"
                    color: root.hasError ? "#7A4247" : "#466E53"
                    font.pixelSize: 11
                    font.family: (Qt.platform.os === "android") ? "monospace" : "JetBrains Mono, Consolas, Courier New"
                    Layout.alignment: Qt.AlignVCenter
                }

                // Mensaje del log
                Text {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignVCenter
                    text: root.statusMessage.length > 0 ? root.statusMessage : "Esperando ejecución o compilación de script..."
                    color: root.statusMessage.length === 0 
                           ? "#545B6B" 
                           : (root.hasError ? "#FFA39E" : "#7EE787")
                    font.pixelSize: 11
                    font.family: (Qt.platform.os === "android") ? "monospace" : "JetBrains Mono, Consolas, Courier New"
                    elide: Text.ElideRight
                }
            }
        }

        // 3. Botón Limpiar estilizado
        Rectangle {
            Layout.preferredWidth: 54
            Layout.preferredHeight: 20
            Layout.alignment: Qt.AlignVCenter
            radius: 3
            color: clearLogMa.containsMouse ? "#212634" : "#141720"
            border.color: clearLogMa.containsMouse ? "#3A435A" : "#1E2330"
            border.width: 1

            Row {
                anchors.centerIn: parent
                spacing: 4

                // Icono papelera / limpiar minimalista
                Canvas {
                    width: 8; height: 8
                    anchors.verticalCenter: parent.verticalCenter
                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.clearRect(0, 0, width, height)
                        ctx.strokeStyle = clearLogMa.containsMouse ? "#C9D1D9" : "#6E7681"
                        ctx.lineWidth = 1
                        ctx.beginPath()
                        ctx.moveTo(1.5, 1.5); ctx.lineTo(6.5, 6.5)
                        ctx.moveTo(6.5, 1.5); ctx.lineTo(1.5, 6.5)
                        ctx.stroke()
                    }
                    Connections {
                        target: clearLogMa
                        function onContainsMouseChanged() { parent.requestPaint() }
                    }
                }

                Text {
                    text: "Limpiar"
                    color: clearLogMa.containsMouse ? "#E6EDF3" : "#7D8590"
                    font.pixelSize: 10
                    font.family: "Segoe UI, Inter, Sans-serif"
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            MouseArea {
                id: clearLogMa
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                ToolTip.visible: containsMouse
                ToolTip.text: "Limpiar mensajes de log"
                ToolTip.delay: 300
                onClicked: {
                    root.statusMessage = ""
                    root.hasError = false
                }
            }
        }
    }
}
