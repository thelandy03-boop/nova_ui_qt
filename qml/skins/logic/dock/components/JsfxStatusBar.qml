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
    Layout.preferredHeight: (Qt.platform.os === "android") ? 36 : 28
    color: "#16171B"
    border.color: "#202229"
    border.width: 1

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 14
        anchors.rightMargin: 14
        spacing: 10

        // Consola de estado
        Row {
            Layout.fillWidth: true
            spacing: 8
            anchors.verticalCenter: parent.verticalCenter

            Text {
                text: "Console:"
                color: "#8F94A0"
                font.pixelSize: 11
                font.bold: true
                font.family: "Segoe UI, Inter, Sans-serif"
                anchors.verticalCenter: parent.verticalCenter
            }

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
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }
}
