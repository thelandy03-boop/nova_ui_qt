import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

/**
 * Editor de Código en Vivo para Scripts JSFX (REAPER EEL2) — Diseño VS Code Dark Modern.
 * Proporciona un entorno de desarrollo integrado completo para crear, editar,
 * depurar y probar efectos de audio DSP en tiempo real.
 */
Rectangle {
    id: root

    // ── Propiedades de integración con AudioEngine ───────────────────────────
    property int trackIndex: -1
    property int pluginIndex: -1
    property string pluginName: "Script JSFX"
    property bool isNewPlugin: false
    property bool isModified: false
    property bool isMaximized: false

    // ── Estado de cursor, selección y sección activa ─────────────────────────
    property int currentLine: 1
    property int currentCol: 1
    property int selectedChars: 0
    property string currentSection: "@sample"

    // ── Estado de vistas (Activity Bar, Sidebar, Minimap, Command Palette) ──
    // 0: Explorer, 1: Search, 2: Sliders DSP en vivo, 3: Snippets EEL2
    property int activeSidebarTab: 0
    property bool sidebarVisible: true
    property bool minimapVisible: true
    property bool wordWrapEnabled: false
    property bool commandPaletteVisible: false

    // ── Secciones colapsables del Explorer (Acordeones VS Code) ──────────────
    property bool openEditorsExpanded: true
    property bool activeScriptExpanded: true
    property bool templatesExpanded: true
    property bool outlineExpanded: true

    // ── Estado del panel inferior (0: Problemas, 1: Salida, 2: Terminal DSP) ─
    property int activeBottomTab: 1
    property bool bottomPanelVisible: true
    property int bottomPanelHeight: 155
    property string lastCompilationLog: "Listo para compilar. Presiona Ctrl+S o F5."
    property string compilationError: ""

    // ── Búsqueda y Reemplazo flotante (Ctrl+F / Ctrl+H) ──────────────────────
    property bool searchBarVisible: false
    property bool replaceVisible: false
    property bool matchCase: false
    property string searchTerm: ""
    property int searchMatchesCount: 0
    property int currentMatchIndex: 0

    // ── Sliders parseados dinámicamente ──────────────────────────────────────
    property var parsedSliders: []

    // ── Paleta Oficial VS Code Dark Modern ───────────────────────────────────
    readonly property color colTitleBar:       "#181818"
    readonly property color colActivityBar:    "#181818"
    readonly property color colSideBar:        "#181818"
    readonly property color colEditorBg:       "#1F1F1F"
    readonly property color colEditorGroupHdr: "#181818"
    readonly property color colTabActive:      "#1F1F1F"
    readonly property color colTabInactive:    "#181818"
    readonly property color colBorder:         "#2B2B2B"
    readonly property color colWidgetBg:       "#252526"
    readonly property color colInputBg:        "#313131"
    readonly property color colHoverBg:        "#2A2D2E"
    readonly property color colListActiveBg:   "#04395E"
    readonly property color colAccent:         "#0078D4"
    readonly property color colAccentHover:    "#026EC1"
    readonly property color colTextPrimary:    "#CCCCCC"
    readonly property color colTextBright:     "#FFFFFF"
    readonly property color colTextMuted:      "#858585"
    readonly property color colError:          "#F14C4C"
    readonly property color colWarning:        "#CCA700"
    readonly property color colSuccess:        "#89D185"
    readonly property color colDirtyDot:       "#E2C08D"

    signal scriptApplied()
    signal closed()

    anchors.fill: parent
    color: "#BF000000" // Backdrop oscuro translúcido
    z: 10000

    // Bloquear clics hacia el fondo
    MouseArea {
        anchors.fill: parent
        onClicked: {}
    }

    // ── Atajos de Teclado Estándar de VS Code ────────────────────────────────
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
        onActivated: root.sidebarVisible = !root.sidebarVisible
    }
    Shortcut {
        sequence: "Ctrl+`"
        onActivated: root.bottomPanelVisible = !root.bottomPanelVisible
    }
    Shortcut {
        sequence: "Ctrl+J"
        onActivated: root.bottomPanelVisible = !root.bottomPanelVisible
    }
    Shortcut {
        sequence: "Alt+Z"
        onActivated: root.wordWrapEnabled = !root.wordWrapEnabled
    }
    Shortcut {
        sequence: "Ctrl+Shift+P"
        onActivated: {
            root.commandPaletteVisible = !root.commandPaletteVisible
            if (root.commandPaletteVisible) {
                cmdPaletteInput.text = ">"
                cmdPaletteInput.forceActiveFocus()
            }
        }
    }
    Shortcut {
        sequence: "F1"
        onActivated: {
            root.commandPaletteVisible = true
            cmdPaletteInput.text = ">"
            cmdPaletteInput.forceActiveFocus()
        }
    }
    Shortcut {
        sequence: "Ctrl+F"
        onActivated: {
            root.searchBarVisible = true
            searchField.forceActiveFocus()
            searchField.selectAll()
        }
    }
    Shortcut {
        sequence: "Ctrl+H"
        onActivated: {
            root.searchBarVisible = true
            root.replaceVisible = true
            searchField.forceActiveFocus()
            searchField.selectAll()
        }
    }
    Shortcut {
        sequence: "Escape"
        onActivated: {
            if (root.commandPaletteVisible) {
                root.commandPaletteVisible = false
                codeEditor.forceActiveFocus()
            } else if (root.searchBarVisible) {
                root.searchBarVisible = false
                codeEditor.forceActiveFocus()
            } else {
                root.closeDialog()
            }
        }
    }

    // =========================================================================
    // VENTANA PRINCIPAL ESTILO VS CODE DARK MODERN
    // =========================================================================
    Rectangle {
        id: vsCodeWindow
        anchors.centerIn: parent
        width: root.isMaximized ? parent.width : Math.min(parent.width - 20, 1220)
        height: root.isMaximized ? parent.height : Math.min(parent.height - 20, 820)
        radius: root.isMaximized ? 0 : 8
        color: root.colEditorBg
        border.color: "#3C3C3C"
        border.width: root.isMaximized ? 0 : 1
        clip: true

        Behavior on width { NumberAnimation { duration: 120; easing.type: Easing.OutQuad } }
        Behavior on height { NumberAnimation { duration: 120; easing.type: Easing.OutQuad } }

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            // ─────────────────────────────────────────────────────────────────
            // 1. BARRA DE TÍTULO SUPERIOR (VS Code Title Bar + Command Center)
            // ─────────────────────────────────────────────────────────────────
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 35
                color: root.colTitleBar

                // Borde inferior sutil de la barra de título
                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: 1
                    color: root.colBorder
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 0
                    spacing: 6

                    // Logo estilo VS Code en azul infinito
                    Rectangle {
                        Layout.preferredWidth: 18
                        Layout.preferredHeight: 18
                        radius: 3
                        color: root.colAccent

                        Canvas {
                            anchors.fill: parent
                            anchors.margins: 3
                            onPaint: {
                                var ctx = getContext("2d")
                                ctx.clearRect(0, 0, width, height)
                                ctx.strokeStyle = "#FFFFFF"
                                ctx.lineWidth = 1.6
                                ctx.beginPath()
                                ctx.moveTo(width * 0.75, 1)
                                ctx.lineTo(width * 0.2, height * 0.5)
                                ctx.lineTo(width * 0.75, height - 1)
                                ctx.moveTo(width * 0.75, 1)
                                ctx.lineTo(width * 0.75, height - 1)
                                ctx.stroke()
                            }
                        }
                    }

                    // Menú superior compacto estilo VS Code
                    Row {
                        spacing: 1
                        Repeater {
                            model: [
                                { label: "Archivo", action: "file" },
                                { label: "Editar", action: "edit" },
                                { label: "Selección", action: "sel" },
                                { label: "Ver", action: "view" },
                                { label: "Ejecutar", action: "run" },
                                { label: "Terminal", action: "term" }
                            ]
                            delegate: Rectangle {
                                width: menuTxt.implicitWidth + 12
                                height: 22
                                radius: 4
                                color: menuArea.containsMouse ? "#2A2D2E" : "transparent"

                                Text {
                                    id: menuTxt
                                    anchors.centerIn: parent
                                    text: modelData.label
                                    color: "#CCCCCC"
                                    font.pixelSize: 12
                                    font.family: "Segoe UI, Inter, system-ui, sans-serif"
                                }

                                MouseArea {
                                    id: menuArea
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        if (modelData.action === "run") root.compileAndApply()
                                        else if (modelData.action === "view") root.sidebarVisible = !root.sidebarVisible
                                        else if (modelData.action === "term") root.bottomPanelVisible = !root.bottomPanelVisible
                                        else if (modelData.action === "edit") {
                                            root.searchBarVisible = !root.searchBarVisible
                                        } else if (modelData.action === "sel") {
                                            codeEditor.selectAll()
                                        } else {
                                            root.commandPaletteVisible = true
                                            cmdPaletteInput.text = ">"
                                            cmdPaletteInput.forceActiveFocus()
                                        }
                                    }
                                }
                            }
                        }
                    }

                    Item { Layout.fillWidth: true }

                    // Flechas de navegación junto al Command Center (← →)
                    Row {
                        spacing: 2
                        Repeater {
                            model: ["←", "→"]
                            delegate: Rectangle {
                                width: 22; height: 22; radius: 4
                                color: navArrArea.containsMouse ? "#2A2D2E" : "transparent"
                                Text {
                                    anchors.centerIn: parent
                                    text: modelData
                                    color: "#858585"
                                    font.pixelSize: 12
                                }
                                MouseArea {
                                    id: navArrArea
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                }
                            }
                        }
                    }

                    // COMMAND CENTER CENTRAL (Barra de búsqueda/comandos de VS Code)
                    Rectangle {
                        Layout.preferredHeight: 24
                        Layout.preferredWidth: Math.min(380, Math.max(240, vsCodeWindow.width * 0.32))
                        radius: 6
                        color: cmdCenterArea.containsMouse ? "#2A2D2E" : "#242424"
                        border.color: cmdCenterArea.containsMouse ? "#454545" : "#333333"
                        border.width: 1

                        RowLayout {
                            anchors.centerIn: parent
                            spacing: 6

                            // Icono lupa pequeño
                            Canvas {
                                width: 12; height: 12
                                onPaint: {
                                    var ctx = getContext("2d")
                                    ctx.clearRect(0, 0, width, height)
                                    ctx.strokeStyle = "#858585"
                                    ctx.lineWidth = 1.3
                                    ctx.beginPath()
                                    ctx.arc(5, 5, 3.8, 0, Math.PI * 2)
                                    ctx.moveTo(8, 8)
                                    ctx.lineTo(11, 11)
                                    ctx.stroke()
                                }
                            }

                            Text {
                                id: titleCenterText
                                text: (root.isNewPlugin ? "nuevo_script.jsfx" : (root.pluginName.toLowerCase().replace(/ /g, "_") + ".jsfx")) +
                                      " — Nova Studio DSP" + (root.isModified ? " ●" : "")
                                color: root.isModified ? root.colDirtyDot : "#CCCCCC"
                                font.pixelSize: 11
                                font.family: "Segoe UI, Inter, sans-serif"
                                elide: Text.ElideRight
                            }
                        }

                        MouseArea {
                            id: cmdCenterArea
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                root.commandPaletteVisible = !root.commandPaletteVisible
                                if (root.commandPaletteVisible) {
                                    cmdPaletteInput.text = ">"
                                    cmdPaletteInput.forceActiveFocus()
                                }
                            }
                        }
                    }

                    Item { Layout.fillWidth: true }

                    // Botón de Compilar y Ejecutar en Caliente (Estilo Run Action de VS Code)
                    Rectangle {
                        Layout.preferredHeight: 24
                        Layout.preferredWidth: compileHeadBtnText.implicitWidth + 30
                        radius: 4
                        color: headCompileArea.containsMouse ? root.colAccentHover : root.colAccent

                        Row {
                            anchors.centerIn: parent
                            spacing: 6

                            Canvas {
                                width: 10; height: 10
                                anchors.verticalCenter: parent.verticalCenter
                                onPaint: {
                                    var ctx = getContext("2d")
                                    ctx.clearRect(0, 0, width, height)
                                    ctx.fillStyle = "#FFFFFF"
                                    ctx.beginPath()
                                    ctx.moveTo(1, 0)
                                    ctx.lineTo(width, height / 2)
                                    ctx.lineTo(1, height)
                                    ctx.closePath()
                                    ctx.fill()
                                }
                            }

                            Text {
                                id: compileHeadBtnText
                                text: "Compilar JIT"
                                color: "#FFFFFF"
                                font.pixelSize: 11
                                font.bold: true
                            }
                        }

                        MouseArea {
                            id: headCompileArea
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.compileAndApply()
                        }
                    }

                    // ── Controles de Layout de VS Code (Sidebar / Panel / Minimap) ──
                    Row {
                        spacing: 2

                        // Toggle Primary Side Bar (Ctrl+B)
                        Rectangle {
                            width: 26; height: 24; radius: 4
                            color: toggleSideArea.containsMouse ? "#2A2D2E" : "transparent"

                            Rectangle {
                                width: 14; height: 12
                                anchors.centerIn: parent
                                color: "transparent"
                                border.color: root.sidebarVisible ? "#CCCCCC" : "#6E6E6E"
                                border.width: 1.2
                                radius: 1

                                Rectangle {
                                    width: 4; height: parent.height
                                    color: root.sidebarVisible ? "#CCCCCC" : "transparent"
                                    border.color: "#6E6E6E"
                                    border.width: root.sidebarVisible ? 0 : 1
                                }
                            }

                            MouseArea {
                                id: toggleSideArea
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.sidebarVisible = !root.sidebarVisible
                            }
                        }

                        // Toggle Bottom Panel (Ctrl+J)
                        Rectangle {
                            width: 26; height: 24; radius: 4
                            color: toggleBottomArea.containsMouse ? "#2A2D2E" : "transparent"

                            Rectangle {
                                width: 14; height: 12
                                anchors.centerIn: parent
                                color: "transparent"
                                border.color: root.bottomPanelVisible ? "#CCCCCC" : "#6E6E6E"
                                border.width: 1.2
                                radius: 1

                                Rectangle {
                                    anchors.bottom: parent.bottom
                                    width: parent.width; height: 4
                                    color: root.bottomPanelVisible ? "#CCCCCC" : "transparent"
                                }
                            }

                            MouseArea {
                                id: toggleBottomArea
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.bottomPanelVisible = !root.bottomPanelVisible
                            }
                        }

                        // Toggle Minimap
                        Rectangle {
                            width: 26; height: 24; radius: 4
                            color: toggleMapArea.containsMouse ? "#2A2D2E" : "transparent"

                            Rectangle {
                                width: 14; height: 12
                                anchors.centerIn: parent
                                color: "transparent"
                                border.color: root.minimapVisible ? "#CCCCCC" : "#6E6E6E"
                                border.width: 1.2
                                radius: 1

                                Rectangle {
                                    anchors.right: parent.right
                                    width: 4; height: parent.height
                                    color: root.minimapVisible ? "#CCCCCC" : "transparent"
                                }
                            }

                            MouseArea {
                                id: toggleMapArea
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.minimapVisible = !root.minimapVisible
                            }
                        }
                    }

                    // Divisor vertical antes de controles de ventana
                    Rectangle {
                        width: 1; height: 16
                        color: "#333333"
                    }

                    // ── Botones de Control de Ventana (Maximizar / Cerrar) ────────
                    Row {
                        spacing: 0

                        // Maximizar / Restaurar
                        Rectangle {
                            width: 38; height: 34
                            color: maxWinArea.containsMouse ? "#2A2D2E" : "transparent"

                            Rectangle {
                                width: 10; height: 10
                                anchors.centerIn: parent
                                color: "transparent"
                                border.color: "#CCCCCC"
                                border.width: 1.2
                            }

                            MouseArea {
                                id: maxWinArea
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.isMaximized = !root.isMaximized
                            }
                        }

                        // Cerrar ventana (Hover rojo #E81123 oficial de VS Code)
                        Rectangle {
                            width: 42; height: 34
                            color: closeHeadArea.containsMouse ? "#E81123" : "transparent"

                            Text {
                                anchors.centerIn: parent
                                text: "✕"
                                color: closeHeadArea.containsMouse ? "#FFFFFF" : "#CCCCCC"
                                font.pixelSize: 12
                            }

                            MouseArea {
                                id: closeHeadArea
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.closeDialog()
                            }
                        }
                    }
                }
            }

            // ─────────────────────────────────────────────────────────────────
            // 2. CUERPO CENTRAL (Activity Bar + Sidebar + Editor + Panel)
            // ─────────────────────────────────────────────────────────────────
            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0

                // 2.1 ACTIVITY BAR LATERAL IZQUIERDA (VS Code Activity Bar)
                Rectangle {
                    Layout.preferredWidth: 48
                    Layout.fillHeight: true
                    color: root.colActivityBar

                    // Borde derecho de la Activity Bar
                    Rectangle {
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        width: 1
                        color: root.colBorder
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 0
                        anchors.topMargin: 2
                        anchors.bottomMargin: 6

                        // ── ICONO 0: EXPLORER (Archivos y Plantillas) ─────────
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 48
                            color: "transparent"

                            // Indicador activo azul VS Code Dark Modern
                            Rectangle {
                                anchors.left: parent.left
                                anchors.verticalCenter: parent.verticalCenter
                                width: 2; height: 24
                                radius: 1
                                color: root.colAccent
                                visible: root.activeSidebarTab === 0 && root.sidebarVisible
                            }

                            Item {
                                width: 22; height: 22
                                anchors.centerIn: parent
                                opacity: (root.activeSidebarTab === 0 && root.sidebarVisible) || actExpArea.containsMouse ? 1.0 : 0.6

                                Rectangle {
                                    x: 2; y: 5; width: 13; height: 15
                                    color: "transparent"
                                    border.color: "#FFFFFF"
                                    border.width: 1.5
                                    radius: 1
                                }
                                Rectangle {
                                    x: 6; y: 1; width: 13; height: 15
                                    color: root.colActivityBar
                                    border.color: "#FFFFFF"
                                    border.width: 1.5
                                    radius: 1
                                }
                            }

                            MouseArea {
                                id: actExpArea
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (root.activeSidebarTab === 0 && root.sidebarVisible) {
                                        root.sidebarVisible = false
                                    } else {
                                        root.activeSidebarTab = 0
                                        root.sidebarVisible = true
                                    }
                                }
                            }
                        }

                        // ── ICONO 1: SEARCH (Búsqueda Rápida) ─────────────────
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 48
                            color: "transparent"

                            Canvas {
                                width: 22; height: 22
                                anchors.centerIn: parent
                                opacity: root.searchBarVisible || actSearchArea.containsMouse ? 1.0 : 0.6
                                onPaint: {
                                    var ctx = getContext("2d")
                                    ctx.clearRect(0, 0, width, height)
                                    ctx.strokeStyle = "#FFFFFF"
                                    ctx.lineWidth = 1.6
                                    ctx.beginPath()
                                    ctx.arc(9.5, 9.5, 6, 0, Math.PI * 2)
                                    ctx.moveTo(14, 14)
                                    ctx.lineTo(19, 19)
                                    ctx.stroke()
                                }
                            }

                            MouseArea {
                                id: actSearchArea
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    root.searchBarVisible = !root.searchBarVisible
                                    if (root.searchBarVisible) {
                                        searchField.forceActiveFocus()
                                        searchField.selectAll()
                                    }
                                }
                            }
                        }

                        // ── ICONO 2: PARÁMETROS DSP EN VIVO (Con Badge de conteo) ──
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 48
                            color: "transparent"

                            Rectangle {
                                anchors.left: parent.left
                                anchors.verticalCenter: parent.verticalCenter
                                width: 2; height: 24
                                radius: 1
                                color: root.colAccent
                                visible: root.activeSidebarTab === 1 && root.sidebarVisible
                            }

                            Item {
                                width: 22; height: 22
                                anchors.centerIn: parent
                                opacity: (root.activeSidebarTab === 1 && root.sidebarVisible) || actSlidersArea.containsMouse ? 1.0 : 0.6

                                Rectangle { x: 2; y: 4; width: 18; height: 1.5; color: "#FFFFFF" }
                                Rectangle { x: 6; y: 1.5; width: 4; height: 6.5; color: "#FFFFFF"; radius: 1 }

                                Rectangle { x: 2; y: 11; width: 18; height: 1.5; color: "#FFFFFF" }
                                Rectangle { x: 13; y: 8.5; width: 4; height: 6.5; color: "#FFFFFF"; radius: 1 }

                                Rectangle { x: 2; y: 18; width: 18; height: 1.5; color: "#FFFFFF" }
                                Rectangle { x: 8; y: 15.5; width: 4; height: 6.5; color: "#FFFFFF"; radius: 1 }
                            }

                            // Badge numérico estilo VS Code
                            Rectangle {
                                visible: root.parsedSliders.length > 0
                                x: 26; y: 24
                                width: 16; height: 16
                                radius: 8
                                color: root.colAccent
                                border.color: root.colActivityBar
                                border.width: 1.5

                                Text {
                                    anchors.centerIn: parent
                                    text: root.parsedSliders.length.toString()
                                    color: "#FFFFFF"
                                    font.pixelSize: 9
                                    font.bold: true
                                }
                            }

                            MouseArea {
                                id: actSlidersArea
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    root.refreshParsedSliders()
                                    if (root.activeSidebarTab === 1 && root.sidebarVisible) {
                                        root.sidebarVisible = false
                                    } else {
                                        root.activeSidebarTab = 1
                                        root.sidebarVisible = true
                                    }
                                }
                            }
                        }

                        // ── ICONO 3: SNIPPETS Y EXTENSIONES EEL2 ──────────────
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 48
                            color: "transparent"

                            Rectangle {
                                anchors.left: parent.left
                                anchors.verticalCenter: parent.verticalCenter
                                width: 2; height: 24
                                radius: 1
                                color: root.colAccent
                                visible: root.activeSidebarTab === 2 && root.sidebarVisible
                            }

                            // Icono de bloques de extensiones estilo VS Code
                            Item {
                                width: 20; height: 20
                                anchors.centerIn: parent
                                opacity: (root.activeSidebarTab === 2 && root.sidebarVisible) || actSnipArea.containsMouse ? 1.0 : 0.6

                                Rectangle { x: 1; y: 1; width: 8; height: 8; color: "transparent"; border.color: "#FFFFFF"; border.width: 1.4; radius: 1 }
                                Rectangle { x: 1; y: 11; width: 8; height: 8; color: "transparent"; border.color: "#FFFFFF"; border.width: 1.4; radius: 1 }
                                Rectangle { x: 11; y: 11; width: 8; height: 8; color: "transparent"; border.color: "#FFFFFF"; border.width: 1.4; radius: 1 }
                                Rectangle { x: 12; y: 0; width: 7; height: 7; color: "transparent"; border.color: "#FFFFFF"; border.width: 1.4; radius: 1; rotation: 15 }
                            }

                            MouseArea {
                                id: actSnipArea
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (root.activeSidebarTab === 2 && root.sidebarVisible) {
                                        root.sidebarVisible = false
                                    } else {
                                        root.activeSidebarTab = 2
                                        root.sidebarVisible = true
                                    }
                                }
                            }
                        }

                        Item { Layout.fillHeight: true }

                        // ── ICONO INFERIOR 1: TERMINAL / OUTPUT TOGGLE ────────
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 44
                            color: "transparent"

                            Rectangle {
                                width: 18; height: 15
                                anchors.centerIn: parent
                                radius: 2
                                color: "transparent"
                                border.color: "#FFFFFF"
                                border.width: 1.4
                                opacity: root.bottomPanelVisible || actTermArea.containsMouse ? 1.0 : 0.55

                                Text {
                                    x: 3; y: 1
                                    text: ">_"
                                    font.pixelSize: 9
                                    font.bold: true
                                    font.family: "Monospace"
                                    color: "#FFFFFF"
                                }
                            }

                            // Badge de error si falla la compilación
                            Rectangle {
                                visible: root.compilationError.length > 0
                                x: 27; y: 8
                                width: 8; height: 8
                                radius: 4
                                color: root.colError
                            }

                            MouseArea {
                                id: actTermArea
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.bottomPanelVisible = !root.bottomPanelVisible
                            }
                        }

                        // ── ICONO INFERIOR 2: SETTINGS GEAR (Command Palette) ─
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 44
                            color: "transparent"

                            Canvas {
                                width: 20; height: 20
                                anchors.centerIn: parent
                                opacity: actGearArea.containsMouse || root.commandPaletteVisible ? 1.0 : 0.55
                                onPaint: {
                                    var ctx = getContext("2d")
                                    ctx.clearRect(0, 0, width, height)
                                    ctx.strokeStyle = "#FFFFFF"
                                    ctx.lineWidth = 1.5
                                    ctx.beginPath()
                                    ctx.arc(10, 10, 4, 0, Math.PI * 2)
                                    ctx.stroke()
                                    for (var i = 0; i < 8; i++) {
                                        var angle = (i * Math.PI) / 4
                                        ctx.beginPath()
                                        ctx.moveTo(10 + Math.cos(angle) * 5.5, 10 + Math.sin(angle) * 5.5)
                                        ctx.lineTo(10 + Math.cos(angle) * 8.5, 10 + Math.sin(angle) * 8.5)
                                        ctx.stroke()
                                    }
                                }
                            }

                            MouseArea {
                                id: actGearArea
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    root.commandPaletteVisible = !root.commandPaletteVisible
                                    if (root.commandPaletteVisible) {
                                        cmdPaletteInput.text = ">"
                                        cmdPaletteInput.forceActiveFocus()
                                    }
                                }
                            }
                        }
                    }
                }
                // 2.2 SIDEBAR COLAPSABLE (VS Code Primary Side Bar)
                Rectangle {
                    id: sideBar
                    Layout.preferredWidth: root.sidebarVisible ? 250 : 0
                    Layout.fillHeight: true
                    visible: root.sidebarVisible
                    clip: true
                    color: root.colSideBar

                    Behavior on Layout.preferredWidth {
                        NumberAnimation { duration: 110; easing.type: Easing.OutCubic }
                    }

                    // Borde derecho divisor con el editor
                    Rectangle {
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        width: 1
                        color: root.colBorder
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.rightMargin: 1
                        spacing: 0

                        // ── CABECERA DE LA SIDEBAR ────────────────────────────
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 35
                            color: root.colSideBar

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 16
                                anchors.rightMargin: 10
                                spacing: 4

                                Text {
                                    text: {
                                        if (root.activeSidebarTab === 0) return "EXPLORADOR"
                                        if (root.activeSidebarTab === 1) return "PARÁMETROS DSP EN VIVO"
                                        return "REFERENCIA Y SNIPPETS EEL2"
                                    }
                                    color: "#BBBBBB"
                                    font.pixelSize: 11
                                    font.bold: true
                                    font.letterSpacing: 0.4
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                }

                                // Botón refrescar sliders (solo visible en pestaña de parámetros)
                                Rectangle {
                                    visible: root.activeSidebarTab === 1
                                    width: 22; height: 22; radius: 4
                                    color: refreshSlidersArea.containsMouse ? root.colHoverBg : "transparent"

                                    Text {
                                        anchors.centerIn: parent
                                        text: "↻"
                                        color: "#CCCCCC"
                                        font.pixelSize: 13
                                    }

                                    MouseArea {
                                        id: refreshSlidersArea
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: root.refreshParsedSliders()
                                    }
                                }

                                // Icono de tres puntos (...) estilo VS Code
                                Rectangle {
                                    width: 22; height: 22; radius: 4
                                    color: sideMoreArea.containsMouse ? root.colHoverBg : "transparent"

                                    Text {
                                        anchors.centerIn: parent
                                        text: "⋯"
                                        color: "#CCCCCC"
                                        font.pixelSize: 13
                                        font.bold: true
                                    }

                                    MouseArea {
                                        id: sideMoreArea
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: root.sidebarVisible = false
                                    }
                                }
                            }
                        }

                        // ── CONTENIDO DINÁMICO DE LA SIDEBAR ─────────────────
                        StackLayout {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            currentIndex: root.activeSidebarTab

                            // =================================================
                            // PESTAÑA 0: EXPLORADOR CON ACORDEONES VS CODE
                            // =================================================
                            ScrollView {
                                clip: true
                                contentWidth: availableWidth

                                ColumnLayout {
                                    width: parent.width
                                    spacing: 0

                                    // ── SECCIÓN 1: EDITORES ABIERTOS ──────────
                                    Rectangle {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 22
                                        color: openEdHdrArea.containsMouse ? root.colHoverBg : root.colSideBar

                                        RowLayout {
                                            anchors.fill: parent
                                            anchors.leftMargin: 6
                                            spacing: 4

                                            Text {
                                                text: root.openEditorsExpanded ? "▾" : "▸"
                                                color: "#CCCCCC"
                                                font.pixelSize: 11
                                            }
                                            Text {
                                                text: "EDITORES ABIERTOS"
                                                color: "#CCCCCC"
                                                font.pixelSize: 11
                                                font.bold: true
                                                Layout.fillWidth: true
                                            }
                                        }

                                        MouseArea {
                                            id: openEdHdrArea
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: root.openEditorsExpanded = !root.openEditorsExpanded
                                        }
                                    }

                                    // Item activo dentro de Editores Abiertos
                                    Rectangle {
                                        visible: root.openEditorsExpanded
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 22
                                        color: root.colListActiveBg

                                        RowLayout {
                                            anchors.fill: parent
                                            anchors.leftMargin: 18
                                            anchors.rightMargin: 8
                                            spacing: 6

                                            Text {
                                                text: root.isModified ? "●" : "×"
                                                color: root.isModified ? root.colDirtyDot : "#858585"
                                                font.pixelSize: 10
                                            }

                                            Text {
                                                text: "JS"
                                                color: "#CBCB41"
                                                font.pixelSize: 10
                                                font.bold: true
                                                font.family: "Monospace"
                                            }

                                            Text {
                                                text: root.isNewPlugin ? "nuevo_script.jsfx" : (root.pluginName.toLowerCase().replace(/ /g, "_") + ".jsfx")
                                                color: "#FFFFFF"
                                                font.pixelSize: 12
                                                elide: Text.ElideRight
                                                Layout.fillWidth: true
                                            }

                                            Text {
                                                text: "dsp/jsfx"
                                                color: "#858585"
                                                font.pixelSize: 10
                                            }
                                        }
                                    }

                                    // Divisor de acordeón
                                    Rectangle { Layout.fillWidth: true; height: 1; color: root.colBorder }

                                    // ── SECCIÓN 2: PLANTILLAS DSP LISTAS ──────
                                    Rectangle {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 22
                                        color: tplHdrArea.containsMouse ? root.colHoverBg : root.colSideBar

                                        RowLayout {
                                            anchors.fill: parent
                                            anchors.leftMargin: 6
                                            anchors.rightMargin: 8
                                            spacing: 4

                                            Text {
                                                text: root.templatesExpanded ? "▾" : "▸"
                                                color: "#CCCCCC"
                                                font.pixelSize: 11
                                            }
                                            Text {
                                                text: "PLANTILLAS DSP"
                                                color: "#CCCCCC"
                                                font.pixelSize: 11
                                                font.bold: true
                                                Layout.fillWidth: true
                                            }
                                            Text {
                                                text: "5"
                                                color: "#858585"
                                                font.pixelSize: 10
                                            }
                                        }

                                        MouseArea {
                                            id: tplHdrArea
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: root.templatesExpanded = !root.templatesExpanded
                                        }
                                    }

                                    // Lista de plantillas con guía de indentación vertical
                                    ColumnLayout {
                                        visible: root.templatesExpanded
                                        Layout.fillWidth: true
                                        spacing: 0

                                        Repeater {
                                            model: [
                                                { name: "tube_saturator.jsfx", title: "Saturador de Tubos", fn: function() { root.loadTemplateTube() }, desc: "Triodo analógico" },
                                                { name: "resonant_lpf.jsfx",   title: "Filtro Pasa-Bajos Q", fn: function() { root.loadTemplateFilter() }, desc: "Biquad 12dB/oct" },
                                                { name: "stereo_widener.jsfx", title: "Stereo Widener M/S", fn: function() { root.loadTemplateWidener() }, desc: "Matriz Mid/Side" },
                                                { name: "stereo_delay.jsfx",   title: "Delay Estéreo",      fn: function() { root.loadTemplateDelay() }, desc: "Línea de retardo" },
                                                { name: "basic_gain.jsfx",     title: "Ganancia Básica",    fn: function() { root.loadTemplateGain() }, desc: "Controlador dB" }
                                            ]

                                            delegate: Rectangle {
                                                Layout.fillWidth: true
                                                Layout.preferredHeight: 26
                                                color: tplMouse.containsMouse ? root.colHoverBg : "transparent"

                                                // Guía vertical de árbol estilo VS Code
                                                Rectangle {
                                                    x: 12
                                                    anchors.top: parent.top
                                                    anchors.bottom: parent.bottom
                                                    width: 1
                                                    color: "#333333"
                                                }

                                                RowLayout {
                                                    anchors.fill: parent
                                                    anchors.leftMargin: 20
                                                    anchors.rightMargin: 8
                                                    spacing: 6

                                                    Text {
                                                        text: "≡"
                                                        color: "#519ABA"
                                                        font.pixelSize: 12
                                                        font.bold: true
                                                        font.family: "Monospace"
                                                    }

                                                    Text {
                                                        text: modelData.title
                                                        color: tplMouse.containsMouse ? "#FFFFFF" : "#CCCCCC"
                                                        font.pixelSize: 12
                                                        Layout.fillWidth: true
                                                        elide: Text.ElideRight
                                                    }

                                                    Text {
                                                        text: modelData.desc
                                                        color: "#6E6E6E"
                                                        font.pixelSize: 10
                                                    }
                                                }

                                                MouseArea {
                                                    id: tplMouse
                                                    anchors.fill: parent
                                                    hoverEnabled: true
                                                    cursorShape: Qt.PointingHandCursor
                                                    onClicked: modelData.fn()
                                                }
                                            }
                                        }
                                    }

                                    // Divisor de acordeón
                                    Rectangle { Layout.fillWidth: true; height: 1; color: root.colBorder }

                                    // ── SECCIÓN 3: ESQUEMA (OUTLINE EEL2) ─────
                                    Rectangle {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 22
                                        color: outlineHdrArea.containsMouse ? root.colHoverBg : root.colSideBar

                                        RowLayout {
                                            anchors.fill: parent
                                            anchors.leftMargin: 6
                                            spacing: 4

                                            Text {
                                                text: root.outlineExpanded ? "▾" : "▸"
                                                color: "#CCCCCC"
                                                font.pixelSize: 11
                                            }
                                            Text {
                                                text: "ESQUEMA (OUTLINE)"
                                                color: "#CCCCCC"
                                                font.pixelSize: 11
                                                font.bold: true
                                                Layout.fillWidth: true
                                            }
                                        }

                                        MouseArea {
                                            id: outlineHdrArea
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: root.outlineExpanded = !root.outlineExpanded
                                        }
                                    }

                                    ColumnLayout {
                                        visible: root.outlineExpanded
                                        Layout.fillWidth: true
                                        spacing: 0

                                        Repeater {
                                            model: [
                                                { sym: "@init",   kind: "fn", color: "#B180D7", desc: "Inicialización DSP" },
                                                { sym: "@slider", kind: "ev", color: "#EE9D28", desc: "Eventos de parámetros" },
                                                { sym: "@block",  kind: "bk", color: "#75BEFF", desc: "Procesamiento por bloque" },
                                                { sym: "@sample", kind: "sp", color: "#4EC9B0", desc: "Bucle de audio por muestra" }
                                            ]

                                            delegate: Rectangle {
                                                Layout.fillWidth: true
                                                Layout.preferredHeight: 23
                                                color: outSymArea.containsMouse ? root.colHoverBg : "transparent"

                                                Rectangle {
                                                    x: 12; width: 1; height: parent.height
                                                    color: "#333333"
                                                }

                                                RowLayout {
                                                    anchors.fill: parent
                                                    anchors.leftMargin: 20
                                                    anchors.rightMargin: 8
                                                    spacing: 6

                                                    Rectangle {
                                                        width: 14; height: 14; radius: 2
                                                        color: "transparent"
                                                        border.color: modelData.color
                                                        border.width: 1.2
                                                        Text {
                                                            anchors.centerIn: parent
                                                            text: "{}"
                                                            color: modelData.color
                                                            font.pixelSize: 8
                                                            font.bold: true
                                                        }
                                                    }

                                                    Text {
                                                        text: modelData.sym
                                                        color: root.currentSection === modelData.sym ? "#FFFFFF" : "#CCCCCC"
                                                        font.pixelSize: 12
                                                        font.bold: root.currentSection === modelData.sym
                                                        font.family: "Monospace, Consolas"
                                                        Layout.fillWidth: true
                                                    }

                                                    Text {
                                                        text: modelData.desc
                                                        color: "#6E6E6E"
                                                        font.pixelSize: 10
                                                    }
                                                }

                                                MouseArea {
                                                    id: outSymArea
                                                    anchors.fill: parent
                                                    hoverEnabled: true
                                                    cursorShape: Qt.PointingHandCursor
                                                    onClicked: root.findInEditor(modelData.sym)
                                                }
                                            }
                                        }
                                    }
                                }
                            }

                            // =================================================
                            // PESTAÑA 1: PARÁMETROS EN VIVO (SLIDERS VS CODE)
                            // =================================================
                            ScrollView {
                                clip: true
                                contentWidth: availableWidth

                                ColumnLayout {
                                    width: parent.width
                                    spacing: 10
                                    anchors.leftMargin: 12
                                    anchors.rightMargin: 12
                                    anchors.topMargin: 8

                                    Rectangle {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 34
                                        radius: 4
                                        color: "#252526"
                                        border.color: "#333333"
                                        border.width: 1

                                        Text {
                                            anchors.fill: parent
                                            anchors.margins: 6
                                            text: "Ajusta los controles en tiempo real sobre el motor de audio:"
                                            color: "#9CDCFE"
                                            font.pixelSize: 10
                                            wrapMode: Text.WordWrap
                                            verticalAlignment: Text.AlignVCenter
                                        }
                                    }

                                    Repeater {
                                        model: root.parsedSliders

                                        delegate: Rectangle {
                                            Layout.fillWidth: true
                                            Layout.preferredHeight: 54
                                            radius: 4
                                            color: "#222222"
                                            border.color: "#2E2E2E"
                                            border.width: 1

                                            ColumnLayout {
                                                anchors.fill: parent
                                                anchors.margins: 8
                                                spacing: 4

                                                RowLayout {
                                                    Layout.fillWidth: true
                                                    spacing: 4

                                                    Text {
                                                        text: "slider" + modelData.index
                                                        color: "#569CD6"
                                                        font.pixelSize: 10
                                                        font.family: "Monospace"
                                                    }

                                                    Text {
                                                        text: modelData.name
                                                        color: "#CCCCCC"
                                                        font.pixelSize: 11
                                                        font.bold: true
                                                        Layout.fillWidth: true
                                                        elide: Text.ElideRight
                                                    }

                                                    // Badge numérico estilo input de VS Code
                                                    Rectangle {
                                                        width: valTxt.implicitWidth + 10
                                                        height: 18
                                                        radius: 3
                                                        color: root.colInputBg
                                                        border.color: "#3C3C3C"
                                                        border.width: 1

                                                        Text {
                                                            id: valTxt
                                                            anchors.centerIn: parent
                                                            text: Number(sliderCtrl.value).toFixed(1)
                                                            color: "#4EC9B0"
                                                            font.pixelSize: 10
                                                            font.family: "Monospace, Consolas"
                                                        }
                                                    }
                                                }

                                                Slider {
                                                    id: sliderCtrl
                                                    Layout.fillWidth: true
                                                    Layout.preferredHeight: 16
                                                    from: modelData.min
                                                    to: modelData.max
                                                    stepSize: modelData.step || 0.1
                                                    value: modelData.value

                                                    background: Rectangle {
                                                        x: sliderCtrl.leftPadding
                                                        y: sliderCtrl.topPadding + sliderCtrl.availableHeight / 2 - height / 2
                                                        implicitWidth: 180
                                                        implicitHeight: 4
                                                        width: sliderCtrl.availableWidth
                                                        height: implicitHeight
                                                        radius: 2
                                                        color: "#3C3C3C"

                                                        Rectangle {
                                                            width: sliderCtrl.visualPosition * parent.width
                                                            height: parent.height
                                                            color: root.colAccent
                                                            radius: 2
                                                        }
                                                    }

                                                    handle: Rectangle {
                                                        x: sliderCtrl.leftPadding + sliderCtrl.visualPosition * (sliderCtrl.availableWidth - width)
                                                        y: sliderCtrl.topPadding + sliderCtrl.availableHeight / 2 - height / 2
                                                        implicitWidth: 12
                                                        implicitHeight: 12
                                                        radius: 6
                                                        color: sliderCtrl.pressed ? "#FFFFFF" : "#CCCCCC"
                                                        border.color: root.colAccent
                                                        border.width: 2
                                                    }

                                                    onMoved: {
                                                        if (typeof AudioEngine !== "undefined" && AudioEngine.tracks && root.trackIndex >= 0 && root.pluginIndex >= 0) {
                                                            AudioEngine.tracks.setPluginParameter(root.trackIndex, root.pluginIndex, modelData.index, sliderCtrl.value)
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }

                                    Text {
                                        visible: root.parsedSliders.length === 0
                                        text: "No se detectaron sliders definidos.\nUsa el formato:\nslider1:0<-24,24,0.1>Ganancia (dB)"
                                        color: "#858585"
                                        font.pixelSize: 11
                                        font.family: "Monospace"
                                        wrapMode: Text.WordWrap
                                        Layout.fillWidth: true
                                    }
                                }
                            }

                            // =================================================
                            // PESTAÑA 2: REFERENCIA Y SNIPPETS EEL2
                            // =================================================
                            ScrollView {
                                clip: true
                                contentWidth: availableWidth

                                ColumnLayout {
                                    width: parent.width
                                    spacing: 6
                                    anchors.leftMargin: 10
                                    anchors.rightMargin: 10
                                    anchors.topMargin: 8

                                    Text {
                                        text: "Haz clic en un bloque para insertarlo en el cursor:"
                                        color: "#858585"
                                        font.pixelSize: 11
                                        wrapMode: Text.WordWrap
                                        Layout.fillWidth: true
                                    }

                                    Repeater {
                                        model: [
                                            { label: "@init", badge: "Sección", snippet: "@init\n// Inicialización de variables y tablas\n", desc: "Se ejecuta al iniciar o cambiar srate" },
                                            { label: "@slider", badge: "Evento", snippet: "@slider\n// Recalcular coeficientes dependientes de sliders\n", desc: "Se ejecuta cuando un slider cambia" },
                                            { label: "@sample", badge: "Audio DSP", snippet: "@sample\nspl0 = spl0 * gain;\nspl1 = spl1 * gain;\n", desc: "Procesa cada muestra estéreo" },
                                            { label: "Biquad Lowpass", badge: "Filtro", snippet: "w0 = 2 * $pi * freq / srate;\nalpha = sin(w0) / (2 * q);\nb0 = (1 - cos(w0)) * 0.5;\n", desc: "Cálculo de filtro resonante rápido" },
                                            { label: "Saturación Tanh", badge: "Clipper", snippet: "spl0 = tanh(spl0 * drive);\nspl1 = tanh(spl1 * drive);\n", desc: "Distorsión armónica suave" },
                                            { label: "Nuevo Slider", badge: "UI", snippet: "slider1:0<-24,24,0.1>Parámetro (dB)\n", desc: "Declara un parámetro automatizable" }
                                        ]

                                        delegate: Rectangle {
                                            Layout.fillWidth: true
                                            Layout.preferredHeight: 46
                                            radius: 4
                                            color: snipArea.containsMouse ? root.colHoverBg : "#232323"
                                            border.color: snipArea.containsMouse ? "#3C3C3C" : "#2B2B2B"
                                            border.width: 1

                                            RowLayout {
                                                anchors.fill: parent
                                                anchors.leftMargin: 10
                                                anchors.rightMargin: 10
                                                spacing: 8

                                                ColumnLayout {
                                                    spacing: 2
                                                    Layout.fillWidth: true

                                                    RowLayout {
                                                        spacing: 6
                                                        Text {
                                                            text: modelData.label
                                                            color: "#9CDCFE"
                                                            font.pixelSize: 11
                                                            font.bold: true
                                                            font.family: "Monospace, Consolas"
                                                        }
                                                        Rectangle {
                                                            width: bdgTxt.implicitWidth + 8
                                                            height: 14
                                                            radius: 2
                                                            color: "#04395E"
                                                            Text {
                                                                id: bdgTxt
                                                                anchors.centerIn: parent
                                                                text: modelData.badge
                                                                color: "#75BEFF"
                                                                font.pixelSize: 9
                                                            }
                                                        }
                                                    }

                                                    Text {
                                                        text: modelData.desc
                                                        color: "#858585"
                                                        font.pixelSize: 10
                                                        elide: Text.ElideRight
                                                        Layout.fillWidth: true
                                                    }
                                                }

                                                Rectangle {
                                                    width: 20; height: 20; radius: 3
                                                    color: snipArea.containsMouse ? root.colAccent : "#313131"
                                                    Text {
                                                        anchors.centerIn: parent
                                                        text: "+"
                                                        color: "#FFFFFF"
                                                        font.pixelSize: 13
                                                        font.bold: true
                                                    }
                                                }
                                            }

                                            MouseArea {
                                                id: snipArea
                                                anchors.fill: parent
                                                hoverEnabled: true
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: {
                                                    codeEditor.insert(codeEditor.cursorPosition, modelData.snippet)
                                                    root.refreshParsedSliders()
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // 2.3 ÁREA CENTRAL DEL EDITOR (Tabs + Breadcrumbs + Text Editor + Panel)
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: root.colEditorBg

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 0

                        // ── PESTAÑAS SUPERIORES (VS Code Editor Group Header) ────────
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 35
                            color: root.colEditorGroupHdr

                            // Línea inferior del contenedor de pestañas
                            Rectangle {
                                anchors.bottom: parent.bottom
                                anchors.left: parent.left
                                anchors.right: parent.right
                                height: 1
                                color: root.colBorder
                            }

                            RowLayout {
                                anchors.fill: parent
                                spacing: 0

                                // Pestaña Activa estilo VS Code Dark Modern
                                Rectangle {
                                    Layout.preferredWidth: Math.min(240, tabLabel.implicitWidth + 68)
                                    Layout.fillHeight: true
                                    color: root.colTabActive

                                    // Borde superior azul de pestaña activa (#0078D4)
                                    Rectangle {
                                        anchors.top: parent.top
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        height: 2
                                        color: root.colAccent
                                    }

                                    // Borde derecho de la pestaña
                                    Rectangle {
                                        anchors.right: parent.right
                                        anchors.top: parent.top
                                        anchors.bottom: parent.bottom
                                        width: 1
                                        color: root.colBorder
                                    }

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 12
                                        anchors.rightMargin: 8
                                        spacing: 6

                                        // Icono JSFX
                                        Text {
                                            text: "JS"
                                            color: "#CBCB41"
                                            font.pixelSize: 11
                                            font.bold: true
                                            font.family: "Monospace"
                                        }

                                        Text {
                                            id: tabLabel
                                            text: root.isNewPlugin ? "nuevo_script.jsfx" : (root.pluginName.toLowerCase().replace(/ /g, "_") + ".jsfx")
                                            color: root.compilationError.length > 0 ? root.colError : "#FFFFFF"
                                            font.pixelSize: 12
                                            Layout.fillWidth: true
                                            elide: Text.ElideRight
                                        }

                                        // Indicador de modificado (●) que cambia a (✕) en hover
                                        Rectangle {
                                            width: 18; height: 18; radius: 3
                                            color: tabCloseArea.containsMouse ? "#313131" : "transparent"

                                            Text {
                                                anchors.centerIn: parent
                                                text: (!tabCloseArea.containsMouse && root.isModified) ? "●" : "✕"
                                                color: (!tabCloseArea.containsMouse && root.isModified) ? root.colDirtyDot : "#CCCCCC"
                                                font.pixelSize: (!tabCloseArea.containsMouse && root.isModified) ? 11 : 10
                                            }

                                            MouseArea {
                                                id: tabCloseArea
                                                anchors.fill: parent
                                                hoverEnabled: true
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: root.closeDialog()
                                            }
                                        }
                                    }
                                }

                                Item { Layout.fillWidth: true }

                                // ── Editor Actions Toolbar (Esquina derecha de las Tabs) ──
                                Row {
                                    spacing: 4
                                    Layout.rightMargin: 8

                                    // Botón Toggle Word Wrap (Alt+Z)
                                    Rectangle {
                                        height: 24
                                        width: wrapBtnTxt.implicitWidth + 14
                                        radius: 4
                                        color: wrapArea.containsMouse ? root.colHoverBg : (root.wordWrapEnabled ? "#04395E" : "transparent")

                                        Text {
                                            id: wrapBtnTxt
                                            anchors.centerIn: parent
                                            text: "↩ Ajuste"
                                            color: root.wordWrapEnabled ? "#75BEFF" : "#CCCCCC"
                                            font.pixelSize: 11
                                        }

                                        MouseArea {
                                            id: wrapArea
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: root.wordWrapEnabled = !root.wordWrapEnabled
                                        }
                                    }

                                    // Botón Revertir cambios
                                    Rectangle {
                                        height: 24
                                        width: revertTxt.implicitWidth + 14
                                        radius: 4
                                        color: revertArea.containsMouse ? root.colHoverBg : "transparent"

                                        Text {
                                            id: revertTxt
                                            anchors.centerIn: parent
                                            text: "↺ Revertir"
                                            color: "#CCCCCC"
                                            font.pixelSize: 11
                                        }

                                        MouseArea {
                                            id: revertArea
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                if (root.isNewPlugin) root.loadTemplateTube()
                                                else root.openForPlugin(root.trackIndex, root.pluginIndex, root.pluginName)
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        // ── BREADCRUMBS BAR INTERACTIVA (VS Code Breadcrumbs) ────────
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 22
                            color: root.colEditorBg

                            Rectangle {
                                anchors.bottom: parent.bottom
                                anchors.left: parent.left
                                anchors.right: parent.right
                                height: 1
                                color: "#262626"
                            }

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 14
                                anchors.rightMargin: 14
                                spacing: 4

                                Text { text: "nova_studio"; color: "#858585"; font.pixelSize: 11 }
                                Text { text: "›"; color: "#555555"; font.pixelSize: 12 }
                                Text { text: "dsp"; color: "#858585"; font.pixelSize: 11 }
                                Text { text: "›"; color: "#555555"; font.pixelSize: 12 }
                                Text { text: "jsfx"; color: "#858585"; font.pixelSize: 11 }
                                Text { text: "›"; color: "#555555"; font.pixelSize: 12 }

                                // Nombre del archivo en Breadcrumb
                                Row {
                                    spacing: 4
                                    Text { text: "JS"; color: "#CBCB41"; font.pixelSize: 10; font.bold: true; font.family: "Monospace" }
                                    Text {
                                        text: root.isNewPlugin ? "nuevo_script.jsfx" : (root.pluginName.toLowerCase().replace(/ /g, "_") + ".jsfx")
                                        color: "#CCCCCC"
                                        font.pixelSize: 11
                                    }
                                }

                                Text { text: "›"; color: "#555555"; font.pixelSize: 12 }

                                // Símbolo activo (@init / @slider / @sample)
                                Rectangle {
                                    width: symRow.implicitWidth + 10
                                    height: 18
                                    radius: 3
                                    color: bcSymArea.containsMouse ? root.colHoverBg : "transparent"

                                    Row {
                                        id: symRow
                                        anchors.centerIn: parent
                                        spacing: 4

                                        Rectangle {
                                            width: 12; height: 12; radius: 2
                                            anchors.verticalCenter: parent.verticalCenter
                                            color: "transparent"
                                            border.color: "#4EC9B0"
                                            border.width: 1
                                            Text {
                                                anchors.centerIn: parent
                                                text: "{}"
                                                color: "#4EC9B0"
                                                font.pixelSize: 7
                                                font.bold: true
                                            }
                                        }

                                        Text {
                                            text: root.currentSection
                                            color: "#E0E0E0"
                                            font.pixelSize: 11
                                            font.family: "Monospace, Consolas"
                                        }
                                    }

                                    MouseArea {
                                        id: bcSymArea
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: root.findInEditor(root.currentSection)
                                    }
                                }

                                Item { Layout.fillWidth: true }
                            }
                        }
                        // ── ÁREA DE CÓDIGO MONOESPACIADA CON GUTTER Y MINIMAP ────────
                        Rectangle {
                            id: editorCanvasContainer
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            color: root.colEditorBg
                            clip: true

                            RowLayout {
                                anchors.fill: parent
                                spacing: 0

                                // ScrollView principal con Gutter, Resaltado de Línea y Editor
                                ScrollView {
                                    id: editorScrollView
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    clip: true

                                    Row {
                                        width: Math.max(editorScrollView.width, codeEditor.contentWidth + 68)
                                        height: Math.max(codeEditor.contentHeight + 48, editorScrollView.height)
                                        spacing: 0

                                        // ── GUTTER: NÚMEROS DE LÍNEA + CHEVRONS DE SECCIÓN ──
                                        Rectangle {
                                            id: lineGutter
                                            width: 56
                                            height: parent.height
                                            color: root.colEditorBg

                                            Column {
                                                anchors.fill: parent
                                                anchors.topMargin: 6

                                                Repeater {
                                                    model: Math.max(1, codeEditor.lineCount)
                                                    delegate: Item {
                                                        width: lineGutter.width
                                                        height: codeEditor.cursorRectangle.height > 0 ? codeEditor.cursorRectangle.height : 17

                                                        readonly property string rawLineText: {
                                                            var arr = codeEditor.text.split("\n")
                                                            return index < arr.length ? arr[index].trim() : ""
                                                        }
                                                        readonly property bool isSectionHeader: rawLineText.indexOf("@") === 0

                                                        // Indicador de línea actual en el gutter
                                                        Rectangle {
                                                            anchors.fill: parent
                                                            color: (index + 1 === root.currentLine) ? "#282828" : "transparent"
                                                        }

                                                        // Número de línea alineado a la derecha
                                                        Text {
                                                            width: 36
                                                            anchors.verticalCenter: parent.verticalCenter
                                                            text: (index + 1).toString()
                                                            color: (index + 1 === root.currentLine) ? "#CCCCCC" : "#6E7681"
                                                            font.family: "JetBrains Mono, Consolas, Monospace"
                                                            font.pixelSize: 12
                                                            font.bold: (index + 1 === root.currentLine)
                                                            horizontalAlignment: Text.AlignRight
                                                        }

                                                        // Chevron de plegado (▾) en cabeceras @init, @slider, @sample
                                                        Text {
                                                            visible: parent.isSectionHeader
                                                            x: 41
                                                            anchors.verticalCenter: parent.verticalCenter
                                                            text: "▾"
                                                            color: "#858585"
                                                            font.pixelSize: 10
                                                        }
                                                    }
                                                }
                                            }

                                            // Línea divisoria sutil del gutter
                                            Rectangle {
                                                anchors.right: parent.right
                                                anchors.top: parent.top
                                                anchors.bottom: parent.bottom
                                                width: 1
                                                color: "#262626"
                                            }
                                        }

                                        // ── LIENZO DE EDICIÓN DE TEXTO CON RESALTADO DE LÍNEA ──
                                        Item {
                                            width: Math.max(editorScrollView.width - 56, codeEditor.contentWidth + 90)
                                            height: parent.height

                                            // Resaltado de línea activa estilo VS Code
                                            Rectangle {
                                                x: 0
                                                y: codeEditor.cursorRectangle.y
                                                width: parent.width
                                                height: codeEditor.cursorRectangle.height > 0 ? codeEditor.cursorRectangle.height : 18
                                                color: "#262626"
                                                border.color: "#303031"
                                                border.width: 1
                                                visible: codeEditor.activeFocus
                                            }

                                            // Guías verticales de indentación (cada 4 espacios)
                                            Row {
                                                x: 12 + 32
                                                y: 6
                                                height: codeEditor.contentHeight
                                                spacing: 31
                                                opacity: 0.25

                                                Repeater {
                                                    model: 4
                                                    delegate: Rectangle {
                                                        width: 1
                                                        height: codeEditor.contentHeight
                                                        color: "#404040"
                                                    }
                                                }
                                            }

                                            // Editor de código fuente EEL2
                                            TextEdit {
                                                id: codeEditor
                                                width: root.wordWrapEnabled ? (editorScrollView.width - 60) : Math.max(editorScrollView.width - 56, contentWidth + 90)
                                                height: Math.max(contentHeight + 48, editorScrollView.height)
                                                color: "#D4D4D4"
                                                selectionColor: "#264F78"
                                                selectedTextColor: "#FFFFFF"
                                                font.family: "JetBrains Mono, Consolas, 'Courier New', Monospace"
                                                font.pixelSize: 12
                                                leftPadding: 12
                                                topPadding: 6
                                                wrapMode: root.wordWrapEnabled ? TextEdit.Wrap : TextEdit.NoWrap
                                                tabStopDistance: 32
                                                cursorVisible: true
                                                selectByMouse: true

                                                // Cursor personalizado de 2px estilo VS Code
                                                cursorDelegate: Rectangle {
                                                    width: 2
                                                    color: "#AEAFAD"
                                                    visible: codeEditor.activeFocus

                                                    SequentialAnimation on opacity {
                                                        loops: Animation.Infinite
                                                        running: codeEditor.activeFocus
                                                        NumberAnimation { to: 1.0; duration: 100 }
                                                        PauseAnimation { duration: 400 }
                                                        NumberAnimation { to: 0.0; duration: 150 }
                                                        PauseAnimation { duration: 300 }
                                                    }
                                                }

                                                onCursorPositionChanged: {
                                                    root.updateCursorPosition()
                                                }

                                                onSelectedTextChanged: {
                                                    root.selectedChars = selectedText.length
                                                }

                                                onTextChanged: {
                                                    root.isModified = true
                                                    root.updateCursorPosition()
                                                }

                                                Component.onCompleted: {
                                                    if (text.length === 0) root.loadTemplateTube()
                                                    root.updateCursorPosition()
                                                }
                                            }
                                        }
                                    }
                                }

                                // ── MINIMAPA LATERAL INTERACTIVO ESTILO VS CODE ──────────
                                Rectangle {
                                    Layout.preferredWidth: root.minimapVisible ? 64 : 0
                                    Layout.fillHeight: true
                                    visible: root.minimapVisible
                                    color: root.colEditorBg
                                    clip: true

                                    // Sombra izquierda del minimapa
                                    Rectangle {
                                        anchors.left: parent.left
                                        anchors.top: parent.top
                                        anchors.bottom: parent.bottom
                                        width: 1
                                        color: "#252526"
                                    }

                                    // Representación de líneas coloreadas según sintaxis EEL2
                                    Column {
                                        anchors.fill: parent
                                        anchors.topMargin: 6
                                        anchors.leftMargin: 8
                                        anchors.rightMargin: 6
                                        spacing: 2
                                        opacity: minimapArea.containsMouse ? 0.75 : 0.45

                                        Behavior on opacity { NumberAnimation { duration: 120 } }

                                        Repeater {
                                            model: Math.min(90, codeEditor.lineCount)
                                            delegate: Item {
                                                width: 48
                                                height: 2

                                                readonly property string lnText: {
                                                    var arr = codeEditor.text.split("\n")
                                                    return index < arr.length ? arr[index].trim() : ""
                                                }

                                                Rectangle {
                                                    visible: parent.lnText.length > 0
                                                    x: parent.lnText.indexOf("@") === 0 ? 0 : (parent.lnText.indexOf("slider") === 0 ? 2 : 6)
                                                    width: Math.min(44, Math.max(8, parent.lnText.length * 1.1))
                                                    height: 2
                                                    radius: 1
                                                    color: {
                                                        if (parent.lnText.indexOf("@") === 0) return "#4EC9B0"
                                                        if (parent.lnText.indexOf("slider") === 0) return "#569CD6"
                                                        if (parent.lnText.indexOf("//") === 0) return "#6A9955"
                                                        if (parent.lnText.indexOf("desc:") === 0) return "#CE9178"
                                                        return "#9CDCFE"
                                                    }
                                                }
                                            }
                                        }
                                    }

                                    // Slider del viewport en el minimapa
                                    Rectangle {
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        y: Math.min(parent.height - 38, Math.max(0, ((root.currentLine - 1) / Math.max(1, codeEditor.lineCount)) * (parent.height - 38)))
                                        height: 38
                                        color: minimapArea.containsMouse ? "#387990AA" : "#22FFFFFF"
                                        border.color: minimapArea.containsMouse ? "#550078D4" : "#33FFFFFF"
                                        border.width: 1
                                    }

                                    // Clic en el minimapa para saltar a esa línea
                                    MouseArea {
                                        id: minimapArea
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: function(mouse) {
                                            var ratio = Math.max(0, Math.min(1, mouse.y / height))
                                            var targetLine = Math.floor(ratio * codeEditor.lineCount)
                                            var lines = codeEditor.text.split("\n")
                                            var charPos = 0
                                            for (var i = 0; i < Math.min(targetLine, lines.length); i++) {
                                                charPos += lines[i].length + 1
                                            }
                                            codeEditor.cursorPosition = Math.min(codeEditor.text.length, charPos)
                                            codeEditor.forceActiveFocus()
                                        }
                                    }
                                }
                            }

                            // ── WIDGET FLOTANTE DE BÚSQUEDA Y REEMPLAZO (Ctrl+F / Ctrl+H) ──
                            Rectangle {
                                id: searchFloatBox
                                visible: root.searchBarVisible
                                anchors.top: parent.top
                                anchors.right: parent.right
                                anchors.rightMargin: root.minimapVisible ? 76 : 16
                                anchors.topMargin: 0
                                width: 355
                                height: root.replaceVisible ? 68 : 36
                                radius: 4
                                color: root.colWidgetBg
                                border.color: "#454545"
                                border.width: 1
                                z: 60

                                Behavior on height { NumberAnimation { duration: 90 } }

                                // Barra decorativa izquierda estilo widget de VS Code
                                Rectangle {
                                    anchors.left: parent.left
                                    anchors.top: parent.top
                                    anchors.bottom: parent.bottom
                                    width: 3
                                    color: root.colAccent
                                    radius: 2
                                }

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 6
                                    anchors.rightMargin: 6
                                    anchors.topMargin: 4
                                    anchors.bottomMargin: 4
                                    spacing: 4

                                    // Botón lateral para expandir/contraer Reemplazar (▸ / ▾)
                                    Rectangle {
                                        Layout.preferredWidth: 18
                                        Layout.fillHeight: true
                                        radius: 3
                                        color: toggleRepArea.containsMouse ? root.colHoverBg : "transparent"

                                        Text {
                                            anchors.centerIn: parent
                                            text: root.replaceVisible ? "▾" : "▸"
                                            color: "#CCCCCC"
                                            font.pixelSize: 11
                                        }

                                        MouseArea {
                                            id: toggleRepArea
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: root.replaceVisible = !root.replaceVisible
                                        }
                                    }

                                    // Columna con fila de Buscar y fila de Reemplazar
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        spacing: 4

                                        // Fila 1: Buscar
                                        RowLayout {
                                            Layout.fillWidth: true
                                            spacing: 4

                                            // Caja de entrada con botón Aa integrado
                                            Rectangle {
                                                Layout.fillWidth: true
                                                Layout.preferredHeight: 24
                                                radius: 3
                                                color: root.colInputBg
                                                border.color: searchField.activeFocus ? root.colAccent : "#3C3C3C"
                                                border.width: 1

                                                RowLayout {
                                                    anchors.fill: parent
                                                    anchors.leftMargin: 6
                                                    anchors.rightMargin: 3
                                                    spacing: 4

                                                    TextInput {
                                                        id: searchField
                                                        Layout.fillWidth: true
                                                        color: "#FFFFFF"
                                                        font.pixelSize: 12
                                                        font.family: "Segoe UI, Inter, sans-serif"
                                                        selectByMouse: true
                                                        clip: true

                                                        Text {
                                                            anchors.fill: parent
                                                            text: "Buscar"
                                                            color: "#858585"
                                                            font.pixelSize: 12
                                                            visible: searchField.text.length === 0
                                                        }

                                                        onTextChanged: root.findInEditor(text)
                                                        onAccepted: root.findNext()
                                                    }

                                                    // Toggle Match Case (Aa)
                                                    Rectangle {
                                                        width: 20; height: 18; radius: 2
                                                        color: root.matchCase ? "#04395E" : (caseArea.containsMouse ? "#3C3C3C" : "transparent")
                                                        border.color: root.matchCase ? root.colAccent : "transparent"
                                                        border.width: 1

                                                        Text {
                                                            anchors.centerIn: parent
                                                            text: "Aa"
                                                            color: root.matchCase ? "#FFFFFF" : "#CCCCCC"
                                                            font.pixelSize: 10
                                                            font.bold: true
                                                        }

                                                        MouseArea {
                                                            id: caseArea
                                                            anchors.fill: parent
                                                            hoverEnabled: true
                                                            cursorShape: Qt.PointingHandCursor
                                                            onClicked: {
                                                                root.matchCase = !root.matchCase
                                                                root.findInEditor(searchField.text)
                                                            }
                                                        }
                                                    }
                                                }
                                            }

                                            // Contador de resultados
                                            Text {
                                                Layout.preferredWidth: 56
                                                text: searchField.text.length === 0 ? "Sin res." :
                                                      (root.searchMatchesCount > 0 ? (root.currentMatchIndex + " de " + root.searchMatchesCount) : "0 res.")
                                                color: (searchField.text.length > 0 && root.searchMatchesCount === 0) ? root.colError : "#CCCCCC"
                                                font.pixelSize: 11
                                                elide: Text.ElideRight
                                            }

                                            // Anterior (↑)
                                            Rectangle {
                                                width: 22; height: 22; radius: 3
                                                color: prevArea.containsMouse ? root.colHoverBg : "transparent"
                                                Text { anchors.centerIn: parent; text: "↑"; color: "#CCCCCC"; font.pixelSize: 12 }
                                                MouseArea {
                                                    id: prevArea; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                                    onClicked: root.findPrev()
                                                }
                                            }

                                            // Siguiente (↓)
                                            Rectangle {
                                                width: 22; height: 22; radius: 3
                                                color: nextArea.containsMouse ? root.colHoverBg : "transparent"
                                                Text { anchors.centerIn: parent; text: "↓"; color: "#CCCCCC"; font.pixelSize: 12 }
                                                MouseArea {
                                                    id: nextArea; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                                    onClicked: root.findNext()
                                                }
                                            }

                                            // Cerrar (✕)
                                            Rectangle {
                                                width: 22; height: 22; radius: 3
                                                color: closeSearchArea.containsMouse ? root.colHoverBg : "transparent"
                                                Text { anchors.centerIn: parent; text: "✕"; color: "#CCCCCC"; font.pixelSize: 11 }
                                                MouseArea {
                                                    id: closeSearchArea; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                                    onClicked: root.searchBarVisible = false
                                                }
                                            }
                                        }

                                        // Fila 2: Reemplazar (Visible cuando replaceVisible es true)
                                        RowLayout {
                                            visible: root.replaceVisible
                                            Layout.fillWidth: true
                                            spacing: 4

                                            Rectangle {
                                                Layout.fillWidth: true
                                                Layout.preferredHeight: 24
                                                radius: 3
                                                color: root.colInputBg
                                                border.color: replaceField.activeFocus ? root.colAccent : "#3C3C3C"
                                                border.width: 1

                                                TextInput {
                                                    id: replaceField
                                                    anchors.fill: parent
                                                    anchors.leftMargin: 6
                                                    anchors.rightMargin: 6
                                                    verticalAlignment: TextInput.AlignVCenter
                                                    color: "#FFFFFF"
                                                    font.pixelSize: 12
                                                    font.family: "Segoe UI, Inter, sans-serif"
                                                    selectByMouse: true
                                                    clip: true

                                                    Text {
                                                        anchors.fill: parent
                                                        verticalAlignment: Text.AlignVCenter
                                                        text: "Reemplazar"
                                                        color: "#858585"
                                                        font.pixelSize: 12
                                                        visible: replaceField.text.length === 0
                                                    }
                                                }
                                            }

                                            // Botón Reemplazar Todo
                                            Rectangle {
                                                height: 22
                                                width: repAllTxt.implicitWidth + 12
                                                radius: 3
                                                color: repAllArea.containsMouse ? root.colAccent : "#313131"

                                                Text {
                                                    id: repAllTxt
                                                    anchors.centerIn: parent
                                                    text: "Reemplazar todo"
                                                    color: "#FFFFFF"
                                                    font.pixelSize: 10
                                                }

                                                MouseArea {
                                                    id: repAllArea
                                                    anchors.fill: parent
                                                    hoverEnabled: true
                                                    cursorShape: Qt.PointingHandCursor
                                                    onClicked: {
                                                        if (searchField.text.length > 0) {
                                                            codeEditor.text = codeEditor.text.split(searchField.text).join(replaceField.text)
                                                            root.findInEditor(searchField.text)
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }

                            // ── COMMAND PALETTE FLOTANTE (Ctrl+Shift+P / F1) ─────────────
                            Rectangle {
                                id: commandPaletteBox
                                visible: root.commandPaletteVisible
                                anchors.top: parent.top
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.topMargin: 6
                                width: Math.min(520, parent.width - 32)
                                height: 250
                                radius: 6
                                color: root.colWidgetBg
                                border.color: "#454545"
                                border.width: 1
                                z: 100
                                clip: true

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 6
                                    spacing: 6

                                    // Input de la paleta de comandos
                                    Rectangle {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 28
                                        radius: 4
                                        color: root.colInputBg
                                        border.color: root.colAccent
                                        border.width: 1

                                        TextInput {
                                            id: cmdPaletteInput
                                            anchors.fill: parent
                                            anchors.leftMargin: 8
                                            anchors.rightMargin: 8
                                            verticalAlignment: TextInput.AlignVCenter
                                            color: "#FFFFFF"
                                            font.pixelSize: 12
                                            font.family: "Segoe UI, Inter, sans-serif"
                                            text: ">"
                                            selectByMouse: true
                                        }
                                    }

                                    // Lista de comandos disponibles con atajos
                                    ScrollView {
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        clip: true

                                        ColumnLayout {
                                            width: parent.width
                                            spacing: 2

                                            Repeater {
                                                model: [
                                                    { name: "JSFX: Compilar y Aplicar Script JIT", key: "Ctrl+S / F5", fn: function() { root.compileAndApply() } },
                                                    { name: "Ver: Alternar Barra Lateral Primaria", key: "Ctrl+B", fn: function() { root.sidebarVisible = !root.sidebarVisible } },
                                                    { name: "Ver: Alternar Panel Inferior (Problemas / Salida)", key: "Ctrl+J", fn: function() { root.bottomPanelVisible = !root.bottomPanelVisible } },
                                                    { name: "Ver: Alternar Minimapa de Código", key: "", fn: function() { root.minimapVisible = !root.minimapVisible } },
                                                    { name: "Ver: Alternar Ajuste de Línea (Word Wrap)", key: "Alt+Z", fn: function() { root.wordWrapEnabled = !root.wordWrapEnabled } },
                                                    { name: "Plantilla DSP: Cargar Saturador de Tubos", key: "Preset", fn: function() { root.loadTemplateTube() } },
                                                    { name: "Plantilla DSP: Cargar Filtro Pasa-Bajos Biquad", key: "Preset", fn: function() { root.loadTemplateFilter() } },
                                                    { name: "Plantilla DSP: Cargar Stereo Widener M/S", key: "Preset", fn: function() { root.loadTemplateWidener() } },
                                                    { name: "Plantilla DSP: Cargar Delay Estéreo", key: "Preset", fn: function() { root.loadTemplateDelay() } }
                                                ]

                                                delegate: Rectangle {
                                                    Layout.fillWidth: true
                                                    Layout.preferredHeight: 26
                                                    radius: 3
                                                    color: cmdItemArea.containsMouse ? root.colListActiveBg : "transparent"

                                                    RowLayout {
                                                        anchors.fill: parent
                                                        anchors.leftMargin: 8
                                                        anchors.rightMargin: 8

                                                        Text {
                                                            text: modelData.name
                                                            color: cmdItemArea.containsMouse ? "#FFFFFF" : "#CCCCCC"
                                                            font.pixelSize: 12
                                                            Layout.fillWidth: true
                                                            elide: Text.ElideRight
                                                        }

                                                        Rectangle {
                                                            visible: modelData.key.length > 0
                                                            width: keyTxt.implicitWidth + 10
                                                            height: 18
                                                            radius: 3
                                                            color: "#333333"
                                                            border.color: "#454545"
                                                            border.width: 1

                                                            Text {
                                                                id: keyTxt
                                                                anchors.centerIn: parent
                                                                text: modelData.key
                                                                color: "#CCCCCC"
                                                                font.pixelSize: 10
                                                                font.family: "Monospace"
                                                            }
                                                        }
                                                    }

                                                    MouseArea {
                                                        id: cmdItemArea
                                                        anchors.fill: parent
                                                        hoverEnabled: true
                                                        cursorShape: Qt.PointingHandCursor
                                                        onClicked: {
                                                            root.commandPaletteVisible = false
                                                            modelData.fn()
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        // ── 2.4 PANEL INFERIOR (VS Code Panel: Problemas / Salida / Terminal DSP) ──
                        Rectangle {
                            id: bottomPanel
                            Layout.fillWidth: true
                            Layout.preferredHeight: root.bottomPanelVisible ? root.bottomPanelHeight : 0
                            visible: root.bottomPanelVisible
                            clip: true
                            color: root.colSideBar

                            // Tirador superior redimensionable (VS Code Sash con resaltado azul)
                            Rectangle {
                                id: panelResizeSash
                                anchors.top: parent.top
                                anchors.left: parent.left
                                anchors.right: parent.right
                                height: 3
                                z: 20
                                color: sashDragArea.containsMouse || sashDragArea.pressed ? root.colAccent : root.colBorder

                                MouseArea {
                                    id: sashDragArea
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.SizeVerCursor
                                    property real startY: 0
                                    onPressed: function(mouse) { startY = mouse.y }
                                    onPositionChanged: function(mouse) {
                                        if (pressed) {
                                            var delta = startY - mouse.y
                                            root.bottomPanelHeight = Math.max(90, Math.min(380, root.bottomPanelHeight + delta))
                                        }
                                    }
                                }
                            }

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.topMargin: 3
                                spacing: 0

                                // Cabecera de pestañas del panel inferior estilo VS Code
                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 32
                                    color: root.colSideBar

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 16
                                        anchors.rightMargin: 8
                                        spacing: 20

                                        // ── PESTAÑA 0: PROBLEMAS ──
                                        Item {
                                            Layout.preferredWidth: tabProbRow.implicitWidth
                                            Layout.fillHeight: true

                                            RowLayout {
                                                id: tabProbRow
                                                anchors.centerIn: parent
                                                spacing: 6

                                                Text {
                                                    text: "PROBLEMAS"
                                                    color: root.activeBottomTab === 0 ? "#FFFFFF" : "#858585"
                                                    font.pixelSize: 11
                                                    font.bold: true
                                                    font.letterSpacing: 0.3
                                                }

                                                Rectangle {
                                                    width: 16; height: 16; radius: 8
                                                    color: root.compilationError.length > 0 ? root.colError : "#313131"

                                                    Text {
                                                        anchors.centerIn: parent
                                                        text: root.compilationError.length > 0 ? "1" : "0"
                                                        color: "#FFFFFF"
                                                        font.pixelSize: 9
                                                        font.bold: true
                                                    }
                                                }
                                            }

                                            // Subrayado azul de pestaña activa
                                            Rectangle {
                                                visible: root.activeBottomTab === 0
                                                anchors.bottom: parent.bottom
                                                anchors.left: parent.left
                                                anchors.right: parent.right
                                                height: 2
                                                color: root.colAccent
                                            }

                                            MouseArea {
                                                anchors.fill: parent
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: root.activeBottomTab = 0
                                            }
                                        }

                                        // ── PESTAÑA 1: SALIDA ──
                                        Item {
                                            Layout.preferredWidth: tabOutText.implicitWidth
                                            Layout.fillHeight: true

                                            Text {
                                                id: tabOutText
                                                anchors.centerIn: parent
                                                text: "SALIDA"
                                                color: root.activeBottomTab === 1 ? "#FFFFFF" : "#858585"
                                                font.pixelSize: 11
                                                font.bold: true
                                                font.letterSpacing: 0.3
                                            }

                                            Rectangle {
                                                visible: root.activeBottomTab === 1
                                                anchors.bottom: parent.bottom
                                                anchors.left: parent.left
                                                anchors.right: parent.right
                                                height: 2
                                                color: root.colAccent
                                            }

                                            MouseArea {
                                                anchors.fill: parent
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: root.activeBottomTab = 1
                                            }
                                        }

                                        // ── PESTAÑA 2: TERMINAL DSP ──
                                        Item {
                                            Layout.preferredWidth: tabTermText.implicitWidth
                                            Layout.fillHeight: true

                                            Text {
                                                id: tabTermText
                                                anchors.centerIn: parent
                                                text: "TERMINAL DSP"
                                                color: root.activeBottomTab === 2 ? "#FFFFFF" : "#858585"
                                                font.pixelSize: 11
                                                font.bold: true
                                                font.letterSpacing: 0.3
                                            }

                                            Rectangle {
                                                visible: root.activeBottomTab === 2
                                                anchors.bottom: parent.bottom
                                                anchors.left: parent.left
                                                anchors.right: parent.right
                                                height: 2
                                                color: root.colAccent
                                            }

                                            MouseArea {
                                                anchors.fill: parent
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: root.activeBottomTab = 2
                                            }
                                        }

                                        Item { Layout.fillWidth: true }

                                        // Selector de canal de salida estilo VS Code (solo en Salida)
                                        Rectangle {
                                            visible: root.activeBottomTab === 1
                                            width: 115; height: 22; radius: 3
                                            color: root.colInputBg
                                            border.color: "#3C3C3C"
                                            border.width: 1

                                            RowLayout {
                                                anchors.fill: parent
                                                anchors.leftMargin: 8
                                                anchors.rightMargin: 6
                                                Text {
                                                    text: "Compilador EEL2"
                                                    color: "#CCCCCC"
                                                    font.pixelSize: 10
                                                    Layout.fillWidth: true
                                                }
                                                Text { text: "▾"; color: "#858585"; font.pixelSize: 10 }
                                            }
                                        }

                                        // Acciones derechas del panel inferior
                                        Row {
                                            spacing: 4

                                            // Limpiar consola
                                            Rectangle {
                                                width: 22; height: 22; radius: 3
                                                color: clearOutArea.containsMouse ? root.colHoverBg : "transparent"

                                                Text {
                                                    anchors.centerIn: parent
                                                    text: "⊘"
                                                    color: "#CCCCCC"
                                                    font.pixelSize: 12
                                                }

                                                MouseArea {
                                                    id: clearOutArea
                                                    anchors.fill: parent
                                                    hoverEnabled: true
                                                    cursorShape: Qt.PointingHandCursor
                                                    onClicked: root.lastCompilationLog = "[EEL2 JIT] Salida limpiada."
                                                }
                                            }

                                            // Expandir / contraer altura del panel
                                            Rectangle {
                                                width: 22; height: 22; radius: 3
                                                color: maxPanelArea.containsMouse ? root.colHoverBg : "transparent"

                                                Text {
                                                    anchors.centerIn: parent
                                                    text: root.bottomPanelHeight > 220 ? "∨" : "∧"
                                                    color: "#CCCCCC"
                                                    font.pixelSize: 11
                                                    font.bold: true
                                                }

                                                MouseArea {
                                                    id: maxPanelArea
                                                    anchors.fill: parent
                                                    hoverEnabled: true
                                                    cursorShape: Qt.PointingHandCursor
                                                    onClicked: root.bottomPanelHeight = (root.bottomPanelHeight > 220 ? 155 : 280)
                                                }
                                            }

                                            // Cerrar panel inferior
                                            Rectangle {
                                                width: 22; height: 22; radius: 3
                                                color: closeBtmArea.containsMouse ? root.colHoverBg : "transparent"

                                                Text {
                                                    anchors.centerIn: parent
                                                    text: "✕"
                                                    color: "#CCCCCC"
                                                    font.pixelSize: 11
                                                }

                                                MouseArea {
                                                    id: closeBtmArea
                                                    anchors.fill: parent
                                                    hoverEnabled: true
                                                    cursorShape: Qt.PointingHandCursor
                                                    onClicked: root.bottomPanelVisible = false
                                                }
                                            }
                                        }
                                    }
                                }

                                // Contenido del panel inferior
                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    color: root.colEditorBg

                                    ScrollView {
                                        anchors.fill: parent
                                        anchors.margins: 10
                                        clip: true

                                        ColumnLayout {
                                            width: parent.width
                                            spacing: 6

                                            // ── VISTA 0: PROBLEMAS ──
                                            RowLayout {
                                                visible: root.activeBottomTab === 0
                                                spacing: 8

                                                Rectangle {
                                                    width: 16; height: 16; radius: 8
                                                    color: root.compilationError.length > 0 ? root.colError : "#388A34"
                                                    Text {
                                                        anchors.centerIn: parent
                                                        text: root.compilationError.length > 0 ? "✕" : "✓"
                                                        color: "#FFFFFF"
                                                        font.pixelSize: 9
                                                        font.bold: true
                                                    }
                                                }

                                                Text {
                                                    text: root.compilationError.length > 0
                                                          ? root.compilationError + " [Ln " + root.currentLine + ", Col " + root.currentCol + "]"
                                                          : "No se han detectado problemas en el espacio de trabajo hasta el momento."
                                                    color: root.compilationError.length > 0 ? root.colError : "#CCCCCC"
                                                    font.family: "JetBrains Mono, Consolas, Monospace"
                                                    font.pixelSize: 12
                                                    Layout.fillWidth: true
                                                    wrapMode: Text.WordWrap
                                                }
                                            }

                                            // ── VISTA 1: SALIDA DEL COMPILADOR ──
                                            Text {
                                                visible: root.activeBottomTab === 1
                                                text: root.lastCompilationLog
                                                color: root.compilationError.length > 0 ? root.colError : root.colSuccess
                                                font.family: "JetBrains Mono, Consolas, Monospace"
                                                font.pixelSize: 12
                                                wrapMode: Text.WordWrap
                                                Layout.fillWidth: true
                                            }

                                            // ── VISTA 2: TERMINAL DSP INTERACTIVA ──
                                            ColumnLayout {
                                                visible: root.activeBottomTab === 2
                                                Layout.fillWidth: true
                                                spacing: 4

                                                Text {
                                                    text: "nova_studio@dsp-engine:~/jsfx$ eel2-jit --status"
                                                    color: "#569CD6"
                                                    font.family: "JetBrains Mono, Consolas, Monospace"
                                                    font.pixelSize: 11
                                                    font.bold: true
                                                }

                                                Text {
                                                    text: "• Pista destino: Track #" + (root.trackIndex >= 0 ? root.trackIndex + 1 : "N/A") +
                                                          " | Sliders vinculados: " + root.parsedSliders.length +
                                                          " | Sección activa: " + root.currentSection +
                                                          " | Líneas: " + codeEditor.lineCount
                                                    color: "#CCCCCC"
                                                    font.family: "JetBrains Mono, Consolas, Monospace"
                                                    font.pixelSize: 11
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // ─────────────────────────────────────────────────────────────────
            // 3. BARRA DE ESTADO INFERIOR (VS Code Status Bar)
            // ─────────────────────────────────────────────────────────────────
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 22
                color: root.compilationError.length > 0 ? "#C72E0F" : root.colAccent

                RowLayout {
                    anchors.fill: parent
                    anchors.rightMargin: 10
                    spacing: 0

                    // Indicador Izquierdo de Entorno Remoto / Motor JIT (><) de VS Code
                    Rectangle {
                        Layout.fillHeight: true
                        Layout.preferredWidth: remoteRow.implicitWidth + 18
                        color: "#16825D" // Verde azulado oficial de VS Code Remote Host

                        Row {
                            id: remoteRow
                            anchors.centerIn: parent
                            spacing: 5

                            Text {
                                text: "><"
                                color: "#FFFFFF"
                                font.pixelSize: 10
                                font.bold: true
                                font.family: "Monospace"
                            }
                            Text {
                                text: "DSP JIT"
                                color: "#FFFFFF"
                                font.pixelSize: 11
                                font.bold: true
                            }
                        }
                    }

                    // Rama Git
                    Rectangle {
                        Layout.fillHeight: true
                        Layout.preferredWidth: gitRow.implicitWidth + 14
                        color: statusGitArea.containsMouse ? "#1FFFFFFF" : "transparent"

                        Row {
                            id: gitRow
                            anchors.centerIn: parent
                            spacing: 4
                            Text { text: "⑂"; color: "#FFFFFF"; font.pixelSize: 11 }
                            Text {
                                text: "main" + (root.isModified ? "*" : "")
                                color: "#FFFFFF"
                                font.pixelSize: 11
                            }
                        }

                        MouseArea {
                            id: statusGitArea
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                        }
                    }

                    // Contador de Errores y Advertencias (Clic abre el panel inferior)
                    Rectangle {
                        Layout.fillHeight: true
                        Layout.preferredWidth: errRow.implicitWidth + 14
                        color: statusErrArea.containsMouse ? "#1FFFFFFF" : "transparent"

                        Row {
                            id: errRow
                            anchors.centerIn: parent
                            spacing: 6
                            Text {
                                text: (root.compilationError.length > 0 ? "⊗ 1" : "⊗ 0") + "  ⚠ 0"
                                color: "#FFFFFF"
                                font.pixelSize: 11
                            }
                        }

                        MouseArea {
                            id: statusErrArea
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                root.activeBottomTab = 0
                                root.bottomPanelVisible = !root.bottomPanelVisible
                            }
                        }
                    }

                    Item { Layout.fillWidth: true }

                    // Posición de Línea, Columna y Selección
                    Rectangle {
                        Layout.fillHeight: true
                        Layout.preferredWidth: lnColTxt.implicitWidth + 14
                        color: statusLnArea.containsMouse ? "#1FFFFFFF" : "transparent"

                        Text {
                            id: lnColTxt
                            anchors.centerIn: parent
                            text: "Ln " + root.currentLine + ", Col " + root.currentCol +
                                  (root.selectedChars > 0 ? " (" + root.selectedChars + " seleccionados)" : "")
                            color: "#FFFFFF"
                            font.pixelSize: 11
                        }

                        MouseArea {
                            id: statusLnArea
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                        }
                    }

                    // Espacios de tabulación
                    Rectangle {
                        Layout.fillHeight: true
                        Layout.preferredWidth: spcTxt.implicitWidth + 14
                        color: "transparent"
                        Text { id: spcTxt; anchors.centerIn: parent; text: "Espacios: 4"; color: "#FFFFFF"; font.pixelSize: 11 }
                    }

                    // Codificación
                    Rectangle {
                        Layout.fillHeight: true
                        Layout.preferredWidth: utfTxt.implicitWidth + 14
                        color: "transparent"
                        Text { id: utfTxt; anchors.centerIn: parent; text: "UTF-8"; color: "#FFFFFF"; font.pixelSize: 11 }
                    }

                    // Secuencia de fin de línea
                    Rectangle {
                        Layout.fillHeight: true
                        Layout.preferredWidth: lfTxt.implicitWidth + 14
                        color: "transparent"
                        Text { id: lfTxt; anchors.centerIn: parent; text: "LF"; color: "#FFFFFF"; font.pixelSize: 11 }
                    }

                    // Modo de Lenguaje con llaves { }
                    Rectangle {
                        Layout.fillHeight: true
                        Layout.preferredWidth: langTxt.implicitWidth + 14
                        color: statusLangArea.containsMouse ? "#1FFFFFFF" : "transparent"

                        Text {
                            id: langTxt
                            anchors.centerIn: parent
                            text: "{ } JSFX / EEL2"
                            color: "#FFFFFF"
                            font.pixelSize: 11
                            font.bold: true
                        }

                        MouseArea {
                            id: statusLangArea
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                root.activeSidebarTab = 2
                                root.sidebarVisible = true
                            }
                        }
                    }

                    // Icono de campana de notificaciones estilo VS Code
                    Rectangle {
                        Layout.fillHeight: true
                        Layout.preferredWidth: 24
                        color: "transparent"
                        Text {
                            anchors.centerIn: parent
                            text: "🔔"
                            font.pixelSize: 10
                            color: "#FFFFFF"
                        }
                    }
                }
            }
        }
    }

    // =========================================================================
    // 4. FUNCIONES PÚBLICAS Y LÓGICA DE COMPILACIÓN
    // =========================================================================

    function updateCursorPosition() {
        var pos = codeEditor.cursorPosition
        var sub = codeEditor.text.substring(0, pos)
        var lines = sub.split("\n")
        root.currentLine = lines.length
        root.currentCol = lines[lines.length - 1].length + 1

        // Detectar automáticamente la sección EEL2 activa (@init, @slider, @block, @sample)
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

    function refreshParsedSliders() {
        var list = []
        var lines = codeEditor.text.split("\n")
        for (var i = 0; i < lines.length; ++i) {
            var l = lines[i].trim()
            if (l.indexOf("slider") === 0) {
                var m = l.match(/^slider([0-9]+)\s*:\s*([0-9.\-]+)\s*<([0-9.\-]+)\s*,\s*([0-9.\-]+)(?:\s*,\s*([0-9.\-]+))?>\s*(.*)$/)
                if (m) {
                    list.push({
                        index: parseInt(m[1]),
                        value: parseFloat(m[2]),
                        min: parseFloat(m[3]),
                        max: parseFloat(m[4]),
                        step: m[5] ? parseFloat(m[5]) : 0.1,
                        name: m[6] || ("Slider " + m[1])
                    })
                }
            }
        }
        root.parsedSliders = list
    }

    function findInEditor(term) {
        if (!term || term.length === 0) {
            root.searchMatchesCount = 0
            root.currentMatchIndex = 0
            return
        }

        var sourceText = root.matchCase ? codeEditor.text : codeEditor.text.toLowerCase()
        var query = root.matchCase ? term : term.toLowerCase()

        // Contar todas las coincidencias en el documento
        var count = 0
        var pos = 0
        var firstIdx = -1
        while (true) {
            var found = sourceText.indexOf(query, pos)
            if (found === -1) break
            if (firstIdx === -1) firstIdx = found
            count++
            pos = found + Math.max(1, query.length)
        }
        root.searchMatchesCount = count

        if (count === 0) {
            root.currentMatchIndex = 0
            return
        }

        var idx = sourceText.indexOf(query, codeEditor.cursorPosition)
        if (idx === -1) idx = firstIdx
        if (idx !== -1) {
            root.currentMatchIndex = 1
            codeEditor.cursorPosition = idx
            codeEditor.select(idx, idx + term.length)
        }
    }

    function findNext() {
        if (!searchField.text || searchField.text.length === 0) return
        var term = root.matchCase ? searchField.text : searchField.text.toLowerCase()
        var sourceText = root.matchCase ? codeEditor.text : codeEditor.text.toLowerCase()
        var start = codeEditor.cursorPosition + 1
        var idx = sourceText.indexOf(term, start)
        if (idx === -1) {
            idx = sourceText.indexOf(term, 0)
            root.currentMatchIndex = root.searchMatchesCount > 0 ? 1 : 0
        } else {
            root.currentMatchIndex = Math.min(root.searchMatchesCount, root.currentMatchIndex + 1)
        }
        if (idx !== -1) {
            codeEditor.cursorPosition = idx
            codeEditor.select(idx, idx + term.length)
        }
    }

    function findPrev() {
        if (!searchField.text || searchField.text.length === 0) return
        var term = root.matchCase ? searchField.text : searchField.text.toLowerCase()
        var sourceText = root.matchCase ? codeEditor.text : codeEditor.text.toLowerCase()
        var start = Math.max(0, codeEditor.cursorPosition - 1)
        var idx = sourceText.lastIndexOf(term, start)
        if (idx === -1) {
            idx = sourceText.lastIndexOf(term)
            root.currentMatchIndex = root.searchMatchesCount
        } else {
            root.currentMatchIndex = Math.max(1, root.currentMatchIndex - 1)
        }
        if (idx !== -1) {
            codeEditor.cursorPosition = idx
            codeEditor.select(idx, idx + term.length)
        }
    }

    function openForPlugin(trkIdx, plgIdx, plgNm) {
        root.trackIndex = trkIdx
        root.pluginIndex = plgIdx
        root.pluginName = plgNm
        root.isNewPlugin = false
        root.isModified = false

        if (typeof AudioEngine !== "undefined" && AudioEngine.tracks) {
            var src = AudioEngine.tracks.getPluginScript(trkIdx, plgIdx)
            if (src && src.length > 0) {
                codeEditor.text = src
            } else {
                root.loadTemplateTube()
            }
        } else {
            root.loadTemplateTube()
        }

        root.compilationError = ""
        root.lastCompilationLog = "[EEL2 JIT] Script cargado para '" + plgNm + "'. Listo para compilar."
        root.refreshParsedSliders()
        root.visible = true
    }

    function openNewScript(trkIdx) {
        root.trackIndex = trkIdx
        root.pluginIndex = -1
        root.pluginName = "Nuevo Efecto JSFX"
        root.isNewPlugin = true
        root.isModified = false
        root.loadTemplateTube()
        root.compilationError = ""
        root.lastCompilationLog = "[EEL2 JIT] Nuevo script inicializado desde plantilla. Presiona Ctrl+S para compilar e insertar."
        root.refreshParsedSliders()
        root.visible = true
    }

    function compileAndApply() {
        if (typeof AudioEngine === "undefined" || !AudioEngine.tracks) {
            root.compilationError = "El motor de audio no está activo"
            root.lastCompilationLog = "[Error] No se puede compilar: AudioEngine no está listo."
            root.activeBottomTab = 0
            root.bottomPanelVisible = true
            return
        }

        var ts = Qt.formatTime(new Date(), "hh:mm:ss")

        if (root.isNewPlugin) {
            var ok = AudioEngine.tracks.createCustomJsfxPlugin(root.trackIndex, "Nuevo Script JSFX", codeEditor.text)
            if (ok) {
                root.compilationError = ""
                root.lastCompilationLog = "[" + ts + "] [EEL2 JIT] ✓ Compilación exitosa: Script creado e insertado en la pista. Procesando audio en caliente."
                root.isModified = false
                root.refreshParsedSliders()
                root.scriptApplied()
                root.closeDialog()
            } else {
                root.compilationError = "Error al compilar el nuevo script (revisa la sintaxis de las secciones @init, @slider, @sample)."
                root.lastCompilationLog = "[" + ts + "] [Error] Falló la compilación del script JSFX."
                root.activeBottomTab = 0
                root.bottomPanelVisible = true
            }
        } else {
            var err = AudioEngine.tracks.updatePluginScript(root.trackIndex, root.pluginIndex, codeEditor.text)
            if (!err || err.length === 0) {
                root.compilationError = ""
                root.lastCompilationLog = "[" + ts + "] [EEL2 JIT] ✓ Compilación exitosa sin errores: audio actualizado en tiempo real."
                root.isModified = false
                root.refreshParsedSliders()
                root.scriptApplied()
            } else {
                root.compilationError = err
                root.lastCompilationLog = "[" + ts + "] [Error EEL2] " + err
                root.activeBottomTab = 0
                root.bottomPanelVisible = true
            }
        }
    }

    function closeDialog() {
        root.visible = false
        root.closed()
    }

    function defaultScript() {
        return "desc: Mi Efecto JSFX Personalizado\n" +
               "slider1:0<-24,24,0.1>Ganancia (dB)\n\n" +
               "@init\n" +
               "gain = 1.0;\n\n" +
               "@slider\n" +
               "gain = 10^(slider1 / 20);\n\n" +
               "@sample\n" +
               "spl0 = spl0 * gain;\n" +
               "spl1 = spl1 * gain;\n"
    }

    function loadTemplateTube() {
        codeEditor.text =
            "desc: Analog Tube Saturator\n" +
            "slider1:6<0,24,0.1>Drive (dB)\n" +
            "slider2:50<0,100,1>Calidez (%)\n" +
            "slider3:0<-18,12,0.1>Salida (dB)\n\n" +
            "@init\n" +
            "drive = 1.0;\n" +
            "outGain = 1.0;\n\n" +
            "@slider\n" +
            "drive = 10^(slider1 / 20);\n" +
            "outGain = 10^(slider3 / 20);\n" +
            "warmth = slider2 / 100;\n\n" +
            "@sample\n" +
            "// Saturación analógica no lineal con armónicos de triodo\n" +
            "x0 = spl0 * drive;\n" +
            "x1 = spl1 * drive;\n" +
            "spl0 = (tanh(x0) + warmth * (x0 * x0 * 0.1)) * outGain;\n" +
            "spl1 = (tanh(x1) + warmth * (x1 * x1 * 0.1)) * outGain;\n"
        root.isModified = false
        root.refreshParsedSliders()
    }

    function loadTemplateFilter() {
        codeEditor.text =
            "desc: Resonant Lowpass Filter\n" +
            "slider1:1000<20,20000,10>Frecuencia Corte (Hz)\n" +
            "slider2:1<0.5,10,0.1>Resonancia (Q)\n\n" +
            "@init\n" +
            "s1 = 0; s2 = 0;\n" +
            "s3 = 0; s4 = 0;\n\n" +
            "@slider\n" +
            "w0 = 2 * 3.14159265 * slider1 / srate;\n" +
            "alpha = sin(w0) / (2 * slider2);\n" +
            "b1 = 1 - cos(w0);\n" +
            "b0 = b1 * 0.5;\n" +
            "b2 = b0;\n" +
            "a0 = 1 + alpha;\n" +
            "a1 = -2 * cos(w0);\n" +
            "a2 = 1 - alpha;\n\n" +
            "@sample\n" +
            "// Filtro Biquad Lowpass de 12 dB/octava\n" +
            "out0 = (b0/a0)*spl0 + s1;\n" +
            "s1 = (b1/a0)*spl0 - (a1/a0)*out0 + s2;\n" +
            "s2 = (b2/a0)*spl0 - (a2/a0)*out0;\n" +
            "spl0 = out0;\n" +
            "out1 = (b0/a0)*spl1 + s3;\n" +
            "s3 = (b1/a0)*spl1 - (a1/a0)*out1 + s4;\n" +
            "s4 = (b2/a0)*spl1 - (a2/a0)*out1;\n" +
            "spl1 = out1;\n"
        root.isModified = false
        root.refreshParsedSliders()
    }

    function loadTemplateWidener() {
        codeEditor.text =
            "desc: Stereo Widener Mid/Side\n" +
            "slider1:120<0,200,1>Ancho Estéreo (%)\n" +
            "slider2:0<-12,12,0.1>Balance Mid/Side (dB)\n\n" +
            "@init\n" +
            "width = 1.0;\n\n" +
            "@slider\n" +
            "width = slider1 / 100;\n" +
            "sideTrim = 10^(slider2 / 20);\n\n" +
            "@sample\n" +
            "// Descomposición Mid/Side\n" +
            "mid = (spl0 + spl1) * 0.5;\n" +
            "side = (spl0 - spl1) * 0.5 * width * sideTrim;\n" +
            "spl0 = mid + side;\n" +
            "spl1 = mid - side;\n"
        root.isModified = false
        root.refreshParsedSliders()
    }

    function loadTemplateDelay() {
        codeEditor.text =
            "desc: Simple Stereo Delay\n" +
            "slider1:250<10,1000,10>Tiempo Retardo (ms)\n" +
            "slider2:40<0,95,1>Feedback (%)\n" +
            "slider3:30<0,100,1>Mezcla Wet (%)\n\n" +
            "@init\n" +
            "buf_pos = 0;\n" +
            "max_delay = 96000;\n\n" +
            "@slider\n" +
            "delay_samples = floor(slider1 * 0.001 * srate);\n" +
            "feedback = slider2 / 100;\n" +
            "wet = slider3 / 100;\n" +
            "dry = 1 - wet;\n\n" +
            "@sample\n" +
            "read_pos = buf_pos - delay_samples;\n" +
            "read_pos < 0 ? read_pos += max_delay;\n" +
            "delayed_l = read_pos[0];\n" +
            "delayed_r = (read_pos + 100000)[0];\n" +
            "buf_pos[0] = spl0 + delayed_l * feedback;\n" +
            "(buf_pos + 100000)[0] = spl1 + delayed_r * feedback;\n" +
            "buf_pos += 1;\n" +
            "buf_pos >= max_delay ? buf_pos = 0;\n" +
            "spl0 = spl0 * dry + delayed_l * wet;\n" +
            "spl1 = spl1 * dry + delayed_r * wet;\n"
        root.isModified = false
        root.refreshParsedSliders()
    }

    function loadTemplateGain() {
        codeEditor.text = defaultScript()
        root.isModified = false
        root.refreshParsedSliders()
    }
}