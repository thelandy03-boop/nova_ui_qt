import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

/**
 * Editor JSFX Integrado (Dock) para NOVA Studio.
 * Se acopla directamente bajo la Timeline en la columna derecha del Workspace,
 * con Activity Bar vectorial, árbol de proyectos/archivos JSFX, editor monoespaciado
 * con alineación matemática perfecta entre números de línea y código,
 * resaltado de sintaxis sin corrupción de etiquetas HTML y consola de compilación.
 */
Rectangle {
    id: root

    property int trackIndex: -1
    property int pluginIndex: -1
    property string pluginName: "simple_delay.jsfx"
    property bool isNewPlugin: false
    property bool isModified: false

    property string statusMessage: "Compiled successfully"
    property bool hasError: false

    // Estado de cursor y posición estilo VS Code
    property int currentLine: 1
    property int currentCol: 1
    property string currentSection: "@init"

    // Estado del explorador lateral izquierdo
    property bool showSidebar: true
    property int activeActivityTab: 0 // 0: Explorer, 1: Search, 2: Git, 3: Run, 4: Extensions
    property string selectedFile: "simple_delay.jsfx"

    property bool searchVisible: false

    signal closeRequested()
    signal scriptApplied()

    color: "#181A20"
    border.color: "#141518"
    border.width: 1

    // Atajos locales cuando el dock tiene foco
    Shortcut {
        sequence: "Ctrl+S"
        onActivated: root.compileAndApply()
    }
    Shortcut {
        sequence: "F5"
        onActivated: root.compileAndApply()
    }
    Shortcut {
        sequence: "Ctrl+B"
        onActivated: root.showSidebar = !root.showSidebar
    }
    Shortcut {
        sequence: "Ctrl+F"
        onActivated: {
            root.searchVisible = !root.searchVisible
            if (root.searchVisible) searchInput.forceActiveFocus()
        }
    }
    Shortcut {
        sequence: "Escape"
        onActivated: {
            if (root.searchVisible) root.searchVisible = false
            else root.closeRequested()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // =====================================================================
        // 1. BARRA DE PESTAÑAS Y HERRAMIENTAS (TAB BAR & TOOLBAR)
        // =====================================================================
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 34
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
                    color: "#181A20" // Se funde con el fondo del editor
                    border.color: "#2D3139"
                    border.width: 1

                    // Cubrir el borde inferior para conectar visualmente con el editor
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
                            text: root.pluginName + (root.isModified ? " *" : "")
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
                                onClicked: root.closeRequested()
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

                        MenuItem {
                            text: "simple_delay.jsfx"
                            onTriggered: root.loadFileByName("simple_delay.jsfx")
                        }
                        MenuItem {
                            text: "chorus.jsfx"
                            onTriggered: root.loadFileByName("chorus.jsfx")
                        }
                        MenuItem {
                            text: "distortion.jsfx"
                            onTriggered: root.loadFileByName("distortion.jsfx")
                        }
                        MenuItem {
                            text: "lfo_filter.jsfx"
                            onTriggered: root.loadFileByName("lfo_filter.jsfx")
                        }
                        MenuItem {
                            text: "reverb.jsfx"
                            onTriggered: root.loadFileByName("reverb.jsfx")
                        }
                    }
                }

                Item { Layout.fillWidth: true }

                // Botón ▶ Compile (con triángulo vectorial puro)
                Rectangle {
                    Layout.preferredHeight: 24
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
                        onClicked: root.compileAndApply()
                    }
                }

                // Botón 💾 Save (con icono vectorial de disquete)
                Rectangle {
                    Layout.preferredHeight: 24
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
                        onClicked: root.saveAndInsert()
                    }
                }

                // Botón 🔍 Search (con icono vectorial de lupa)
                Rectangle {
                    Layout.preferredHeight: 24
                    Layout.preferredWidth: searchBtnRow.implicitWidth + 20
                    radius: 3
                    color: searchMouse.containsMouse || root.searchVisible ? "#323846" : "#252832"
                    border.color: root.searchVisible ? "#3B82F6" : "#343A48"
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
                        onClicked: {
                            root.searchVisible = !root.searchVisible
                            if (root.searchVisible) searchInput.forceActiveFocus()
                        }
                    }
                }

                // Botón ⚙ Settings (con engranaje vectorial Canvas)
                Rectangle {
                    Layout.preferredHeight: 24
                    Layout.preferredWidth: 26
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
                            text: (root.showSidebar ? "Ocultar Explorador" : "Mostrar Explorador") + " (Ctrl+B)"
                            onTriggered: root.showSidebar = !root.showSidebar
                        }
                        MenuItem {
                            text: "Formatear Script"
                            onTriggered: root.formatScript()
                        }
                        MenuItem {
                            text: "Revertir Cambios"
                            onTriggered: root.loadFileByName(root.selectedFile)
                        }
                        MenuSeparator {}
                        MenuItem {
                            text: "Cerrar Editor"
                            onTriggered: root.closeRequested()
                        }
                    }
                }
            }
        }

        // =====================================================================
        // 2. BARRA DE BÚSQUEDA INTEGRADA (DESPLEGABLE)
        // =====================================================================
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: root.searchVisible ? 32 : 0
            visible: root.searchVisible
            clip: true
            color: "#1E2027"
            border.color: "#2D3139"
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 8

                // Lupa vectorial en el input
                Item {
                    width: 12; height: 12
                    Rectangle {
                        x: 1; y: 1; width: 7; height: 7
                        radius: 3.5
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
                    onTextChanged: root.searchInCode(text)
                }

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
                        onClicked: root.findPrev()
                    }
                }

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
                        onClicked: root.findNext()
                    }
                }

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
                        onClicked: root.searchVisible = false
                    }
                }
            }
        }

        // =====================================================================
        // 3. CUERPO CENTRAL: ACTIVITY BAR + EXPLORER TREE + EDITOR DE CÓDIGO
        // =====================================================================
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // ── 3.1 ACTIVITY BAR LATERAL IZQUIERDA (VS CODE ACTIVITY BAR) ─────
            Rectangle {
                Layout.preferredWidth: 38
                Layout.fillHeight: true
                color: "#1E1F24"
                border.color: "#141518"
                border.width: 1

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 4
                    anchors.topMargin: 6
                    anchors.bottomMargin: 6

                    // Botón 1: Explorador de Archivos (Páginas superpuestas vectoriales)
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 38
                        color: root.activeActivityTab === 0 && root.showSidebar ? "#252832" : "transparent"

                        // Indicador de selección activa (línea blanca izquierda)
                        Rectangle {
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            width: 2
                            color: "#FFFFFF"
                            visible: root.activeActivityTab === 0 && root.showSidebar
                        }

                        Item {
                            width: 18; height: 18
                            anchors.centerIn: parent
                            opacity: (root.activeActivityTab === 0 && root.showSidebar) ? 1.0 : 0.6

                            Rectangle {
                                x: 1; y: 3; width: 12; height: 14
                                color: "transparent"
                                border.color: "#FFFFFF"
                                border.width: 1.4
                                radius: 1
                            }
                            Rectangle {
                                x: 4; y: 0; width: 12; height: 14
                                color: "#1E1F24"
                                border.color: "#FFFFFF"
                                border.width: 1.4
                                radius: 1
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (root.activeActivityTab === 0 && root.showSidebar) {
                                    root.showSidebar = false
                                } else {
                                    root.activeActivityTab = 0
                                    root.showSidebar = true
                                }
                            }
                        }
                    }

                    // Botón 2: Lupa / Búsqueda (vectorial puro)
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 38
                        color: root.activeActivityTab === 1 && root.showSidebar ? "#252832" : "transparent"

                        Rectangle {
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            width: 2
                            color: "#FFFFFF"
                            visible: root.activeActivityTab === 1 && root.showSidebar
                        }

                        Item {
                            width: 16; height: 16
                            anchors.centerIn: parent
                            opacity: (root.activeActivityTab === 1 && root.showSidebar) ? 1.0 : 0.6

                            Rectangle {
                                x: 1; y: 1; width: 9; height: 9
                                radius: 4.5
                                color: "transparent"
                                border.color: "#FFFFFF"
                                border.width: 1.4
                            }
                            Rectangle {
                                x: 8; y: 8; width: 6; height: 1.8
                                color: "#FFFFFF"
                                rotation: 45
                                transformOrigin: Item.TopLeft
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                root.activeActivityTab = 1
                                root.showSidebar = true
                                root.searchVisible = true
                                searchInput.forceActiveFocus()
                            }
                        }
                    }

                    // Botón 3: Source Control / Git (Canvas vectorial garantizado)
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 38
                        color: root.activeActivityTab === 2 && root.showSidebar ? "#252832" : "transparent"

                        Rectangle {
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            width: 2
                            color: "#FFFFFF"
                            visible: root.activeActivityTab === 2 && root.showSidebar
                        }

                        Canvas {
                            width: 16; height: 16
                            anchors.centerIn: parent
                            opacity: (root.activeActivityTab === 2 && root.showSidebar) ? 1.0 : 0.6
                            onPaint: {
                                var ctx = getContext("2d")
                                ctx.clearRect(0, 0, width, height)
                                ctx.strokeStyle = "#FFFFFF"
                                ctx.fillStyle = "#FFFFFF"
                                ctx.lineWidth = 1.4

                                ctx.beginPath()
                                ctx.moveTo(4, 5)
                                ctx.lineTo(4, 12)
                                ctx.stroke()

                                ctx.beginPath()
                                ctx.moveTo(4, 10)
                                ctx.bezierCurveTo(4, 7, 12, 7, 12, 5)
                                ctx.stroke()

                                ctx.beginPath()
                                ctx.arc(4, 12, 1.8, 0, Math.PI * 2)
                                ctx.fill()

                                ctx.beginPath()
                                ctx.arc(4, 4, 1.8, 0, Math.PI * 2)
                                ctx.fill()

                                ctx.beginPath()
                                ctx.arc(12, 4, 1.8, 0, Math.PI * 2)
                                ctx.fill()
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                root.activeActivityTab = 2
                                root.showSidebar = true
                            }
                        }
                    }

                    // Botón 4: Run / Debug (Canvas vectorial de play contorno)
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 38
                        color: root.activeActivityTab === 3 && root.showSidebar ? "#252832" : "transparent"

                        Rectangle {
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            width: 2
                            color: "#FFFFFF"
                            visible: root.activeActivityTab === 3 && root.showSidebar
                        }

                        Canvas {
                            width: 15; height: 15
                            anchors.centerIn: parent
                            opacity: (root.activeActivityTab === 3 && root.showSidebar) ? 1.0 : 0.6
                            onPaint: {
                                var ctx = getContext("2d")
                                ctx.clearRect(0, 0, width, height)
                                ctx.strokeStyle = "#FFFFFF"
                                ctx.lineWidth = 1.4
                                ctx.beginPath()
                                ctx.moveTo(3, 2)
                                ctx.lineTo(12, 7.5)
                                ctx.lineTo(3, 13)
                                ctx.closePath()
                                ctx.stroke()
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                root.activeActivityTab = 3
                                root.showSidebar = true
                            }
                        }
                    }

                    // Botón 5: Extensiones (4 cubos modulares de VS Code vectoriales)
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 38
                        color: root.activeActivityTab === 4 && root.showSidebar ? "#252832" : "transparent"

                        Rectangle {
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            width: 2
                            color: "#FFFFFF"
                            visible: root.activeActivityTab === 4 && root.showSidebar
                        }

                        Item {
                            width: 15; height: 15
                            anchors.centerIn: parent
                            opacity: (root.activeActivityTab === 4 && root.showSidebar) ? 1.0 : 0.6

                            Rectangle { x: 1; y: 1; width: 5.5; height: 5.5; color: "transparent"; border.color: "#FFFFFF"; border.width: 1.3; radius: 1 }
                            Rectangle { x: 1; y: 8.5; width: 5.5; height: 5.5; color: "transparent"; border.color: "#FFFFFF"; border.width: 1.3; radius: 1 }
                            Rectangle { x: 8.5; y: 8.5; width: 5.5; height: 5.5; color: "transparent"; border.color: "#FFFFFF"; border.width: 1.3; radius: 1 }
                            Rectangle { x: 8.5; y: 0; width: 5.5; height: 5.5; color: "transparent"; border.color: "#FFFFFF"; border.width: 1.3; radius: 1 }
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                root.activeActivityTab = 4
                                root.showSidebar = true
                            }
                        }
                    }

                    Item { Layout.fillHeight: true }
                }
            }

            // ── 3.2 ÁRBOL DE PROYECTOS Y ARCHIVOS (SIDEBAR COLAPSABLE) ────────
            Rectangle {
                id: fileSidebar
                Layout.preferredWidth: root.showSidebar ? 180 : 0
                Layout.fillHeight: true
                visible: root.showSidebar
                clip: true
                color: "#181A20"
                border.color: "#141518"
                border.width: 1

                ScrollView {
                    anchors.fill: parent
                    clip: true
                    contentWidth: availableWidth

                    ColumnLayout {
                        width: parent.width
                        spacing: 2
                        anchors.topMargin: 6
                        anchors.bottomMargin: 6

                        // Cabecera de sección: ∨ PROJECTS
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 22
                            color: "transparent"

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 8
                                spacing: 6

                                Canvas {
                                    width: 8; height: 8
                                    Layout.alignment: Qt.AlignVCenter
                                    onPaint: {
                                        var ctx = getContext("2d")
                                        ctx.clearRect(0, 0, 8, 8)
                                        ctx.strokeStyle = "#8F94A0"
                                        ctx.lineWidth = 1.3
                                        ctx.beginPath()
                                        ctx.moveTo(1.5, 2.5)
                                        ctx.lineTo(4, 5.5)
                                        ctx.lineTo(6.5, 2.5)
                                        ctx.stroke()
                                    }
                                }
                                Text {
                                    text: "PROJECTS"
                                    color: "#8F94A0"
                                    font.pixelSize: 11
                                    font.bold: true
                                    font.family: "Segoe UI, Inter, Sans-serif"
                                }
                            }
                        }

                        // Carpeta raíz: 📁 jsfx_effects (Carpeta vectorial dorada)
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 22
                            color: "transparent"

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 16
                                spacing: 6

                                Canvas {
                                    width: 8; height: 8
                                    Layout.alignment: Qt.AlignVCenter
                                    onPaint: {
                                        var ctx = getContext("2d")
                                        ctx.clearRect(0, 0, 8, 8)
                                        ctx.strokeStyle = "#8F94A0"
                                        ctx.lineWidth = 1.3
                                        ctx.beginPath()
                                        ctx.moveTo(1.5, 2.5)
                                        ctx.lineTo(4, 5.5)
                                        ctx.lineTo(6.5, 2.5)
                                        ctx.stroke()
                                    }
                                }

                                // Icono vectorial de carpeta dorada
                                Item {
                                    width: 14; height: 11
                                    anchors.verticalCenter: parent.verticalCenter
                                    Rectangle { x: 0; y: 0; width: 6; height: 3; color: "#D4B16A"; radius: 1 }
                                    Rectangle { x: 0; y: 2; width: 14; height: 9; color: "#E5C07B"; radius: 1.5 }
                                }

                                Text {
                                    text: "jsfx_effects"
                                    color: "#CCCCCC"
                                    font.pixelSize: 11
                                    font.family: "Segoe UI, Inter, Sans-serif"
                                }
                            }
                        }

                        // Lista de archivos JSFX (con icono vectorial garantizado)
                        Repeater {
                            model: [
                                { name: "chorus.jsfx",        cat: "Mod" },
                                { name: "distortion.jsfx",    cat: "Distortion" },
                                { name: "lfo_filter.jsfx",     cat: "Filter" },
                                { name: "simple_delay.jsfx",  cat: "Delay" },
                                { name: "multi_tap.jsfx",     cat: "Delay" },
                                { name: "reverb.jsfx",        cat: "Space" },
                                { name: "phaser_stereo.jsfx", cat: "Mod" },
                                { name: "utils.jsh",          cat: "Utility" }
                            ]

                            delegate: Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 22
                                color: root.selectedFile === modelData.name ? "#2A2D37" : (fileMouse.containsMouse ? "#20232B" : "transparent")

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 28
                                    anchors.rightMargin: 8
                                    spacing: 6

                                    // Icono vectorial de archivo JSFX con insignia
                                    Rectangle {
                                        width: 11; height: 13
                                        color: "#1E2026"
                                        border.color: root.selectedFile === modelData.name ? "#D4B16A" : "#8F94A0"
                                        border.width: 1
                                        radius: 1

                                        Text {
                                            anchors.centerIn: parent
                                            text: "JS"
                                            color: root.selectedFile === modelData.name ? "#D4B16A" : "#8F94A0"
                                            font.pixelSize: 6
                                            font.bold: true
                                        }
                                    }

                                    Text {
                                        text: modelData.name
                                        color: root.selectedFile === modelData.name ? "#FFFFFF" : "#CCCCCC"
                                        font.pixelSize: 11
                                        font.family: "Monospace, Consolas, Courier New"
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }
                                }

                                MouseArea {
                                    id: fileMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        root.loadFileByName(modelData.name)
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // ── 3.3 ÁREA DE CÓDIGO CON NÚMEROS DE LÍNEA Y EDITOR MONOESPACIADO ──
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "#181A20"
                clip: true

                RowLayout {
                    anchors.fill: parent
                    spacing: 0

                    ScrollView {
                        id: editorScrollView
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true

                        Row {
                            id: codeRow
                            spacing: 0
                            width: Math.max(editorScrollView.width - 60, codeEditorContainer.width + gutter.width + 40)
                            height: Math.max(editorScrollView.height, codeTextEdit.contentHeight + 30)

                            // GUTTER: NÚMEROS DE LÍNEA EXACTOS PÍXEL POR PÍXEL
                            Rectangle {
                                id: gutter
                                width: 44
                                height: parent.height
                                color: "#181A20"

                                Column {
                                    anchors.top: parent.top
                                    anchors.topMargin: 8
                                    width: parent.width

                                    Repeater {
                                        model: Math.max(1, codeTextEdit.lineCount)
                                        delegate: Item {
                                            width: gutter.width
                                            height: codeTextEdit.cursorRectangle.height > 0 ? codeTextEdit.cursorRectangle.height : 18

                                            readonly property string rawLineText: {
                                                var arr = codeTextEdit.text.split("\n")
                                                return index < arr.length ? arr[index].trim() : ""
                                            }
                                            readonly property bool isSection: rawLineText.indexOf("@") === 0

                                            // Indicador de línea actual activa
                                            Rectangle {
                                                anchors.fill: parent
                                                color: (index + 1 === root.currentLine) ? "#252832" : "transparent"
                                            }

                                            // Número de línea alineado
                                            Text {
                                                width: 30
                                                anchors.right: parent.right
                                                anchors.rightMargin: 8
                                                anchors.verticalCenter: parent.verticalCenter
                                                text: (index + 1).toString()
                                                color: (index + 1 === root.currentLine) ? "#E6EDF3" : "#555C6E"
                                                font.family: codeTextEdit.font.family
                                                font.pixelSize: codeTextEdit.font.pixelSize
                                                font.bold: (index + 1 === root.currentLine)
                                                horizontalAlignment: Text.AlignRight
                                            }

                                            // Chevron de sección
                                            Canvas {
                                                visible: parent.isSection
                                                width: 7; height: 7
                                                anchors.verticalCenter: parent.verticalCenter
                                                x: 5
                                                onPaint: {
                                                    var ctx = getContext("2d")
                                                    ctx.clearRect(0, 0, 7, 7)
                                                    ctx.strokeStyle = "#858585"
                                                    ctx.lineWidth = 1.2
                                                    ctx.beginPath()
                                                    ctx.moveTo(1, 2)
                                                    ctx.lineTo(3.5, 5)
                                                    ctx.lineTo(6, 2)
                                                    ctx.stroke()
                                                }
                                            }
                                        }
                                    }
                                }

                                // Línea divisoria vertical del gutter
                                Rectangle {
                                    anchors.right: parent.right
                                    anchors.top: parent.top
                                    anchors.bottom: parent.bottom
                                    width: 1
                                    color: "#22252D"
                                }
                            }

                            // ZONA EDITABLE DEL CÓDIGO (100% NATIVA Y CRISTALINA)
                            Item {
                                id: codeEditorContainer
                                width: Math.max(editorScrollView.width - gutter.width - 50, codeTextEdit.contentWidth + 40)
                                height: parent.height

                                // Resaltado de línea activa estilo VS Code
                                Rectangle {
                                    x: 0
                                    y: codeTextEdit.cursorRectangle.y
                                    width: parent.width
                                    height: codeTextEdit.cursorRectangle.height > 0 ? codeTextEdit.cursorRectangle.height : 18
                                    color: "#20232B"
                                    border.color: "#282C37"
                                    border.width: 1
                                    visible: codeTextEdit.activeFocus
                                }

                                // Guías de indentación verticales
                                Row {
                                    x: 10 + 32
                                    y: 8
                                    height: codeTextEdit.contentHeight
                                    spacing: 31
                                    opacity: 0.12

                                    Repeater {
                                        model: 6
                                        delegate: Rectangle {
                                            width: 1
                                            height: codeTextEdit.contentHeight
                                            color: "#8F94A0"
                                        }
                                    }
                                }

                                // TextEdit con renderizado nativo nítido
                                TextEdit {
                                    id: codeTextEdit
                                    anchors.top: parent.top
                                    anchors.left: parent.left
                                    anchors.topMargin: 8
                                    anchors.leftMargin: 10
                                    width: parent.width
                                    height: Math.max(contentHeight + 30, editorScrollView.height)
                                    color: "#D4D4D4"
                                    selectedTextColor: "#FFFFFF"
                                    selectionColor: "#264F78"
                                    font.family: "JetBrains Mono, Consolas, 'Courier New', Monospace"
                                    font.pixelSize: 13
                                    wrapMode: TextEdit.NoWrap
                                    tabStopDistance: 32
                                    cursorVisible: true
                                    selectByMouse: true

                                    // Cursor personalizado de 2px estilo VS Code
                                    cursorDelegate: Rectangle {
                                        width: 2
                                        color: "#AEAFAD"
                                        visible: codeTextEdit.activeFocus

                                        SequentialAnimation on opacity {
                                            loops: Animation.Infinite
                                            running: codeTextEdit.activeFocus
                                            NumberAnimation { to: 1.0; duration: 100 }
                                            PauseAnimation { duration: 400 }
                                            NumberAnimation { to: 0.0; duration: 150 }
                                            PauseAnimation { duration: 300 }
                                        }
                                    }

                                    onCursorPositionChanged: {
                                        root.updateCursorPosition()
                                    }

                                    onTextChanged: {
                                        root.isModified = true
                                        root.updateCursorPosition()
                                    }

                                    Component.onCompleted: {
                                        if (text.length === 0) root.loadTemplateDelay()
                                        root.updateCursorPosition()
                                    }
                                }
                            }
                        }
                    }

                    // ── MINIMAPA LATERAL ESTILO VS CODE ──────────
                    Rectangle {
                        Layout.preferredWidth: 54
                        Layout.fillHeight: true
                        color: "#181A20"
                        clip: true

                        Rectangle {
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            width: 1
                            color: "#22252D"
                        }

                        Column {
                            anchors.fill: parent
                            anchors.topMargin: 8
                            anchors.leftMargin: 6
                            anchors.rightMargin: 6
                            spacing: 2
                            opacity: 0.65

                            Repeater {
                                model: Math.min(80, codeTextEdit.lineCount)
                                delegate: Item {
                                    width: 42
                                    height: 2

                                    readonly property string lineStr: {
                                        var arr = codeTextEdit.text.split("\n")
                                        return index < arr.length ? arr[index].trim() : ""
                                    }

                                    Rectangle {
                                        width: Math.min(parent.width, Math.max(6, parent.lineStr.length * 1.4))
                                        height: 2
                                        radius: 1
                                        color: {
                                            if (parent.lineStr.indexOf("//") === 0) return "#628A62"
                                            if (parent.lineStr.indexOf("@") === 0) return "#F0C674"
                                            if (parent.lineStr.indexOf("slider") === 0) return "#7EC67E"
                                            if (parent.lineStr.indexOf("desc:") === 0) return "#E5A44E"
                                            return "#7A8294"
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        // =====================================================================
        // 4. CONSOLA INFERIOR Y BARRA DE ESTADO (CONSOLE & STATUS BAR)
        // =====================================================================
        Rectangle {
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
    }

    // =========================================================================
    // 5. CÁLCULO DE POSICIÓN Y ESTADO DE LÍNEA
    // =========================================================================

    function updateCursorPosition() {
        var pos = codeTextEdit.cursorPosition
        var sub = codeTextEdit.text.substring(0, pos)
        var lines = sub.split("\n")
        root.currentLine = lines.length
        root.currentCol = lines[lines.length - 1].length + 1

        var detectedSection = "desc"
        for (var i = lines.length - 1; i >= 0; --i) {
            var trimmed = lines[i].trim()
            if (trimmed.indexOf("@") === 0) {
                detectedSection = trimmed.split(/\s+/)[0]
                break
            }
        }
        root.currentSection = detectedSection
    }

    // =========================================================================
    // 6. LÓGICA DE COMPILACIÓN, GUARDADO, BÚSQUEDA Y NAVEGACIÓN
    // =========================================================================

    function compileAndApply() {
        if (typeof AudioEngine === "undefined" || !AudioEngine.tracks) {
            root.statusMessage = "Error: El motor de audio no está activo"
            root.hasError = true
            return
        }

        if (root.isNewPlugin) {
            var ok = AudioEngine.tracks.createCustomJsfxPlugin(root.trackIndex, root.pluginName.replace(".jsfx", ""), codeTextEdit.text)
            if (ok) {
                root.statusMessage = "Compiled successfully"
                root.hasError = false
                root.isModified = false
                root.isNewPlugin = false
                root.scriptApplied()
            } else {
                root.statusMessage = "Compilation failed: Syntax error in script sections"
                root.hasError = true
            }
        } else {
            var err = AudioEngine.tracks.updatePluginScript(root.trackIndex, root.pluginIndex, codeTextEdit.text)
            if (!err || err.length === 0) {
                root.statusMessage = "Compiled successfully"
                root.hasError = false
                root.isModified = false
                root.scriptApplied()
            } else {
                root.statusMessage = err
                root.hasError = true
            }
        }
    }

    function saveAndInsert() {
        root.compileAndApply()
    }

    function loadFileByName(fname) {
        root.selectedFile = fname
        root.pluginName = fname
        root.isModified = false

        if (fname === "simple_delay.jsfx") root.loadTemplateDelay()
        else if (fname === "multi_tap.jsfx") root.loadTemplateMultiTap()
        else if (fname === "chorus.jsfx") root.loadTemplateChorus()
        else if (fname === "phaser_stereo.jsfx") root.loadTemplatePhaser()
        else if (fname === "distortion.jsfx") root.loadTemplateDistortion()
        else if (fname === "lfo_filter.jsfx") root.loadTemplateLfoFilter()
        else if (fname === "reverb.jsfx") root.loadTemplateReverb()
        else if (fname === "utils.jsh") root.loadTemplateUtils()
        else root.loadTemplateDelay()

        root.statusMessage = "Compiled successfully"
        root.hasError = false
        root.updateCursorPosition()
    }

    function openForPlugin(trkIdx, plgIdx, plgNm) {
        root.trackIndex = trkIdx
        root.pluginIndex = plgIdx
        root.pluginName = (plgNm.indexOf(".jsfx") !== -1) ? plgNm : (plgNm.toLowerCase().replace(/ /g, "_") + ".jsfx")
        root.selectedFile = root.pluginName
        root.isNewPlugin = false
        root.isModified = false

        if (typeof AudioEngine !== "undefined" && AudioEngine.tracks) {
            var src = AudioEngine.tracks.getPluginScript(trkIdx, plgIdx)
            if (src && src.length > 0) {
                codeTextEdit.text = src
            } else {
                root.loadTemplateDelay()
            }
        } else {
            root.loadTemplateDelay()
        }

        root.statusMessage = "Compiled successfully"
        root.hasError = false
        root.updateCursorPosition()
    }

    function openNewScript(trkIdx) {
        root.trackIndex = trkIdx
        root.pluginIndex = -1
        root.pluginName = "simple_delay.jsfx"
        root.selectedFile = "simple_delay.jsfx"
        root.isNewPlugin = true
        root.isModified = false
        root.loadTemplateDelay()
        root.statusMessage = "Compiled successfully"
        root.hasError = false
        root.updateCursorPosition()
    }

    function searchInCode(term) {
        if (!term || term.length === 0) return
        var idx = codeTextEdit.text.indexOf(term, codeTextEdit.cursorPosition)
        if (idx === -1) idx = codeTextEdit.text.indexOf(term, 0)
        if (idx !== -1) {
            codeTextEdit.cursorPosition = idx
            codeTextEdit.select(idx, idx + term.length)
        }
    }

    function findNext() {
        if (!searchInput.text || searchInput.text.length === 0) return
        var term = searchInput.text
        var start = codeTextEdit.cursorPosition + 1
        var idx = codeTextEdit.text.indexOf(term, start)
        if (idx === -1) idx = codeTextEdit.text.indexOf(term, 0)
        if (idx !== -1) {
            codeTextEdit.cursorPosition = idx
            codeTextEdit.select(idx, idx + term.length)
        }
    }

    function findPrev() {
        if (!searchInput.text || searchInput.text.length === 0) return
        var term = searchInput.text
        var start = Math.max(0, codeTextEdit.cursorPosition - 1)
        var idx = codeTextEdit.text.lastIndexOf(term, start)
        if (idx === -1) idx = codeTextEdit.text.lastIndexOf(term)
        if (idx !== -1) {
            codeTextEdit.cursorPosition = idx
            codeTextEdit.select(idx, idx + term.length)
        }
    }

    function formatScript() {
        var lines = codeTextEdit.text.split("\n")
        var out = []
        for (var i = 0; i < lines.length; ++i) {
            var l = lines[i].trim()
            if (l.indexOf("@") === 0) {
                out.push("")
                out.push(l)
            } else if (l.length > 0) {
                out.push(l)
            }
        }
        codeTextEdit.text = out.join("\n").trim() + "\n"
    }

    // =========================================================================
    // 7. SCRIPTS COMPLETOS PARA EL ÁRBOL DE PROYECTO
    // =========================================================================

    function loadTemplateDelay() {
        codeTextEdit.text =
            "// Simple Stereo Delay\n" +
            "desc:Stereo Delay Effect\n" +
            "slider1:200<0,1000,10>Delay (ms)\n" +
            "slider2:0.5<0,1,0.01>Feedback\n\n" +
            "@init\n" +
            "delay_samples = srate * (slider1 / 1000);\n" +
            "buf_pos = 0;\n" +
            "buf_len = delay_samples;\n\n" +
            "@slider\n" +
            "delay_samples = srate * (slider1 / 1000);\n\n" +
            "@sample\n" +
            "dry_L = spl0;\n" +
            "dry_R = spl1;\n" +
            "delayed_L = buf_pos[0];\n" +
            "spl0 = dry_L + delayed_L * slider2;\n" +
            "buf_pos[0] = dry_L;\n" +
            "buf_pos += 1;\n" +
            "if (buf_pos >= buf_len) then buf_pos = 0;\n"
    }

    function loadTemplateMultiTap() {
        codeTextEdit.text =
            "// Multi-Tap Delay\n" +
            "desc:Multi-Tap Stereo Delay\n" +
            "slider1:250<10,1000,10>Tap 1 (ms)\n" +
            "slider2:500<10,2000,10>Tap 2 (ms)\n" +
            "slider3:0.4<0,1,0.01>Tap Feedback\n\n" +
            "@init\n" +
            "buf_pos = 0;\n" +
            "max_len = srate * 2;\n\n" +
            "@slider\n" +
            "t1 = floor(slider1 * 0.001 * srate);\n" +
            "t2 = floor(slider2 * 0.001 * srate);\n\n" +
            "@sample\n" +
            "p1 = (buf_pos - t1 + max_len) % max_len;\n" +
            "p2 = (buf_pos - t2 + max_len) % max_len;\n" +
            "tap1 = p1[0];\n" +
            "tap2 = p2[0];\n" +
            "buf_pos[0] = spl0 + (tap1 + tap2) * 0.5 * slider3;\n" +
            "spl0 = spl0 + tap1 * 0.6;\n" +
            "spl1 = spl1 + tap2 * 0.6;\n" +
            "buf_pos = (buf_pos + 1) % max_len;\n"
    }

    function loadTemplateChorus() {
        codeTextEdit.text =
            "// Stereo Chorus\n" +
            "desc:Vintage Stereo Chorus\n" +
            "slider1:1.5<0.1,5,0.1>Rate (Hz)\n" +
            "slider2:15<1,40,1>Depth (ms)\n" +
            "slider3:50<0,100,1>Mix (%)\n\n" +
            "@init\n" +
            "lfo_phase = 0;\n" +
            "buf_pos = 0;\n" +
            "max_del = srate * 0.1;\n\n" +
            "@slider\n" +
            "rate_inc = 2 * 3.14159265 * slider1 / srate;\n" +
            "depth_samp = slider2 * 0.001 * srate;\n" +
            "wet = slider3 / 100;\n" +
            "dry = 1 - wet;\n\n" +
            "@sample\n" +
            "mod = sin(lfo_phase) * depth_samp;\n" +
            "read_p = (buf_pos - floor(mod + depth_samp) + max_del) % max_del;\n" +
            "del_sig = read_p[0];\n" +
            "buf_pos[0] = (spl0 + spl1) * 0.5;\n" +
            "buf_pos = (buf_pos + 1) % max_del;\n" +
            "lfo_phase += rate_inc;\n" +
            "spl0 = spl0 * dry + del_sig * wet;\n" +
            "spl1 = spl1 * dry + del_sig * wet;\n"
    }

    function loadTemplatePhaser() {
        codeTextEdit.text =
            "// Stereo Phaser\n" +
            "desc:4-Pole Analog Phaser\n" +
            "slider1:0.5<0.05,4,0.05>LFO Speed (Hz)\n" +
            "slider2:60<0,95,1>Feedback (%)\n\n" +
            "@init\n" +
            "phase = 0;\n" +
            "a1 = 0; a2 = 0; a3 = 0; a4 = 0;\n\n" +
            "@slider\n" +
            "inc = 2 * 3.14159265 * slider1 / srate;\n" +
            "fb = slider2 / 100;\n\n" +
            "@sample\n" +
            "lfo = (sin(phase) + 1) * 0.5;\n" +
            "phase += inc;\n" +
            "in = spl0 + a4 * fb;\n" +
            "a1 = in * (1 - lfo);\n" +
            "a2 = a1 * (1 - lfo);\n" +
            "a3 = a2 * (1 - lfo);\n" +
            "a4 = a3 * (1 - lfo);\n" +
            "spl0 = spl0 * 0.5 + a4 * 0.5;\n" +
            "spl1 = spl1 * 0.5 + a4 * 0.5;\n"
    }

    function loadTemplateDistortion() {
        codeTextEdit.text =
            "// Hard Distortion\n" +
            "desc:Hard Knee Clipper Distortion\n" +
            "slider1:12<0,36,0.1>Drive (dB)\n" +
            "slider2:0<-24,6,0.1>Output Trim (dB)\n\n" +
            "@init\n" +
            "in_gain = 1;\n" +
            "out_gain = 1;\n\n" +
            "@slider\n" +
            "in_gain = 10^(slider1 / 20);\n" +
            "out_gain = 10^(slider2 / 20);\n\n" +
            "@sample\n" +
            "x0 = spl0 * in_gain;\n" +
            "x1 = spl1 * in_gain;\n" +
            "spl0 = min(1.0, max(-1.0, x0)) * out_gain;\n" +
            "spl1 = min(1.0, max(-1.0, x1)) * out_gain;\n"
    }

    function loadTemplateLfoFilter() {
        codeTextEdit.text =
            "// LFO Auto-Wah Filter\n" +
            "desc:LFO Modulated Resonant Filter\n" +
            "slider1:2.0<0.1,10,0.1>Rate (Hz)\n" +
            "slider2:800<200,4000,10>Center Freq (Hz)\n" +
            "slider3:2.5<0.5,10,0.1>Resonance (Q)\n\n" +
            "@init\n" +
            "lfo_p = 0;\n" +
            "s1 = 0; s2 = 0;\n\n" +
            "@slider\n" +
            "lfo_inc = 2 * 3.14159265 * slider1 / srate;\n\n" +
            "@sample\n" +
            "mod_freq = slider2 * (1 + 0.6 * sin(lfo_p));\n" +
            "lfo_p += lfo_inc;\n" +
            "w0 = 2 * 3.14159265 * mod_freq / srate;\n" +
            "alpha = sin(w0) / (2 * slider3);\n" +
            "b0 = (1 - cos(w0)) * 0.5;\n" +
            "b1 = 1 - cos(w0);\n" +
            "b2 = b0;\n" +
            "a0 = 1 + alpha;\n" +
            "a1 = -2 * cos(w0);\n" +
            "a2 = 1 - alpha;\n" +
            "out0 = (b0/a0)*spl0 + s1;\n" +
            "s1 = (b1/a0)*spl0 - (a1/a0)*out0 + s2;\n" +
            "s2 = (b2/a0)*spl0 - (a2/a0)*out0;\n" +
            "spl0 = out0;\n" +
            "spl1 = out0;\n"
    }

    function loadTemplateReverb() {
        codeTextEdit.text =
            "// Algorithmic Reverb\n" +
            "desc:4-Comb Stereo Reverb\n" +
            "slider1:45<0,100,1>Room Size (%)\n" +
            "slider2:30<0,100,1>Wet Mix (%)\n\n" +
            "@init\n" +
            "pos1 = 0; pos2 = 0; pos3 = 0;\n" +
            "len1 = 1557; len2 = 1617; len3 = 1491;\n\n" +
            "@slider\n" +
            "feedback = 0.5 + 0.45 * (slider1 / 100);\n" +
            "wet = slider2 / 100;\n" +
            "dry = 1 - wet;\n\n" +
            "@sample\n" +
            "sig = (spl0 + spl1) * 0.5;\n" +
            "out1 = pos1[0];\n" +
            "pos1[0] = sig + out1 * feedback;\n" +
            "pos1 = (pos1 + 1) % len1;\n" +
            "out2 = (pos2 + 2000)[0];\n" +
            "(pos2 + 2000)[0] = sig + out2 * feedback;\n" +
            "pos2 = (pos2 + 1) % len2;\n" +
            "rev_sig = (out1 + out2) * 0.5;\n" +
            "spl0 = spl0 * dry + rev_sig * wet;\n" +
            "spl1 = spl1 * dry + rev_sig * wet;\n"
    }

    function loadTemplateUtils() {
        codeTextEdit.text =
            "// Utility Functions Header\n" +
            "desc:NOVA DSP Math Utilities (Header)\n\n" +
            "function db2gain(db) (\n" +
            "    10 ^ (db / 20);\n" +
            ");\n\n" +
            "function gain2db(gain) (\n" +
            "    gain <= 0.00001 ? -100 : 20 * log10(gain);\n" +
            ");\n\n" +
            "function clip(val, min_v, max_v) (\n" +
            "    val < min_v ? min_v : (val > max_v ? max_v : val);\n" +
            ");\n"
    }
}
