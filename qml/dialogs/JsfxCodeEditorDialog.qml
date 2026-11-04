import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

/**
 * Editor de Código en Vivo para Scripts JSFX (REAPER EEL2) con Estilo VS Code.
 * Proporciona un entorno de desarrollo integrado completo para crear, editar,
 * depurar y probar efectos de audio DSP en tiempo real.
 */
Rectangle {
    id: root

    property int trackIndex: -1
    property int pluginIndex: -1
    property string pluginName: "Script JSFX"
    property bool isNewPlugin: false
    property bool isModified: false

    // Estado de cursor y posición
    property int currentLine: 1
    property int currentCol: 1

    // Vistas de la barra de actividades (0: Explorer, 1: Sliders en vivo, 2: Referencia EEL2, 3: Salida/Logs)
    property int activeSidebarTab: 0
    property bool sidebarVisible: true

    // Estado del panel inferior (0: Problemas, 1: Salida del compilador)
    property int activeBottomTab: 1
    property bool bottomPanelVisible: true
    property string lastCompilationLog: "Listo para compilar. Presiona Ctrl+S o F5."
    property string compilationError: ""

    // Sliders parseados dinámicamente
    property var parsedSliders: []

    signal scriptApplied()
    signal closed()

    anchors.fill: parent
    color: "#B3000000" // Backdrop oscuro
    z: 10000

    // Evitar que los clics traspasen al fondo
    MouseArea {
        anchors.fill: parent
        onClicked: {}
    }

    // Atajos de teclado estilo VS Code
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
        sequence: "Escape"
        onActivated: root.closeDialog()
    }

    // =========================================================================
    // VENTANA PRINCIPAL ESTILO VS CODE
    // =========================================================================
    Rectangle {
        id: vsCodeWindow
        anchors.centerIn: parent
        width: Math.min(parent.width - 24, 1120)
        height: Math.min(parent.height - 24, 760)
        radius: 6
        color: "#1E1E1E" // Color de fondo característico de VS Code Dark+
        border.color: "#333333"
        border.width: 1
        clip: true

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            // ─────────────────────────────────────────────────────────────────
            // 1. BARRA DE TÍTULO SUPERIOR (VS Code Title Bar)
            // ─────────────────────────────────────────────────────────────────
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 34
                color: "#323233"
                border.color: "#252526"
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    spacing: 8

                    // Icono de la aplicación estilo VS Code
                    Rectangle {
                        Layout.preferredWidth: 20
                        Layout.preferredHeight: 20
                        radius: 4
                        color: "#007ACC"

                        Text {
                            anchors.centerIn: parent
                            text: "⚡"
                            font.pixelSize: 11
                            color: "#FFFFFF"
                        }
                    }

                    Text {
                        text: "NOVA Studio — Editor JSFX (EEL2)"
                        color: "#CCCCCC"
                        font.pixelSize: 12
                        font.family: "Segoe UI, Inter, Sans-serif"
                    }

                    Item { Layout.fillWidth: true }

                    // Breadcrumb del archivo activo en el centro
                    Rectangle {
                        Layout.preferredHeight: 22
                        Layout.preferredWidth: titleCenterText.implicitWidth + 24
                        radius: 4
                        color: "#1F1F1F"
                        border.color: "#3C3C3C"
                        border.width: 1

                        Text {
                            id: titleCenterText
                            anchors.centerIn: parent
                            text: (root.isNewPlugin ? "nuevo_script.jsfx" : (root.pluginName.toLowerCase().replace(/ /g, "_") + ".jsfx")) + (root.isModified ? " ●" : "")
                            color: root.isModified ? "#E2C08D" : "#CCCCCC"
                            font.pixelSize: 11
                            font.family: "monospace"
                        }
                    }

                    Item { Layout.fillWidth: true }

                    // Botones de acción rápida en cabecera
                    Rectangle {
                        Layout.preferredHeight: 24
                        Layout.preferredWidth: compileHeadBtnText.implicitWidth + 20
                        radius: 3
                        color: headCompileArea.containsMouse ? "#0098FF" : "#007ACC"

                        Row {
                            anchors.centerIn: parent
                            spacing: 5
                            Text { text: "▶"; color: "#FFFFFF"; font.pixelSize: 10 }
                            Text {
                                id: compileHeadBtnText
                                text: "Compilar (Ctrl+S)"
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

                    // Botón Cerrar ventana
                    Rectangle {
                        Layout.preferredWidth: 24
                        Layout.preferredHeight: 24
                        radius: 3
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
                    color: "#333333"
                    border.color: "#252526"
                    border.width: 1

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 4
                        anchors.topMargin: 4
                        anchors.bottomMargin: 8

                        // Botón 1: Explorador de Archivos / Plantillas
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 48
                            color: root.activeSidebarTab === 0 && root.sidebarVisible ? "#252526" : "transparent"

                            // Indicador de pestaña activa (línea blanca izquierda)
                            Rectangle {
                                anchors.left: parent.left
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                width: 2
                                color: "#FFFFFF"
                                visible: root.activeSidebarTab === 0 && root.sidebarVisible
                            }

                            Text {
                                anchors.centerIn: parent
                                text: "📁"
                                font.pixelSize: 18
                                opacity: (root.activeSidebarTab === 0 && root.sidebarVisible) ? 1.0 : 0.6
                            }

                            MouseArea {
                                anchors.fill: parent
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

                        // Botón 2: Sliders y Parámetros en Vivo
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 48
                            color: root.activeSidebarTab === 1 && root.sidebarVisible ? "#252526" : "transparent"

                            Rectangle {
                                anchors.left: parent.left
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                width: 2
                                color: "#FFFFFF"
                                visible: root.activeSidebarTab === 1 && root.sidebarVisible
                            }

                            Text {
                                anchors.centerIn: parent
                                text: "🎛️"
                                font.pixelSize: 18
                                opacity: (root.activeSidebarTab === 1 && root.sidebarVisible) ? 1.0 : 0.6
                            }

                            MouseArea {
                                anchors.fill: parent
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

                        // Botón 3: Referencia EEL2 y Snippets
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 48
                            color: root.activeSidebarTab === 2 && root.sidebarVisible ? "#252526" : "transparent"

                            Rectangle {
                                anchors.left: parent.left
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                width: 2
                                color: "#FFFFFF"
                                visible: root.activeSidebarTab === 2 && root.sidebarVisible
                            }

                            Text {
                                anchors.centerIn: parent
                                text: "📖"
                                font.pixelSize: 18
                                opacity: (root.activeSidebarTab === 2 && root.sidebarVisible) ? 1.0 : 0.6
                            }

                            MouseArea {
                                anchors.fill: parent
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

                        // Botón Toggle Consola / Panel inferior
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 40
                            color: "transparent"

                            Text {
                                anchors.centerIn: parent
                                text: "📋"
                                font.pixelSize: 16
                                opacity: root.bottomPanelVisible ? 1.0 : 0.5
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.bottomPanelVisible = !root.bottomPanelVisible
                            }
                        }
                    }
                }

                // 2.2 SIDEBAR COLAPSABLE (VS Code Sidebar)
                Rectangle {
                    id: sideBar
                    Layout.preferredWidth: root.sidebarVisible ? 220 : 0
                    Layout.fillHeight: true
                    visible: root.sidebarVisible
                    clip: true
                    color: "#252526"
                    border.color: "#1E1E1E"
                    border.width: 1

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 0

                        // Cabecera de la Sidebar
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 35
                            color: "#252526"

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 12
                                anchors.rightMargin: 8

                                Text {
                                    text: {
                                        if (root.activeSidebarTab === 0) return "EXPLORADOR"
                                        if (root.activeSidebarTab === 1) return "PARÁMETROS EN VIVO"
                                        return "REFERENCIA EEL2"
                                    }
                                    color: "#BBBBBB"
                                    font.pixelSize: 11
                                    font.bold: true
                                    font.letterSpacing: 0.5
                                }

                                Item { Layout.fillWidth: true }

                                // Botón colapsar
                                Text {
                                    text: "‹"
                                    color: "#858585"
                                    font.pixelSize: 16
                                    font.bold: true

                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: root.sidebarVisible = false
                                    }
                                }
                            }
                        }

                        // Divisor fino
                        Rectangle {
                            Layout.fillWidth: true
                            height: 1
                            color: "#333333"
                        }

                        // Contenido dinámico según la pestaña activa
                        StackLayout {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            currentIndex: root.activeSidebarTab

                            // ── PESTAÑA 0: EXPLORADOR Y PLANTILLAS ──
                            ScrollView {
                                clip: true
                                contentWidth: availableWidth

                                ColumnLayout {
                                    width: parent.width
                                    spacing: 8
                                    anchors.leftMargin: 8
                                    anchors.rightMargin: 8
                                    anchors.topMargin: 8

                                    // Sección: Script actual
                                    Text {
                                        text: "▼ SCRIPT ACTIVO"
                                        color: "#717171"
                                        font.pixelSize: 10
                                        font.bold: true
                                    }

                                    Rectangle {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 28
                                        radius: 3
                                        color: "#37373D"

                                        RowLayout {
                                            anchors.fill: parent
                                            anchors.leftMargin: 8
                                            spacing: 6

                                            Text { text: "📄"; font.pixelSize: 12 }
                                            Text {
                                                text: (root.isNewPlugin ? "nuevo_script.jsfx" : root.pluginName) + (root.isModified ? " *" : "")
                                                color: "#E0E0E0"
                                                font.pixelSize: 11
                                                font.bold: true
                                                elide: Text.ElideRight
                                                Layout.fillWidth: true
                                            }
                                        }
                                    }

                                    Item { height: 10 }

                                    // Sección: Plantillas Disponibles
                                    Text {
                                        text: "▼ PLANTILLAS DSP LISTAS"
                                        color: "#717171"
                                        font.pixelSize: 10
                                        font.bold: true
                                    }

                                    Repeater {
                                        model: [
                                            { name: "Saturador de Tubos", fn: function() { root.loadTemplateTube() }, desc: "Calidez analógica triodo" },
                                            { name: "Filtro Pasa-Bajos con Q", fn: function() { root.loadTemplateFilter() }, desc: "Biquad resonante 12dB" },
                                            { name: "Stereo Widener M/S", fn: function() { root.loadTemplateWidener() }, desc: "Amplitud espacial Mid/Side" },
                                            { name: "Delay Estéreo Simple", fn: function() { root.loadTemplateDelay() }, desc: "Eco temporal con feedback" },
                                            { name: "Ganancia Básica", fn: function() { root.loadTemplateGain() }, desc: "Controlador dB elemental" }
                                        ]

                                        delegate: Rectangle {
                                            Layout.fillWidth: true
                                            Layout.preferredHeight: 40
                                            radius: 3
                                            color: tplMouse.containsMouse ? "#2A2D2E" : "transparent"

                                            ColumnLayout {
                                                anchors.fill: parent
                                                anchors.leftMargin: 8
                                                spacing: 2
                                                alignment: Qt.AlignVCenter

                                                Text {
                                                    text: "⚡ " + modelData.name
                                                    color: tplMouse.containsMouse ? "#FFFFFF" : "#CCCCCC"
                                                    font.pixelSize: 11
                                                }
                                                Text {
                                                    text: modelData.desc
                                                    color: "#7F7F7F"
                                                    font.pixelSize: 9
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
                            }

                            // ── PESTAÑA 1: PARÁMETROS EN VIVO (SLIDERS) ──
                            ScrollView {
                                clip: true
                                contentWidth: availableWidth

                                ColumnLayout {
                                    width: parent.width
                                    spacing: 12
                                    anchors.leftMargin: 10
                                    anchors.rightMargin: 10
                                    anchors.topMargin: 10

                                    Text {
                                        text: "Prueba los sliders en tiempo real mientras el audio se reproduce:"
                                        color: "#999999"
                                        font.pixelSize: 10
                                        wrapMode: Text.WordWrap
                                        Layout.fillWidth: true
                                    }

                                    Repeater {
                                        model: root.parsedSliders

                                        delegate: ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 4

                                            RowLayout {
                                                Layout.fillWidth: true
                                                Text {
                                                    text: modelData.name
                                                    color: "#D4D4D4"
                                                    font.pixelSize: 11
                                                    font.bold: true
                                                    Layout.fillWidth: true
                                                    elide: Text.ElideRight
                                                }
                                                Text {
                                                    text: Number(sliderCtrl.value).toFixed(1)
                                                    color: "#007ACC"
                                                    font.pixelSize: 11
                                                    font.family: "monospace"
                                                }
                                            }

                                            Slider {
                                                id: sliderCtrl
                                                Layout.fillWidth: true
                                                from: modelData.min
                                                to: modelData.max
                                                stepSize: modelData.step || 0.1
                                                value: modelData.value

                                                onMoved: {
                                                    // Actualizar valor en el motor de audio si el plugin existe
                                                    if (typeof AudioEngine !== "undefined" && AudioEngine.tracks && root.trackIndex >= 0 && root.pluginIndex >= 0) {
                                                        AudioEngine.tracks.setPluginParameter(root.trackIndex, root.pluginIndex, modelData.index, sliderCtrl.value)
                                                    }
                                                }
                                            }
                                        }
                                    }

                                    Text {
                                        visible: root.parsedSliders.length === 0
                                        text: "No se detectaron sliders definidos (usa la sintaxis slider1:def<min,max>Nombre)."
                                        color: "#777777"
                                        font.pixelSize: 11
                                        wrapMode: Text.WordWrap
                                        Layout.fillWidth: true
                                    }
                                }
                            }

                            // ── PESTAÑA 2: REFERENCIA Y SNIPPETS EEL2 ──
                            ScrollView {
                                clip: true
                                contentWidth: availableWidth

                                ColumnLayout {
                                    width: parent.width
                                    spacing: 8
                                    anchors.leftMargin: 10
                                    anchors.rightMargin: 10
                                    anchors.topMargin: 8

                                    Text {
                                        text: "Haz clic para insertar fragmentos de código:"
                                        color: "#888888"
                                        font.pixelSize: 10
                                        wrapMode: Text.WordWrap
                                        Layout.fillWidth: true
                                    }

                                    Repeater {
                                        model: [
                                            { label: "@init", snippet: "@init\n// Inicialización de variables y tablas\n", desc: "Se ejecuta al iniciar o cambiar srate" },
                                            { label: "@slider", snippet: "@slider\n// Recalcular coeficientes dependientes de sliders\n", desc: "Se ejecuta cuando un slider cambia" },
                                            { label: "@sample", snippet: "@sample\nspl0 = spl0 * gain;\nspl1 = spl1 * gain;\n", desc: "Procesa cada muestra estéreo" },
                                            { label: "Biquad Lowpass", snippet: "w0 = 2 * $pi * freq / srate;\nalpha = sin(w0) / (2 * q);\nb0 = (1 - cos(w0)) * 0.5;\n", desc: "Cálculo de filtro rápido" },
                                            { label: "Saturación Tanh", snippet: "spl0 = tanh(spl0 * drive);\nspl1 = tanh(spl1 * drive);\n", desc: "Clipper armónico suave" }
                                        ]

                                        delegate: Rectangle {
                                            Layout.fillWidth: true
                                            Layout.preferredHeight: 38
                                            radius: 3
                                            color: snipArea.containsMouse ? "#37373D" : "#2D2D30"

                                            RowLayout {
                                                anchors.fill: parent
                                                anchors.leftMargin: 8
                                                anchors.rightMargin: 8

                                                ColumnLayout {
                                                    spacing: 1
                                                    Layout.fillWidth: true
                                                    Text {
                                                        text: modelData.label
                                                        color: "#9CDCFE"
                                                        font.pixelSize: 11
                                                        font.bold: true
                                                    }
                                                    Text {
                                                        text: modelData.desc
                                                        color: "#6A9955"
                                                        font.pixelSize: 9
                                                        elide: Text.ElideRight
                                                    }
                                                }

                                                Text { text: "+"; color: "#007ACC"; font.pixelSize: 14; font.bold: true }
                                            }

                                            MouseArea {
                                                id: snipArea
                                                anchors.fill: parent
                                                hoverEnabled: true
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: {
                                                    codeEditor.insert(codeEditor.cursorPosition, modelData.snippet)
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // 2.3 ÁREA CENTRAL DEL EDITOR (Tabs + Breadcrumbs + Text Editor)
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: "#1E1E1E"

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 0

                        // ── PESTAÑAS SUPERIORES (VS Code Editor Tabs) ────────────────
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 35
                            color: "#252526"

                            RowLayout {
                                anchors.fill: parent
                                spacing: 0

                                // Pestaña activa
                                Rectangle {
                                    Layout.preferredWidth: Math.min(220, tabLabel.implicitWidth + 50)
                                    Layout.fillHeight: true
                                    color: "#1E1E1E" // Mismo color del editor

                                    // Barra azul superior de pestaña activa
                                    Rectangle {
                                        anchors.top: parent.top
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        height: 2
                                        color: "#007ACC"
                                    }

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 12
                                        anchors.rightMargin: 8
                                        spacing: 6

                                        Text { text: "JS"; color: "#E5C07B"; font.pixelSize: 11; font.bold: true }

                                        Text {
                                            id: tabLabel
                                            text: root.isNewPlugin ? "nuevo_script.jsfx" : (root.pluginName.replace(/ /g, "_") + ".jsfx")
                                            color: "#FFFFFF"
                                            font.pixelSize: 12
                                            Layout.fillWidth: true
                                            elide: Text.ElideRight
                                        }

                                        // Indicador de modificado
                                        Text {
                                            visible: root.isModified
                                            text: "●"
                                            color: "#FFFFFF"
                                            font.pixelSize: 11
                                        }
                                    }
                                }

                                Item { Layout.fillWidth: true }

                                // Botón Revertir cambios
                                Rectangle {
                                    Layout.preferredHeight: 26
                                    Layout.preferredWidth: 70
                                    radius: 3
                                    color: revertArea.containsMouse ? "#37373D" : "transparent"

                                    Text {
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

                                Item { width: 8 }
                            }
                        }

                        // ── BREADCRUMBS BAR (VS Code Breadcrumbs) ─────────────────────
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 22
                            color: "#1E1E1E"
                            border.color: "#282828"
                            border.width: 1

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 16
                                spacing: 6

                                Text { text: "nova_studio"; color: "#888888"; font.pixelSize: 11 }
                                Text { text: "›"; color: "#555555"; font.pixelSize: 11 }
                                Text { text: "audio"; color: "#888888"; font.pixelSize: 11 }
                                Text { text: "›"; color: "#555555"; font.pixelSize: 11 }
                                Text { text: "jsfx"; color: "#888888"; font.pixelSize: 11 }
                                Text { text: "›"; color: "#555555"; font.pixelSize: 11 }
                                Text {
                                    text: (root.isNewPlugin ? "nuevo_script.jsfx" : root.pluginName) + " › @sample"
                                    color: "#CCCCCC"
                                    font.pixelSize: 11
                                    font.bold: true
                                }
                            }
                        }

                        // ── ÁREA DE CÓDIGO MONOESPACIADA CON GUTTER ──────────────────
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            color: "#1E1E1E"

                            ScrollView {
                                id: editorScrollView
                                anchors.fill: parent
                                clip: true

                                Row {
                                    width: Math.max(editorScrollView.width, codeEditor.contentWidth + 70)
                                    spacing: 0

                                    // GUTTER: NÚMEROS DE LÍNEA ESTILO VS CODE
                                    Rectangle {
                                        id: lineGutter
                                        width: 48
                                        height: Math.max(codeEditor.height, editorScrollView.height)
                                        color: "#1E1E1E"

                                        Column {
                                            anchors.fill: parent
                                            anchors.topMargin: 6

                                            Repeater {
                                                model: Math.max(1, codeEditor.lineCount)
                                                delegate: Text {
                                                    width: 40
                                                    text: (index + 1).toString()
                                                    color: (index + 1 === root.currentLine) ? "#C6C6C6" : "#858585"
                                                    font.family: "Monospace, Consolas, Courier New"
                                                    font.pixelSize: 12
                                                    horizontalAlignment: Text.AlignRight
                                                }
                                            }
                                        }

                                        // Línea divisoria del gutter
                                        Rectangle {
                                            anchors.right: parent.right
                                            anchors.top: parent.top
                                            anchors.bottom: parent.bottom
                                            width: 1
                                            color: "#2B2B2B"
                                        }
                                    }

                                    // ÁREA DE EDICIÓN DE TEXTO
                                    TextEdit {
                                        id: codeEditor
                                        width: Math.max(editorScrollView.width - 50, contentWidth + 80)
                                        height: Math.max(contentHeight + 40, editorScrollView.height)
                                        color: "#D4D4D4"
                                        selectionColor: "#264F78"
                                        selectedTextColor: "#FFFFFF"
                                        font.family: "Monospace, Consolas, 'Courier New'"
                                        font.pixelSize: 12
                                        leftPadding: 12
                                        topPadding: 6
                                        wrapMode: TextEdit.NoWrap
                                        tabStopDistance: 32
                                        cursorVisible: true

                                        onCursorPositionChanged: {
                                            root.updateCursorPosition()
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

                        // ── 2.4 PANEL INFERIOR (VS Code Terminal / Salida / Problemas) ──
                        Rectangle {
                            id: bottomPanel
                            Layout.fillWidth: true
                            Layout.preferredHeight: root.bottomPanelVisible ? 140 : 0
                            visible: root.bottomPanelVisible
                            clip: true
                            color: "#1E1E1E"
                            border.color: "#2B2B2B"
                            border.width: 1

                            ColumnLayout {
                                anchors.fill: parent
                                spacing: 0

                                // Cabecera de pestañas del panel inferior
                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 28
                                    color: "#252526"

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 12
                                        spacing: 16

                                        // Pestaña PROBLEMAS
                                        Row {
                                            spacing: 6
                                            Text {
                                                text: "PROBLEMAS"
                                                color: root.activeBottomTab === 0 ? "#FFFFFF" : "#858585"
                                                font.pixelSize: 11
                                                font.bold: root.activeBottomTab === 0
                                            }

                                            Rectangle {
                                                anchors.verticalCenter: parent.verticalCenter
                                                width: 16
                                                height: 16
                                                radius: 8
                                                color: root.compilationError.length > 0 ? "#F14C4C" : "#3C3C3C"

                                                Text {
                                                    anchors.centerIn: parent
                                                    text: root.compilationError.length > 0 ? "1" : "0"
                                                    color: "#FFFFFF"
                                                    font.pixelSize: 9
                                                    font.bold: true
                                                }
                                            }

                                            MouseArea {
                                                anchors.fill: parent
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: root.activeBottomTab = 0
                                            }
                                        }

                                        // Pestaña SALIDA
                                        Text {
                                            text: "SALIDA"
                                            color: root.activeBottomTab === 1 ? "#FFFFFF" : "#858585"
                                            font.pixelSize: 11
                                            font.bold: root.activeBottomTab === 1

                                            MouseArea {
                                                anchors.fill: parent
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: root.activeBottomTab = 1
                                            }
                                        }

                                        Item { Layout.fillWidth: true }

                                        // Botón cerrar panel inferior
                                        Text {
                                            text: "✕"
                                            color: "#858585"
                                            font.pixelSize: 11

                                            MouseArea {
                                                anchors.fill: parent
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: root.bottomPanelVisible = false
                                            }
                                        }

                                        Item { width: 6 }
                                    }
                                }

                                // Contenido del panel
                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    color: "#1E1E1E"

                                    ScrollView {
                                        anchors.fill: parent
                                        anchors.margins: 8
                                        clip: true

                                        ColumnLayout {
                                            width: parent.width
                                            spacing: 4

                                            // Vista Problemas
                                            RowLayout {
                                                visible: root.activeBottomTab === 0
                                                spacing: 8
                                                Text {
                                                    text: root.compilationError.length > 0 ? "⊗" : "✓"
                                                    color: root.compilationError.length > 0 ? "#F14C4C" : "#73C991"
                                                    font.pixelSize: 14
                                                }
                                                Text {
                                                    text: root.compilationError.length > 0 ? root.compilationError : "No se han detectado problemas de compilación en el código."
                                                    color: root.compilationError.length > 0 ? "#F14C4C" : "#CCCCCC"
                                                    font.family: "Monospace"
                                                    font.pixelSize: 11
                                                    Layout.fillWidth: true
                                                    wrapMode: Text.WordWrap
                                                }
                                            }

                                            // Vista Salida / Log
                                            Text {
                                                visible: root.activeBottomTab === 1
                                                text: root.lastCompilationLog
                                                color: root.compilationError.length > 0 ? "#F14C4C" : "#89D185"
                                                font.family: "Monospace"
                                                font.pixelSize: 11
                                                wrapMode: Text.WordWrap
                                                Layout.fillWidth: true
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
                color: root.compilationError.length > 0 ? "#C5292A" : "#007ACC" // Rojo si hay error, azul VS Code si OK

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    spacing: 12

                    // Indicador de Git
                    Row {
                        spacing: 4
                        Text { text: "⑂"; color: "#FFFFFF"; font.pixelSize: 11 }
                        Text { text: "main"; color: "#FFFFFF"; font.pixelSize: 11 }
                    }

                    // Contador de Errores y Avisos
                    Row {
                        spacing: 6
                        Text {
                            text: (root.compilationError.length > 0 ? "⊗ 1" : "⊗ 0") + "  ⚠ 0"
                            color: "#FFFFFF"
                            font.pixelSize: 11
                            font.bold: true
                        }
                    }

                    // Motor JIT
                    Text {
                        text: "JIT EEL2: Activo"
                        color: "#FFFFFF"
                        font.pixelSize: 11
                    }

                    Item { Layout.fillWidth: true }

                    // Línea y Columna
                    Text {
                        text: "Ln " + root.currentLine + ", Col " + root.currentCol
                        color: "#FFFFFF"
                        font.pixelSize: 11
                    }

                    Text { text: "Espacios: 4"; color: "#FFFFFF"; font.pixelSize: 11 }
                    Text { text: "UTF-8"; color: "#FFFFFF"; font.pixelSize: 11 }
                    Text { text: "JSFX / EEL2"; color: "#FFFFFF"; font.pixelSize: 11; font.bold: true }
                }
            }
        }
    }

    // =========================================================================
    // FUNCIONES PÚBLICAS Y LÓGICA DE COMPILACIÓN
    // =========================================================================

    function updateCursorPosition() {
        var pos = codeEditor.cursorPosition
        var sub = codeEditor.text.substring(0, pos)
        var lines = sub.split("\n")
        root.currentLine = lines.length
        root.currentCol = lines[lines.length - 1].length + 1
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
