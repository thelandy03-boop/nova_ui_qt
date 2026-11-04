import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

/**
 * Consola / Terminal auténtica para el editor DSP JSFX.
 * Cuenta con cabecera de terminal (pestaña TERMINAL, botón limpiar y toggle expandir),
 * prompt de shell interactivo (nova@dsp:~$), marcas de tiempo [HH:MM:SS],
 * colores de salida [OK]/[ERR] y cursor terminal parpadeante.
 */
Rectangle {
    id: root

    property string statusMessage: "Compiled successfully"
    property bool hasError: false
    property bool expanded: false

    Layout.fillWidth: true
    Layout.preferredHeight: expanded ? 110 : ((Qt.platform.os === "android") ? 48 : 40)
    Behavior on Layout.preferredHeight {
        NumberAnimation { duration: 160; easing.type: Easing.OutCubic }
    }

    color: "#0A0C10"
    border.color: "#1A1D26"
    border.width: 1
    clip: true

    // Historial de logs de terminal
    ListModel {
        id: logModel

        Component.onCompleted: {
            root.pushLog(root.hasError, root.statusMessage)
        }
    }

    function pushLog(isErr, msg) {
        if (!msg || msg.trim().length === 0) return
        var now = new Date()
        var pad = (n) => (n < 10 ? "0" + n : n)
        var timeStr = pad(now.getHours()) + ":" + pad(now.getMinutes()) + ":" + pad(now.getSeconds())

        // Evitar duplicación del último mensaje idéntico
        if (logModel.count > 0) {
            var last = logModel.get(logModel.count - 1)
            if (last.text === msg && last.isError === isErr) return
        }

        logModel.append({
            "timestamp": timeStr,
            "isError": isErr,
            "text": msg
        })

        if (logModel.count > 25) {
            logModel.remove(0)
        }

        if (logView.count > 0) {
            logView.positionViewAtEnd()
        }
    }

    onStatusMessageChanged: {
        pushLog(root.hasError, root.statusMessage)
    }

    onHasErrorChanged: {
        pushLog(root.hasError, root.statusMessage)
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // 1. Barra de pestañas y herramientas de la terminal
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 22
            color: "#11141B"
            border.color: "#181B24"
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 8
                spacing: 6

                // Pestaña TERMINAL
                Row {
                    spacing: 6
                    Layout.alignment: Qt.AlignVCenter

                    Canvas {
                        width: 11; height: 10
                        anchors.verticalCenter: parent.verticalCenter
                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.clearRect(0, 0, width, height)
                            ctx.strokeStyle = "#58A6FF"
                            ctx.lineWidth = 1.3
                            // Prompt >
                            ctx.beginPath()
                            ctx.moveTo(1.5, 1.5)
                            ctx.lineTo(5.5, 5)
                            ctx.lineTo(1.5, 8.5)
                            ctx.stroke()
                            // Guión _
                            ctx.beginPath()
                            ctx.moveTo(7.5, 8.5)
                            ctx.lineTo(10.5, 8.5)
                            ctx.stroke()
                        }
                    }

                    Text {
                        text: "TERMINAL"
                        color: "#E6EDF3"
                        font.pixelSize: 10
                        font.bold: true
                        font.family: "Segoe UI, Inter, Sans-serif"
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    // Punto de estado
                    Rectangle {
                        width: 6; height: 6; radius: 3
                        color: root.hasError ? "#F85149" : "#3FB950"
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }

                Item { Layout.fillWidth: true }

                // Botón Limpiar (Clear)
                Rectangle {
                    width: 48; height: 16
                    radius: 3
                    color: clearMa.containsMouse ? "#21262D" : "transparent"
                    border.color: clearMa.containsMouse ? "#30363D" : "transparent"
                    border.width: 1
                    Layout.alignment: Qt.AlignVCenter

                    Text {
                        anchors.centerIn: parent
                        text: "Limpiar"
                        color: clearMa.containsMouse ? "#C9D1D9" : "#7D8590"
                        font.pixelSize: 10
                        font.family: "Segoe UI, Inter, Sans-serif"
                    }

                    MouseArea {
                        id: clearMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            logModel.clear()
                        }
                    }
                }

                // Botón Expandir / Colapsar terminal
                Rectangle {
                    width: 20; height: 16
                    radius: 3
                    color: expandMa.containsMouse ? "#21262D" : "transparent"
                    border.color: expandMa.containsMouse ? "#30363D" : "transparent"
                    border.width: 1
                    Layout.alignment: Qt.AlignVCenter

                    Canvas {
                        width: 8; height: 6
                        anchors.centerIn: parent
                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.clearRect(0, 0, width, height)
                            ctx.strokeStyle = expandMa.containsMouse ? "#C9D1D9" : "#7D8590"
                            ctx.lineWidth = 1.2
                            ctx.beginPath()
                            if (root.expanded) {
                                // Flecha hacia abajo
                                ctx.moveTo(1, 1.5)
                                ctx.lineTo(4, 4.5)
                                ctx.lineTo(7, 1.5)
                            } else {
                                // Flecha hacia arriba
                                ctx.moveTo(1, 4.5)
                                ctx.lineTo(4, 1.5)
                                ctx.lineTo(7, 4.5)
                            }
                            ctx.stroke()
                        }
                    }

                    MouseArea {
                        id: expandMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            root.expanded = !root.expanded
                        }
                    }
                }
            }
        }

        // 2. Área de salida de texto de la terminal
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ListView {
                id: logView
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                anchors.topMargin: 3
                anchors.bottomMargin: 3
                clip: true
                spacing: 2
                model: logModel

                delegate: Row {
                    spacing: 6
                    width: logView.width

                    // Timestamp
                    Text {
                        text: "[" + model.timestamp + "]"
                        color: "#484F58"
                        font.pixelSize: 11
                        font.family: (Qt.platform.os === "android") ? "monospace" : "JetBrains Mono, Consolas, Courier New"
                    }

                    // Prompt de terminal
                    Text {
                        text: "nova@dsp:~$ "
                        color: "#58A6FF"
                        font.bold: true
                        font.pixelSize: 11
                        font.family: (Qt.platform.os === "android") ? "monospace" : "JetBrains Mono, Consolas, Courier New"
                    }

                    // Tag [OK] o [ERR]
                    Text {
                        text: model.isError ? "[ERR]" : "[OK]"
                        color: model.isError ? "#F85149" : "#3FB950"
                        font.bold: true
                        font.pixelSize: 11
                        font.family: (Qt.platform.os === "android") ? "monospace" : "JetBrains Mono, Consolas, Courier New"
                    }

                    // Texto del log
                    Text {
                        width: parent.width - 240
                        text: model.text
                        color: model.isError ? "#FFA39E" : "#C9D1D9"
                        font.pixelSize: 11
                        font.family: (Qt.platform.os === "android") ? "monospace" : "JetBrains Mono, Consolas, Courier New"
                        elide: Text.ElideRight
                    }
                }

                // Fila final con cursor parpadeante si no hay logs o al final
                footer: Row {
                    spacing: 6
                    height: 16
                    width: logView.width

                    Text {
                        text: "nova@dsp:~$ "
                        color: "#58A6FF"
                        font.bold: true
                        font.pixelSize: 11
                        font.family: (Qt.platform.os === "android") ? "monospace" : "JetBrains Mono, Consolas, Courier New"
                    }

                    // Cursor de bloque de terminal parpadeante
                    Rectangle {
                        width: 7; height: 12
                        color: "#7EE787"
                        anchors.verticalCenter: parent.verticalCenter

                        SequentialAnimation on opacity {
                            loops: Animation.Infinite
                            running: true
                            NumberAnimation { to: 1.0; duration: 450 }
                            NumberAnimation { to: 0.0; duration: 450 }
                        }
                    }
                }
            }
        }
    }
}
