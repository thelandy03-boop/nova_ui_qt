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
    property string activeView: "code" // "code" o "ui"
    property bool isMaximized: false

    signal compileClicked()
    signal saveClicked()
    signal pasteClicked()
    signal searchClicked()
    signal closeClicked()
    signal templateSelected(string name)
    signal toggleSidebarClicked()
    signal formatClicked()
    signal revertClicked()
    signal viewModeChanged(string mode)
    signal toggleMaximizeClicked()
    signal reapackClicked()

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

                MenuItem { text: "ace_eq.jsfx (ACE EQ)";                   onTriggered: root.templateSelected("ace_eq.jsfx") }
                MenuItem { text: "ace_compressor.jsfx (ACE Compressor)";   onTriggered: root.templateSelected("ace_compressor.jsfx") }
                MenuItem { text: "noise_gate.jsfx (Fast Noise Gate)";      onTriggered: root.templateSelected("noise_gate.jsfx") }
                MenuItem { text: "ace_delay.jsfx (ACE Delay)";             onTriggered: root.templateSelected("ace_delay.jsfx") }
                MenuItem { text: "ace_reverb.jsfx (ACE Reverb)";           onTriggered: root.templateSelected("ace_reverb.jsfx") }
                MenuItem { text: "tube_saturation.jsfx (Tube Saturator)";  onTriggered: root.templateSelected("tube_saturation.jsfx") }
                MenuItem { text: "stereo_widener.jsfx (Stereo Widener)";   onTriggered: root.templateSelected("stereo_widener.jsfx") }
                MenuItem { text: "vocal_air.jsfx (Vocal Air Exciter)";     onTriggered: root.templateSelected("vocal_air.jsfx") }
            }
        }

        Item { Layout.fillWidth: true }

        // Botones de Modo de Vista: [ 🎛️ Controles ] y [ </> Código ]
        Row {
            spacing: 4
            Layout.alignment: Qt.AlignVCenter

            // Botón [ 🎛️ Vista previa ]
            Rectangle {
                width: 104
                height: (Qt.platform.os === "android") ? 28 : 24
                radius: 3
                color: root.activeView === "ui" ? "#2563EB" : (uiMouse.containsMouse ? "#323846" : "#252832")
                border.color: root.activeView === "ui" ? "#3B82F6" : "#343A48"
                border.width: 1

                Row {
                    anchors.centerIn: parent
                    spacing: 6

                    Canvas {
                        id: previewEyeCanvas
                        width: 13; height: 9
                        anchors.verticalCenter: parent.verticalCenter
                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.clearRect(0, 0, width, height)
                            ctx.strokeStyle = (root.activeView === "ui") ? "#FFFFFF" : "#CBD5E1"
                            ctx.fillStyle = (root.activeView === "ui") ? "#FFFFFF" : "#CBD5E1"
                            ctx.lineWidth = 1.2
                            // Contorno del ojo
                            ctx.beginPath()
                            ctx.moveTo(0.5, height / 2)
                            ctx.quadraticCurveTo(width / 2, -1, width - 0.5, height / 2)
                            ctx.quadraticCurveTo(width / 2, height + 1, 0.5, height / 2)
                            ctx.stroke()
                            // Pupila central
                            ctx.beginPath()
                            ctx.arc(width / 2, height / 2, 2, 0, Math.PI * 2)
                            ctx.fill()
                        }
                        Connections {
                            target: root
                            function onActiveViewChanged() { previewEyeCanvas.requestPaint() }
                        }
                    }

                    Text {
                        text: "Vista previa"
                        color: root.activeView === "ui" ? "#FFFFFF" : "#E8EAED"
                        font.pixelSize: 11
                        font.bold: true
                        font.family: "Segoe UI, Inter, Sans-serif"
                    }
                }

                MouseArea {
                    id: uiMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.viewModeChanged("ui")
                }
            }

            // Botón [ </> Código ]
            Rectangle {
                width: 80
                height: (Qt.platform.os === "android") ? 28 : 24
                radius: 3
                color: root.activeView === "code" ? "#2563EB" : (codeMouse.containsMouse ? "#323846" : "#252832")
                border.color: root.activeView === "code" ? "#3B82F6" : "#343A48"
                border.width: 1

                Row {
                    anchors.centerIn: parent
                    spacing: 5
                    Text {
                        text: "</>"
                        color: root.activeView === "code" ? "#FFFFFF" : "#38BDF8"
                        font.pixelSize: 10
                        font.bold: true
                        font.family: "Monospace"
                    }
                    Text {
                        text: "Código"
                        color: root.activeView === "code" ? "#FFFFFF" : "#E8EAED"
                        font.pixelSize: 11
                        font.bold: true
                        font.family: "Segoe UI, Inter, Sans-serif"
                    }
                }

                MouseArea {
                    id: codeMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.viewModeChanged("code")
                }
            }
        }

        Item { width: 6 }

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

        // Botón ⌨ Ocultar Teclado (Específico de Android)
        Rectangle {
            visible: (Qt.platform.os === "android")
            Layout.preferredHeight: 28
            Layout.preferredWidth: hideKbRow.implicitWidth + 18
            radius: 3
            color: hideKbMouse.containsMouse ? "#323846" : "#252832"
            border.color: "#3B82F6"
            border.width: 1

            Row {
                id: hideKbRow
                anchors.centerIn: parent
                spacing: 5
                Text { text: "⌨▾"; color: "#60A5FA"; font.pixelSize: 12; font.bold: true }
                Text { text: "Teclado"; color: "#E8EAED"; font.pixelSize: 11; font.family: "Segoe UI, Inter, Sans-serif" }
            }

            MouseArea {
                id: hideKbMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: Qt.inputMethod.hide()
            }
        }

        // Botón 📋 Pegar
        Rectangle {
            Layout.preferredHeight: (Qt.platform.os === "android") ? 28 : 24
            Layout.preferredWidth: pasteBtnRow.implicitWidth + 20
            radius: 3
            color: pasteMouse.containsMouse ? "#323846" : "#252832"
            border.color: "#343A48"
            border.width: 1

            Row {
                id: pasteBtnRow
                anchors.centerIn: parent
                spacing: 6

                Canvas {
                    width: 11; height: 12
                    anchors.verticalCenter: parent.verticalCenter
                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.clearRect(0, 0, width, height)
                        ctx.strokeStyle = "#E8EAED"
                        ctx.lineWidth = 1.1
                        // Portapapeles
                        ctx.strokeRect(1, 2, 9, 9)
                        // Clip dorado
                        ctx.fillStyle = "#D4B16A"
                        ctx.fillRect(3.5, 0.5, 4, 2.5)
                        // Líneas de texto
                        ctx.beginPath()
                        ctx.moveTo(3, 5)
                        ctx.lineTo(8, 5)
                        ctx.moveTo(3, 7.5)
                        ctx.lineTo(8, 7.5)
                        ctx.stroke()
                    }
                }

                Text { text: "Pegar"; color: "#E8EAED"; font.pixelSize: 11; font.family: "Segoe UI, Inter, Sans-serif" }
            }

            MouseArea {
                id: pasteMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                ToolTip.visible: containsMouse
                ToolTip.text: "Pegar texto del portapapeles (Ctrl+V)"
                ToolTip.delay: 300
                onClicked: root.pasteClicked()
            }
        }

        // Botón 📦 ReaPack Packages
        Rectangle {
            Layout.preferredHeight: (Qt.platform.os === "android") ? 28 : 24
            Layout.preferredWidth: reapackBtnRow.implicitWidth + 18
            radius: 3
            color: reapackTabMouse.containsMouse ? "#323846" : "#252832"
            border.color: "#383E4F"
            border.width: 1

            Row {
                id: reapackBtnRow
                anchors.centerIn: parent
                spacing: 5
                Text { text: "📦"; font.pixelSize: 11 }
                Text { text: "ReaPack"; color: "#E8EAED"; font.pixelSize: 11; font.family: "Segoe UI, Inter, Sans-serif" }
            }

            MouseArea {
                id: reapackTabMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.reapackClicked()
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

        // Botón ⛶ Maximizar / Restaurar (útil especialmente en Android)
        Rectangle {
            Layout.preferredHeight: (Qt.platform.os === "android") ? 28 : 24
            Layout.preferredWidth: (Qt.platform.os === "android") ? 30 : 26
            radius: 3
            color: maxMouse.containsMouse ? "#323846" : "transparent"

            Canvas {
                id: maxCanvas
                width: 12; height: 12
                anchors.centerIn: parent
                onPaint: {
                    var ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    ctx.strokeStyle = "#9AA0A6"
                    ctx.lineWidth = 1.3
                    if (root.isMaximized) {
                        // Dos recuadros solapados (Restaurar)
                        ctx.strokeRect(3, 1, 8, 7)
                        ctx.strokeRect(1, 4, 8, 7)
                    } else {
                        // Un recuadro único (Maximizar)
                        ctx.strokeRect(1, 1, 10, 10)
                        ctx.strokeRect(1, 1, 10, 3) // Barra superior del recuadro
                    }
                }
                Connections {
                    target: root
                    function onIsMaximizedChanged() { maxCanvas.requestPaint() }
                }
            }

            MouseArea {
                id: maxMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.toggleMaximizeClicked()
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
