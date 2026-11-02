import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root

    implicitWidth: (Qt.platform.os === "android") ? 145 : 130
    implicitHeight: 70
    color: maLogo.pressed ? "#22252D" : (maLogo.containsMouse ? "#1A1D23" : "#15171C")

    signal homeRequested()

    Rectangle {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 1
        color: "#0E0F13"
    }

    Row {
        anchors.centerIn: parent
        spacing: 6

        // Indicador visual de regreso al menú de proyectos en móvil
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: "‹"
            color: "#4A90E2"
            font.pixelSize: 26
            font.bold: true
            visible: Qt.platform.os === "android"
        }

        Column {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "NOVA"
                color: "#FFFFFF"
                font.pixelSize: 21
                font.weight: Font.DemiBold
                font.letterSpacing: 2.5
                font.family: "sans-serif"
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "STUDIO"
                color: "#9A9EA8"
                font.pixelSize: 10
                font.weight: Font.Normal
                font.letterSpacing: 3.5
                font.family: "sans-serif"
            }
        }
    }

    MouseArea {
        id: maLogo
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            if (typeof AudioEngine !== "undefined" && AudioEngine.hapticFeedback) {
                AudioEngine.hapticFeedback();
            }
            root.homeRequested();
        }
    }
}