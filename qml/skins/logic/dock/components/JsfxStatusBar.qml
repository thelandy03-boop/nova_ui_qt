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

    Layout.fillWidth: true
    Layout.preferredHeight: 46
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

        // Indicadores de estado estilo VS Code a la derecha
        RowLayout {
            spacing: 14
            Layout.alignment: Qt.AlignVCenter

            Text {
                text: root.currentSection
                color: "#D4B16A"
                font.pixelSize: 11
                font.bold: true
                font.family: "Monospace, Consolas, Courier New"
            }

            Rectangle { width: 1; height: 14; color: "#2B2E38" }

            Text {
                text: "Ln " + root.currentLine + ", Col " + root.currentCol
                color: "#8F94A0"
                font.pixelSize: 11
                font.family: "Segoe UI, Inter, Sans-serif"
            }

            Rectangle { width: 1; height: 14; color: "#2B2E38" }

            Text {
                text: "Spaces: 4"
                color: "#8F94A0"
                font.pixelSize: 11
                font.family: "Segoe UI, Inter, Sans-serif"
            }

            Rectangle { width: 1; height: 14; color: "#2B2E38" }

            Text {
                text: "UTF-8"
                color: "#8F94A0"
                font.pixelSize: 11
                font.family: "Segoe UI, Inter, Sans-serif"
            }

            Rectangle { width: 1; height: 14; color: "#2B2E38" }

            Text {
                text: "EEL2 (JSFX)"
                color: "#8F94A0"
                font.pixelSize: 11
                font.family: "Segoe UI, Inter, Sans-serif"
            }
        }
    }
}
