import QtQuick
import QtQuick.Layouts

/**
 * Barra de estado y consola inferior del editor JSFX.
 * Muestra el estado de compilación a la izquierda y la posición del cursor,
 * espaciado y tipo de archivo estilo VS Code a la derecha.
 */
Rectangle {
    id: root

    property string statusMessage: "Compiled successfully"
    property bool hasError: false
    property string currentSection: "@init"
    property int currentLine: 1
    property int currentCol: 1

    signal pasteClicked()

    Layout.fillWidth: true
    Layout.preferredHeight: (Qt.platform.os === "android") ? 46 : 40
    color: "#16171B"
    border.color: "#202229"
    border.width: 1

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 14
        anchors.rightMargin: 14
        spacing: 12

        // Consola de estado a la izquierda
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            Text {
                text: "Console"
                color: "#8F94A0"
                font.pixelSize: 11
                font.bold: true
                font.family: "Segoe UI, Inter, Sans-serif"
            }

            Row {
                spacing: 6
                Rectangle {
                    width: 6; height: 6; radius: 3
                    anchors.verticalCenter: parent.verticalCenter
                    color: root.hasError ? "#FF4D4F" : "#52C41A"
                }
                Text {
                    text: root.statusMessage
                    color: root.hasError ? "#FF4D4F" : "#52C41A"
                    font.pixelSize: 11
                    font.family: "Monospace, Consolas, 'Courier New'"
                    elide: Text.ElideRight
                }
            }
        }

        // Botón Pegar texto de portapapeles a la derecha
        Rectangle {
            Layout.alignment: Qt.AlignVCenter
            Layout.preferredHeight: (Qt.platform.os === "android") ? 32 : 26
            Layout.preferredWidth: pasteRow.implicitWidth + 20
            color: pasteMouse.containsMouse ? "#2E323D" : "#20232B"
            border.color: pasteMouse.containsMouse ? "#4D5366" : "#2B2E38"
            border.width: 1
            radius: 3

            RowLayout {
                id: pasteRow
                anchors.centerIn: parent
                spacing: 7

                // Icono vectorial de portapapeles
                Canvas {
                    Layout.preferredWidth: 13
                    Layout.preferredHeight: 14
                    Layout.alignment: Qt.AlignVCenter
                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.clearRect(0, 0, 13, 14)
                        ctx.strokeStyle = pasteMouse.containsMouse ? "#FFFFFF" : "#AEAFAD"
                        ctx.lineWidth = 1.2
                        // Tabla de portapapeles
                        ctx.strokeRect(1.5, 2.5, 10, 10)
                        // Presilla / clip superior
                        ctx.fillStyle = "#D4B16A"
                        ctx.fillRect(4.5, 0.5, 4, 3)
                        // Líneas de texto simuladas
                        ctx.beginPath()
                        ctx.moveTo(3.5, 5.5)
                        ctx.lineTo(9.5, 5.5)
                        ctx.moveTo(3.5, 8.5)
                        ctx.lineTo(9.5, 8.5)
                        ctx.stroke()
                    }
                }

                Text {
                    text: "Pegar portapapeles"
                    color: pasteMouse.containsMouse ? "#FFFFFF" : "#D4D4D4"
                    font.pixelSize: (Qt.platform.os === "android") ? 12 : 11
                    font.bold: true
                    font.family: "Segoe UI, Inter, Sans-serif"
                    Layout.alignment: Qt.AlignVCenter
                }
            }

            MouseArea {
                id: pasteMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                ToolTip.visible: containsMouse
                ToolTip.text: "Pegar texto del portapapeles en el editor"
                ToolTip.delay: 300
                onClicked: {
                    root.pasteClicked()
                }
            }
        }
    }
}
