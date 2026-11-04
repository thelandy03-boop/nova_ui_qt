import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

/**
 * Editor JSFX Integrado (Dock) para NOVA Studio.
 * Se acopla directamente bajo la Timeline en la columna derecha del Workspace,
 * con Activity Bar, árbol de carpetas/archivos JSFX (estilo VS Code),
 * pestañas de edición, resaltado de sintaxis en tiempo real, consola de estado
 * y controles de compilación instantánea.
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
                    anchors.bottom: parent.bottom
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

                        // Botón cerrar pestaña
                        Text {
                            text: "✕"
                            color: closeTabMouse.containsMouse ? "#FFFFFF" : "#7A8294"
                            font.pixelSize: 11

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

                // Botón ▶ Compile
                Rectangle {
                    Layout.preferredHeight: 24
                    Layout.preferredWidth: compileBtnRow.implicitWidth + 18
                    radius: 3
                    color: compileMouse.containsMouse ? "#323846" : "#252832"
                    border.color: "#343A48"
                    border.width: 1

                    Row {
                        id: compileBtnRow
                        anchors.centerIn: parent
                        spacing: 5

                        Text { text: "▶"; color: "#E8EAED"; font.pixelSize: 9; anchors.verticalCenter: parent.verticalCenter }
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

                // Botón 💾 Save
                Rectangle {
                    Layout.preferredHeight: 24
                    Layout.preferredWidth: saveBtnRow.implicitWidth + 18
                    radius: 3
                    color: saveMouse.containsMouse ? "#323846" : "#252832"
                    border.color: "#343A48"
                    border.width: 1

                    Row {
                        id: saveBtnRow
                        anchors.centerIn: parent
                        spacing: 5

                        Text { text: "💾"; color: "#E8EAED"; font.pixelSize: 10; anchors.verticalCenter: parent.verticalCenter }
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

                // Botón 🔍 Search
                Rectangle {
                    Layout.preferredHeight: 24
                    Layout.preferredWidth: searchBtnRow.implicitWidth + 18
                    radius: 3
                    color: searchMouse.containsMouse || root.searchVisible ? "#323846" : "#252832"
                    border.color: root.searchVisible ? "#3B82F6" : "#343A48"
                    border.width: 1

                    Row {
                        id: searchBtnRow
                        anchors.centerIn: parent
                        spacing: 5

                        Text { text: "🔍"; color: "#E8EAED"; font.pixelSize: 10; anchors.verticalCenter: parent.verticalCenter }
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

                // Botón ⚙ Settings
                Rectangle {
                    Layout.preferredHeight: 24
                    Layout.preferredWidth: 26
                    radius: 3
                    color: settingsMouse.containsMouse ? "#323846" : "transparent"

                    Text {
                        anchors.centerIn: parent
                        text: "⚙"
                        color: "#9AA0A6"
                        font.pixelSize: 13
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

                Text { text: "🔍"; color: "#8F94A0"; font.pixelSize: 11 }

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
                    Text { anchors.centerIn: parent; text: "↑"; color: "#FFFFFF"; font.pixelSize: 12 }
                    MouseArea {
                        id: searchPrevMouse; anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                        onClicked: root.findPrev()
                    }
                }

                Rectangle {
                    width: 20; height: 20; radius: 3
                    color: searchNextMouse.containsMouse ? "#3D4250" : "transparent"
                    Text { anchors.centerIn: parent; text: "↓"; color: "#FFFFFF"; font.pixelSize: 12 }
                    MouseArea {
                        id: searchNextMouse; anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                        onClicked: root.findNext()
                    }
                }

                Rectangle {
                    width: 20; height: 20; radius: 3
                    color: searchCloseMouse.containsMouse ? "#3D4250" : "transparent"
                    Text { anchors.centerIn: parent; text: "✕"; color: "#8F94A0"; font.pixelSize: 11 }
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

                    // Botón 1: Explorador de Archivos (Páginas superpuestas)
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

                    // Botón 2: Lupa / Búsqueda
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

                    // Botón 3: Source Control / Git (Rama con nodos)
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 38
                        color: root.activeActivityTab === 2 && root.showSidebar ? "#252832" : "transparent"

                        Text {
                            anchors.centerIn: parent
                            text: "⑂"
                            font.pixelSize: 16
                            color: "#FFFFFF"
                            opacity: (root.activeActivityTab === 2 && root.showSidebar) ? 1.0 : 0.6
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

                    // Botón 4: Run / Debug
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 38
                        color: root.activeActivityTab === 3 && root.showSidebar ? "#252832" : "transparent"

                        Text {
                            anchors.centerIn: parent
                            text: "▷"
                            font.pixelSize: 15
                            font.bold: true
                            color: "#FFFFFF"
                            opacity: (root.activeActivityTab === 3 && root.showSidebar) ? 1.0 : 0.6
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

                    // Botón 5: Extensiones (Bloques modulares)
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 38
                        color: root.activeActivityTab === 4 && root.showSidebar ? "#252832" : "transparent"

                        Text {
                            anchors.centerIn: parent
                            text: "◫"
                            font.pixelSize: 15
                            color: "#FFFFFF"
                            opacity: (root.activeActivityTab === 4 && root.showSidebar) ? 1.0 : 0.6
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
                                spacing: 5

                                Text { text: "∨"; color: "#8F94A0"; font.pixelSize: 9; font.bold: true }
                                Text {
                                    text: "PROJECTS"
                                    color: "#8F94A0"
                                    font.pixelSize: 11
                                    font.bold: true
                                    font.family: "Segoe UI, Inter, Sans-serif"
                                }
                            }
                        }

                        // Carpeta raíz: ∨ 📁 jsfx_effects
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 22
                            color: "transparent"

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 16
                                spacing: 5

                                Text { text: "∨"; color: "#8F94A0"; font.pixelSize: 8 }
                                Text { text: "📁"; font.pixelSize: 11 }
                                Text {
                                    text: "jsfx_effects"
                                    color: "#CCCCCC"
                                    font.pixelSize: 11
                                    font.family: "Segoe UI, Inter, Sans-serif"
                                }
                            }
                        }

                        // Lista de archivos JSFX (idéntica a las capturas del usuario)
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

                                    // Icono de archivo JSFX con insignia
                                    Rectangle {
                                        width: 11; height: 13
                                        color: "transparent"
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

            // ── 3.3 ÁREA DE CÓDIGO CON NÚMEROS DE LÍNEA Y SINTAXIS COLOREADA ──
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "#181A20"
                clip: true

                Flickable {
                    id: editorFlickable
                    anchors.fill: parent
                    contentWidth: Math.max(width, codeColumn.width + 60)
                    contentHeight: Math.max(height, codeColumn.height + 40)
                    clip: true

                    Row {
                        id: codeColumn
                        spacing: 0
                        height: Math.max(codeTextEdit.implicitHeight, editorFlickable.height)

                        // GUTTER: NÚMEROS DE LÍNEA
                        Rectangle {
                            id: gutter
                            width: 38
                            height: Math.max(codeTextEdit.height, editorFlickable.height)
                            color: "#181A20"

                            Column {
                                anchors.fill: parent
                                anchors.topMargin: 8

                                Repeater {
                                model: Math.max(1, codeTextEdit.lineCount)
                                delegate: Text {
                                    width: 28
                                    text: (index + 1).toString()
                                    color: "#555C6E"
                                    font.family: "Monospace, Consolas, 'Courier New', Courier"
                                    font.pixelSize: 13
                                    horizontalAlignment: Text.AlignRight
                                }
                            }
                        }
                    }

                    // ZONA EDITABLE CON SINTAXIS COLOREADA
                    Item {
                        width: Math.max(editorFlickable.width - 50, codeTextEdit.contentWidth + 40)
                        height: Math.max(codeTextEdit.contentHeight + 20, editorFlickable.height)

                        // Capa 1 (Detrás): Renderizado con colores de sintaxis HTML
                        Text {
                            id: syntaxLayer
                            anchors.fill: parent
                            anchors.topMargin: 8
                            anchors.leftMargin: 8
                            textFormat: Text.RichText
                            font.family: "Monospace, Consolas, 'Courier New', Courier"
                            font.pixelSize: 13
                            wrapMode: Text.NoWrap
                            text: root.generateHighlightedHtml(codeTextEdit.text)
                        }

                        // Capa 2 (Encima): TextEdit nativo con texto transparente para edición
                        TextEdit {
                            id: codeTextEdit
                            anchors.fill: parent
                            anchors.topMargin: 8
                            anchors.leftMargin: 8
                            color: "transparent" // El texto lo pinta syntaxLayer con colores
                            selectedTextColor: "#FFFFFF"
                            selectionColor: "#264F78"
                            font.family: "Monospace, Consolas, 'Courier New', Courier"
                            font.pixelSize: 13
                            wrapMode: TextEdit.NoWrap
                            tabStopDistance: 32
                            cursorVisible: true

                            onTextChanged: {
                                root.isModified = true
                            }

                            Component.onCompleted: {
                                if (text.length === 0) root.loadTemplateDelay()
                            }
                        }
                    }
                }

                ScrollBar.vertical: ScrollBar {
                    active: true
                    width: 8
                }
                ScrollBar.horizontal: ScrollBar {
                    active: true
                    height: 8
                }
            }
        }
    }

    // =========================================================================
    // 4. CONSOLA INFERIOR INTEGRADA (CONSOLE)
    // =========================================================================
    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: 46
        color: "#16171B"
        border.color: "#202229"
        border.width: 1

        ColumnLayout {
            anchors.fill: parent
            anchors.leftMargin: 14
            anchors.rightMargin: 14
            anchors.topMargin: 4
            anchors.bottomMargin: 4
            spacing: 2

            Text {
                text: "Console"
                color: "#8F94A0"
                font.pixelSize: 11
                font.bold: true
                font.family: "Segoe UI, Inter, Sans-serif"
            }

            Text {
                text: root.statusMessage
                color: root.hasError ? "#FF4D4F" : "#52C41A"
                font.pixelSize: 11
                font.family: "Monospace, Consolas, 'Courier New'"
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
        }
    }
}

// =============================================================================
// 5. MOTOR DE RESALTADO DE SINTAXIS EN TIEMPO REAL (HTML RICH TEXT)
// =============================================================================

function generateHighlightedHtml(code) {
    if (!code) return ""

    var escaped = code
        .replace(/&/g, "&amp;")
        .replace(/</g, "&lt;")
        .replace(/>/g, "&gt;")

    var lines = escaped.split("\n")
    var result = []

    for (var i = 0; i < lines.length; ++i) {
        var line = lines[i]

        // 1. Comentarios de línea: // ...
        var commentIdx = line.indexOf("//")
        var commentPart = ""
        if (commentIdx !== -1) {
            commentPart = "<font color='#628A62'>" + line.substring(commentIdx) + "</font>"
            line = line.substring(0, commentIdx)
        }

        // 2. Directivas de sección: @init, @slider, @sample, @block
        if (line.trim().indexOf("@") === 0) {
            line = "<font color='#F0C674'><b>" + line + "</b></font>"
        } else if (line.indexOf("desc:") === 0) {
            // 3. Cabecera desc: ...
            var descVal = line.substring(5)
            line = "<font color='#DCDCDC'>desc:</font><font color='#E5A44E'>" + descVal + "</font>"
        } else if (line.indexOf("slider") === 0) {
            // 4. Definición de sliders: slider1:200<0,1000,10>Delay (ms)
            var colonIdx = line.indexOf(":")
            if (colonIdx !== -1) {
                var sName = line.substring(0, colonIdx + 1)
                var rest = line.substring(colonIdx + 1)
                var gtIdx = rest.indexOf("&gt;")
                if (gtIdx !== -1) {
                    var rangePart = rest.substring(0, gtIdx + 4)
                    var labelPart = rest.substring(gtIdx + 4)
                    line = "<font color='#DCDCDC'>" + sName + "</font>" +
                           "<font color='#7EC67E'>" + rangePart + "</font>" +
                           "<font color='#E5A44E'>" + labelPart + "</font>"
                } else {
                    line = "<font color='#DCDCDC'>" + sName + "</font><font color='#7EC67E'>" + rest + "</font>"
                }
            }
        } else {
            // 5. Palabras clave de control de flujo
            line = line.replace(/\b(if|then|else|while|loop)\b/g, "<font color='#F97583'><b>$1</b></font>")

            // 6. Operadores lógicos y matemáticos: =, *, /, +, -, >=, <=
            line = line.replace(/(&gt;=|&lt;=|==|!=|\+=|-=|\*=|\/=|[\+\*\/=])/g, "<font color='#F97583'>$1</font>")

            // 7. Variables de audio universales y canales
            line = line.replace(/\b(spl0|spl1|srate|num_ch)\b/g, "<font color='#E1E4E8'><b>$1</b></font>")

            // 8. Números
            line = line.replace(/\b([0-9]+(?:\.[0-9]+)?)\b/g, "<font color='#85E89D'>$1</font>")
        }

        result.push(line + commentPart)
    }

    // Unir con <br/> para preservar el espaciado vertical
    return "<pre style='margin:0; padding:0; line-height:1.25; font-family:Monospace;'>" + result.join("<br/>") + "</pre>"
}

// =============================================================================
// 6. LÓGICA DE COMPILACIÓN, GUARDADO, BÚSQUEDA Y NAVEGACIÓN
// =============================================================================

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

// =============================================================================
// 7. SCRIPTS COMPLETOS PARA EL ÁRBOL DE PROYECTO
// =============================================================================

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
