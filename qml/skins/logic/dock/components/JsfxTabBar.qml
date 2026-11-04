import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

/**
 * Barra de pestañas y herramientas superior del editor JSFX.
 * Contiene la pestaña activa, el botón de añadir plantilla, y los botones
 * de acción: Compile, Save, Search y Settings.
 */
Rectangle {
    id: root

    property string fileName: "simple_delay.jsfx"
    property bool isModified: false
    property bool searchActive: false
    property bool sidebarOpen: true

    signal compileClicked()
    signal saveClicked()
    signal searchClicked()
    signal closeClicked()
    signal templateSelected(string name)
    signal toggleSidebarClicked()
    signal formatClicked()
    signal revertClicked()

    Layout.fillWidth: true
    Layout.preferredHeight: (Qt.platform.os === "android") ? 38 : 34
    color: "#21232A"
    border.color: "#15161A"
    border.width: 1

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 10
        spacing: 8

        // Título de la sección
        Text {
            text: "JSFX Editor"
            color: "#9AA0A6"
            font.pixelSize: 11
            font.bold: true
            font.family: "Segoe UI, Inter, Sans-serif"
        }

        Item { width: 4 }

        // Pestaña activa del archivo
        Rectangle {
            Layout.preferredHeight: 28
            Layout.preferredWidth: tabRow.implicitWidth + 24
            Layout.alignment: Qt.AlignBottom
            radius: 3
            color: "#181A20"
            border.color: "#2D3139"
            border.width: 1

            // Conexión visual con el fondo del editor
            Rectangle {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                height: 2
                color: "#181A20"
            }

            RowLayout {
                id: tabRow
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 8
                spacing: 8

                Text {
                    text: root.fileName + (root.isModified ? " *" : "")
                    color: "#E8EAED"
                    font.pixelSize: 11
                    font.family: "Monospace, Consolas, Courier New"
                }

                // Botón cerrar pestaña (icono vectorial de cruz)
                Item {
                    width: 12; height: 12
                    Rectangle {
                        anchors.centerIn: parent
                        width: 9; height: 1.4
                        color: closeTabMouse.containsMouse ? "#FFFFFF" : "#7A8294"
                        rotation: 45
                    }
                    Rectangle {
                        anchors.centerIn: parent
                        width: 9; height: 1.4
                        color: closeTabMouse.containsMouse ? "#FFFFFF" : "#7A8294"
                        rotation: -45
                    }

                    MouseArea {
                        id: closeTabMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.closeClicked()
                    }
                }
            }
        }

        // Botón nueva pestaña / plantilla (+)
        Rectangle {
            Layout.preferredWidth: 22
            Layout.preferredHeight: 22
            radius: 3
            color: addTabMouse.containsMouse ? "#2D3139" : "transparent"

            Text {
                anchors.centerIn: parent
                text: "+"
                color: "#9AA0A6"
                font.pixelSize: 14
                font.bold: true
            }

            MouseArea {
                id: addTabMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: templateMenu.open()
            }

            Menu {
                id: templateMenu
                y: parent.height + 4

                MenuItem { text: "simple_delay.jsfx"; onTriggered: root.templateSelected("simple_delay.jsfx") }
                MenuItem { text: "multi_tap.jsfx";    onTriggered: root.templateSelected("multi_tap.jsfx") }
                MenuItem { text: "chorus.jsfx";       onTriggered: root.templateSelected("chorus.jsfx") }
                MenuItem { text: "distortion.jsfx";   onTriggered: root.templateSelected("distortion.jsfx") }
                MenuItem { text: "lfo_filter.jsfx";   onTriggered: root.templateSelected("lfo_filter.jsfx") }
                MenuItem { text: "reverb.jsfx";       onTriggered: root.templateSelected("reverb.jsfx") }
                MenuItem { text: "phaser_stereo.jsfx";onTriggered: root.templateSelected("phaser_stereo.jsfx") }
                MenuItem { text: "utils.jsh";         onTriggered: root.templateSelected("utils.jsh") }
            }
        }

        Item { Layout.fillWidth: true }

        // Botón ▶ Compile
        Rectangle {
            Layout.preferredHeight: (Qt.platform.os === "android") ? 28 : 24
            Layout.preferredWidth: compileBtnRow.implicitWidth + 20
            radius: 3
            color: compileMouse.containsMouse ? "#323846" : "#252832"
            border.color: "#343A48"
            border.width: 1

            Row {
                id: compileBtnRow
                anchors.centerIn: parent
                spacing: 6

                Canvas {
                    width: 8; height: 9
                    anchors.verticalCenter: parent.verticalCenter
                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.clearRect(0, 0, width, height)
                        ctx.fillStyle = "#E8EAED"
                        ctx.beginPath()
                        ctx.moveTo(0, 0)
                        ctx.lineTo(width, height / 2)
                        ctx.lineTo(0, height)
                        ctx.closePath()
                        ctx.fill()
                    }
                }

                Text { text: "Compile"; color: "#E8EAED"; font.pixelSize: 11; font.family: "Segoe UI, Inter, Sans-serif" }
            }

            MouseArea {
                id: compileMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.compileClicked()
            }
        }

        // Botón 💾 Save
        Rectangle {
            Layout.preferredHeight: (Qt.platform.os === "android") ? 28 : 24
            Layout.preferredWidth: saveBtnRow.implicitWidth + 20
            radius: 3
            color: saveMouse.containsMouse ? "#323846" : "#252832"
            border.color: "#343A48"
            border.width: 1

            Row {
                id: saveBtnRow
                anchors.centerIn: parent
                spacing: 6

                Item {
                    width: 11; height: 11
                    anchors.verticalCenter: parent.verticalCenter

                    Rectangle {
                        anchors.fill: parent
                        color: "transparent"
                        border.color: "#E8EAED"
                        border.width: 1.2
                        radius: 1
                    }
                    Rectangle {
                        anchors.top: parent.top
                        anchors.topMargin: 1
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 5; height: 3
                        color: "#E8EAED"
                    }
                    Rectangle {
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 1
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 5; height: 2.5
                        color: "#252832"
                    }
                }

                Text { text: "Save"; color: "#E8EAED"; font.pixelSize: 11; font.family: "Segoe UI, Inter, Sans-serif" }
            }

            MouseArea {
                id: saveMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.saveClicked()
            }
        }

        // Botón 🔍 Search
        Rectangle {
            Layout.preferredHeight: (Qt.platform.os === "android") ? 28 : 24
            Layout.preferredWidth: searchBtnRow.implicitWidth + 20
            radius: 3
            color: searchMouse.containsMouse || root.searchActive ? "#323846" : "#252832"
            border.color: root.searchActive ? "#3B82F6" : "#343A48"
            border.width: 1

            Row {
                id: searchBtnRow
                anchors.centerIn: parent
                spacing: 6

                Item {
                    width: 11; height: 11
                    anchors.verticalCenter: parent.verticalCenter

                    Rectangle {
                        x: 1; y: 1; width: 6.5; height: 6.5
                        radius: 3.25
                        color: "transparent"
                        border.color: "#E8EAED"
                        border.width: 1.3
                    }
                    Rectangle {
                        x: 6; y: 6; width: 4.5; height: 1.4
                        color: "#E8EAED"
                        rotation: 45
                        transformOrigin: Item.TopLeft
                    }
                }

                Text { text: "Search"; color: "#E8EAED"; font.pixelSize: 11; font.family: "Segoe UI, Inter, Sans-serif" }
            }

            MouseArea {
                id: searchMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.searchClicked()
            }
        }

        // Botón ⚙ Settings
        Rectangle {
            Layout.preferredHeight: (Qt.platform.os === "android") ? 28 : 24
            Layout.preferredWidth: (Qt.platform.os === "android") ? 30 : 26
            radius: 3
            color: settingsMouse.containsMouse ? "#323846" : "transparent"

            Canvas {
                width: 14; height: 14
                anchors.centerIn: parent
                onPaint: {
                    var ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    ctx.strokeStyle = "#9AA0A6"
                    ctx.fillStyle = "#9AA0A6"
                    ctx.lineWidth = 1.3

                    var cx = width / 2
                    var cy = height / 2
                    var r = 3.5

                    for (var i = 0; i < 8; ++i) {
                        var ang = i * Math.PI / 4
                        var x1 = cx + Math.cos(ang) * 3.5
                        var y1 = cy + Math.sin(ang) * 3.5
                        var x2 = cx + Math.cos(ang) * 5.8
                        var y2 = cy + Math.sin(ang) * 5.8
                        ctx.beginPath()
                        ctx.moveTo(x1, y1)
                        ctx.lineTo(x2, y2)
                        ctx.stroke()
                    }

                    ctx.beginPath()
                    ctx.arc(cx, cy, r, 0, Math.PI * 2)
                    ctx.stroke()
                }
            }

            MouseArea {
                id: settingsMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: settingsMenu.open()
            }

            Menu {
                id: settingsMenu
                y: parent.height + 4

                MenuItem {
                    text: (root.sidebarOpen ? "Ocultar Explorador" : "Mostrar Explorador") + " (Ctrl+B)"
                    onTriggered: root.toggleSidebarClicked()
                }
                MenuItem {
                    text: "Formatear Script"
                    onTriggered: root.formatClicked()
                }
                MenuItem {
                    text: "Revertir Cambios"
                    onTriggered: root.revertClicked()
                }
                MenuSeparator {}
                MenuItem {
                    text: "Cerrar Editor"
                    onTriggered: root.closeClicked()
                }
            }
        }
    }
}
