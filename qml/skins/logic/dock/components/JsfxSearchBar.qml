import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

/**
 * Barra de búsqueda integrada en el editor JSFX.
 * Permite buscar texto hacia adelante y hacia atrás con botones vectoriales.
 */
Rectangle {
    id: root

    property alias searchText: searchInput.text

    signal searchChanged(string text)
    signal prevClicked()
    signal nextClicked()
    signal closeClicked()

    function forceFocus() {
        searchInput.forceActiveFocus()
    }

    Layout.fillWidth: true
    Layout.preferredHeight: 32
    clip: true
    color: "#1E2027"
    border.color: "#2D3139"
    border.width: 1

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        spacing: 8

        // Icono de búsqueda
        Item {
            width: 14; height: 14
            Rectangle {
                x: 1; y: 1; width: 7.5; height: 7.5
                radius: 3.75
                color: "transparent"
                border.color: "#8F94A0"
                border.width: 1.3
            }
            Rectangle {
                x: 6.5; y: 6.5; width: 4.5; height: 1.4
                color: "#8F94A0"
                rotation: 45
                transformOrigin: Item.TopLeft
            }
        }

        TextField {
            id: searchInput
            Layout.fillWidth: true
            placeholderText: "Buscar en el código..."
            color: "#FFFFFF"
            font.pixelSize: 11
            background: Rectangle {
                color: "#282B34"
                radius: 3
                border.color: searchInput.activeFocus ? "#3B82F6" : "#343A48"
            }
            onTextChanged: root.searchChanged(text)
        }

        // Botón Anterior
        Rectangle {
            width: 20; height: 20; radius: 3
            color: searchPrevMouse.containsMouse ? "#3D4250" : "transparent"
            Canvas {
                anchors.centerIn: parent
                width: 8; height: 8
                onPaint: {
                    var ctx = getContext("2d")
                    ctx.clearRect(0, 0, 8, 8)
                    ctx.strokeStyle = "#FFFFFF"
                    ctx.lineWidth = 1.3
                    ctx.beginPath()
                    ctx.moveTo(1.5, 5.5)
                    ctx.lineTo(4, 2.5)
                    ctx.lineTo(6.5, 5.5)
                    ctx.stroke()
                }
            }
            MouseArea {
                id: searchPrevMouse; anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                onClicked: root.prevClicked()
            }
        }

        // Botón Siguiente
        Rectangle {
            width: 20; height: 20; radius: 3
            color: searchNextMouse.containsMouse ? "#3D4250" : "transparent"
            Canvas {
                anchors.centerIn: parent
                width: 8; height: 8
                onPaint: {
                    var ctx = getContext("2d")
                    ctx.clearRect(0, 0, 8, 8)
                    ctx.strokeStyle = "#FFFFFF"
                    ctx.lineWidth = 1.3
                    ctx.beginPath()
                    ctx.moveTo(1.5, 2.5)
                    ctx.lineTo(4, 5.5)
                    ctx.lineTo(6.5, 2.5)
                    ctx.stroke()
                }
            }
            MouseArea {
                id: searchNextMouse; anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                onClicked: root.nextClicked()
            }
        }

        // Botón Cerrar
        Rectangle {
            width: 20; height: 20; radius: 3
            color: searchCloseMouse.containsMouse ? "#3D4250" : "transparent"
            Item {
                anchors.centerIn: parent
                width: 9; height: 9
                Rectangle { anchors.centerIn: parent; width: 8; height: 1.3; color: "#8F94A0"; rotation: 45 }
                Rectangle { anchors.centerIn: parent; width: 8; height: 1.3; color: "#8F94A0"; rotation: -45 }
            }
            MouseArea {
                id: searchCloseMouse; anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                onClicked: root.closeClicked()
            }
        }
    }
}
