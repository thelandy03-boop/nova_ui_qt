import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "components"

/**
 * Editor JSFX Integrado (Dock) para NOVA Studio.
 * Arquitectura modular compuesta por submódulos especializados:
 * - JsfxActivityBar: barra lateral con herramientas VS Code
 * - JsfxFileTree: explorador de carpetas y proyectos JSFX
 * - JsfxTabBar: pestañas, plantillas y barra de compilación
 * - JsfxSearchBar: búsqueda interactiva con botones vectoriales
 * - JsfxEditorArea: editor de código EEL2 sincronizado píxel por píxel con gutter
 * - JsfxMinimap: representación visual miniatura del código
 * - JsfxStatusBar: consola y métricas de cursor estilo VS Code
 * - JsfxTemplates: biblioteca de presets DSP de procesamiento de audio
 */
Rectangle {
    id: root

    property int trackIndex: -1
    property int pluginIndex: -1
    property string pluginName: "ace_eq.jsfx"
    property string activeRackPluginFile: ""
    property int activeRackPluginIndex: -1
    property bool isNewPlugin: false
    property bool isModified: false

    property string statusMessage: "Compiled successfully"
    property bool hasError: false

    // Estado del explorador lateral izquierdo
    property bool showSidebar: (Qt.platform.os !== "android")
    property int activeActivityTab: 0 // 0: Explorer, 1: Search, 2: Git, 3: Run, 4: Extensions
    property string selectedFile: "ace_eq.jsfx"

    property bool searchVisible: false
    property string activeView: "code" // "code" o "ui"
    property bool isMaximized: (Qt.platform.os === "android")

    onActiveViewChanged: {
        if (activeView === "ui") {
            controlsView.refreshSliders()
        }
    }

    signal closeRequested()
    signal scriptApplied()
    signal reapackRequested()

    color: "#181A20"
    border.color: "#141518"
    border.width: 1

    // Biblioteca de plantillas DSP
    JsfxTemplates {
        id: templates
    }

    // Atajos de teclado locales
    Shortcut {
        sequence: "Ctrl+S"
        onActivated: root.compileAndApply()
    }
    Shortcut {
        sequence: "F5"
        onActivated: root.compileAndApply()
    }
    Shortcut {
        sequence: "F2"
        onActivated: fileTree.startRenameSelected()
    }
    Shortcut {
        sequence: "Ctrl+B"
        onActivated: root.showSidebar = !root.showSidebar
    }
    Shortcut {
        sequence: "Ctrl+F"
        onActivated: {
            root.searchVisible = !root.searchVisible
            if (root.searchVisible) searchBar.forceFocus()
        }
    }
    Shortcut {
        sequence: "Escape"
        onActivated: {
            if (fileTree.renamingIndex !== -1) {
                fileTree.cancelRename()
            } else if (root.searchVisible) {
                root.searchVisible = false
            } else {
                root.closeRequested()
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // 1. Barra de pestañas y herramientas
        JsfxTabBar {
            id: tabBar
            fileName: root.pluginName
            isModified: root.isModified
            searchActive: root.searchVisible
            sidebarOpen: root.showSidebar
            activeView: root.activeView
            isMaximized: root.isMaximized

            onCompileClicked: root.compileAndApply()
            onSaveClicked: root.saveAndInsert()
            onPasteClicked: editorArea.pasteFromClipboard()
            onSearchClicked: {
                root.searchVisible = !root.searchVisible
                if (root.searchVisible) searchBar.forceFocus()
            }
            onCloseClicked: root.closeRequested()
            onTemplateSelected: (name) => root.loadFileByName(name)
            onToggleSidebarClicked: root.showSidebar = !root.showSidebar
            onFormatClicked: root.formatScript()
            onRevertClicked: root.loadFileByName(root.selectedFile)
            onViewModeChanged: (mode) => root.activeView = mode
            onToggleMaximizeClicked: root.isMaximized = !root.isMaximized
            onReapackClicked: root.reapackRequested()
        }

        // 2. Barra de búsqueda desplegable
        JsfxSearchBar {
            id: searchBar
            visible: root.searchVisible
            Layout.preferredHeight: root.searchVisible ? 32 : 0

            onSearchChanged: (term) => editorArea.searchInCode(term)
            onPrevClicked: editorArea.findPrev(searchBar.searchText)
            onNextClicked: editorArea.findNext(searchBar.searchText)
            onCloseClicked: root.searchVisible = false
        }

        // 3. Cuerpo principal: Activity Bar + File Tree + Editor/UI
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // 3.1 Activity Bar (VS Code)
            JsfxActivityBar {
                activeTab: (root.activeView === "ui") ? 3 : root.activeActivityTab
                sidebarOpen: root.showSidebar
                onTabClicked: (idx) => {
                    if (idx === 1) {
                        root.activeActivityTab = 1
                        root.showSidebar = true
                        root.searchVisible = true
                        searchBar.forceFocus()
                    } else if (idx === 3) {
                        // Conmutar entre vista de Controles (Run/DSP) y Código
                        root.activeView = (root.activeView === "ui") ? "code" : "ui"
                    } else if (root.activeActivityTab === idx && root.showSidebar) {
                        root.showSidebar = false
                    } else {
                        root.activeActivityTab = idx
                        root.showSidebar = true
                    }
                }
            }

            // 3.2 Árbol de proyectos y archivos con renombrado seguro
            JsfxFileTree {
                id: fileTree
                Layout.preferredWidth: root.showSidebar ? 180 : 0
                Layout.fillHeight: true
                visible: root.showSidebar
                clip: true
                selectedFile: root.selectedFile
                onFileSelected: (name) => root.loadFileByName(name)
                onFileRenamed: (oldName, newName) => root.handleFileRenamed(oldName, newName)
                onFileCreated: (name) => root.handleFileCreated(name)
                onFileDeleted: (name) => root.handleFileDeleted(name)
            }

            // 3.3 Zona central de contenido: Editor de código o Vista de Controles UI
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                // Modo Código: Editor con Gutter alineado + Minimapa
                RowLayout {
                    anchors.fill: parent
                    spacing: 0
                    visible: root.activeView === "code"

                    JsfxEditorArea {
                        id: editorArea
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        onTextModified: {
                            fileTree.cancelRename()
                            root.isModified = true
                            root.customFileScripts[root.selectedFile] = editorArea.text
                        }
                    }

                    JsfxMinimap {
                        sourceText: editorArea.text
                        lineCount: editorArea.lineCount
                    }
                }

                // Modo UI: Controles y Sliders en tiempo real
                JsfxControlsView {
                    id: controlsView
                    anchors.fill: parent
                    visible: root.activeView === "ui"
                    trackIndex: root.trackIndex
                    pluginIndex: root.pluginIndex
                    scriptSource: editorArea.text

                    onEditCodeRequested: {
                        root.activeView = "code"
                    }
                    onInsertExampleSliderRequested: {
                        root.insertExampleSliders()
                        root.activeView = "code"
                    }
                }
            }
        }

        // 4. Barra de estado y consola
        JsfxStatusBar {
            id: statusBar
            statusMessage: root.statusMessage
            hasError: root.hasError
        }
    }

    // =========================================================================
    // LÓGICA DE COMPILACIÓN, GUARDADO Y ARCHIVOS
    // =========================================================================

    function compileAndApply() {
        // Guardar cambios en el búfer de memoria del archivo actual
        root.customFileScripts[root.selectedFile] = editorArea.text

        if (typeof AudioEngine === "undefined" || !AudioEngine.tracks) {
            root.statusMessage = "Error: El motor de audio no está activo"
            root.hasError = true
            statusBar.setLog(true, root.statusMessage)
            return
        }

        // 1. Compilación y validación de sintaxis en la VM JSFX
        var compileErr = ""
        if (typeof AudioEngine.tracks.compileJsfxScript === "function") {
            compileErr = AudioEngine.tracks.compileJsfxScript(editorArea.text)
        }
        if (compileErr && compileErr.length > 0) {
            root.statusMessage = compileErr
            root.hasError = true
            statusBar.setLog(true, compileErr)
            return
        }

        // 2. Si está asignado a un plugin existente en la pista, actualizarlo en vivo
        if (root.pluginIndex >= 0) {
            var updateErr = AudioEngine.tracks.updatePluginScript(root.trackIndex, root.pluginIndex, editorArea.text)
            if (updateErr && updateErr.length > 0) {
                root.statusMessage = updateErr
                root.hasError = true
                statusBar.setLog(true, updateErr)
                return
            }
        } else if (root.isNewPlugin && root.trackIndex >= 0) {
            AudioEngine.tracks.createCustomJsfxPlugin(root.trackIndex, root.pluginName.replace(".jsfx", ""), editorArea.text)
            root.isNewPlugin = false
        }

        // 3. Confirmación de compilación exitosa con feedback visual inmediato
        root.hasError = false
        root.isModified = false
        root.statusMessage = "Compilación exitosa: " + root.selectedFile
        statusBar.setLog(false, root.statusMessage)
        root.scriptApplied()
        controlsView.refreshSliders()
        root.activeView = "ui"
    }

    function insertExampleSliders() {
        var example = "slider1:250<10,2000,10>Delay Time (ms)\nslider2:0.4<0,0.95,0.01>Feedback\nslider3:0.5<0,1,0.01>Dry/Wet Mix\n\n"
        editorArea.text = example + editorArea.text
        root.isModified = true
        root.customFileScripts[root.selectedFile] = editorArea.text
        controlsView.refreshSliders()
    }

    // Caché en memoria para scripts editados y renombrados
    property var customFileScripts: ({})

    function handleFileRenamed(oldName, newName) {
        if (root.selectedFile === oldName) {
            root.selectedFile = newName
        }
        if (root.pluginName === oldName) {
            root.pluginName = newName
        }
        if (root.customFileScripts[oldName]) {
            root.customFileScripts[newName] = root.customFileScripts[oldName]
            delete root.customFileScripts[oldName]
        } else {
            root.customFileScripts[newName] = editorArea.text
        }
        root.statusMessage = "Archivo renombrado a " + newName
    }

    function handleFileCreated(name) {
        root.pluginIndex = -1
        root.activeRackPluginFile = ""
        root.activeRackPluginIndex = -1
        var baseContent = "// " + name + "\n// Script JSFX nuevo\n\ndesc: " + name.replace(".jsfx", "") + "\n\n@init\n// Inicializacion de variables\n\n@sample\nspl0 = spl0;\nspl1 = spl1;\n"
        root.customFileScripts[name] = baseContent
        root.loadFileByName(name)
        root.statusMessage = "Nuevo archivo creado: " + name
    }

    function handleFileDeleted(name) {
        if (root.customFileScripts && root.customFileScripts[name]) {
            delete root.customFileScripts[name]
        }
        root.statusMessage = "Archivo eliminado: " + name
    }

    function saveAndInsert() {
        if (root.pluginIndex < 0 && typeof AudioEngine !== "undefined" && AudioEngine.tracks) {
            var trk = (root.trackIndex >= 0) ? root.trackIndex : 0
            var plugName = root.pluginName.replace(".jsfx", "")
            var ok = AudioEngine.tracks.createCustomJsfxPlugin(trk, plugName, editorArea.text)
            if (ok) {
                root.trackIndex = trk
                var plugins = AudioEngine.tracks.trackPlugins(trk)
                root.pluginIndex = (plugins && plugins.length > 0) ? (plugins.length - 1) : 0
                root.activeRackPluginFile = root.pluginName
                root.activeRackPluginIndex = root.pluginIndex
                root.isNewPlugin = false
                root.isModified = false
                root.hasError = false
                root.statusMessage = "Plug-in insertado en la pista con éxito"
                statusBar.setLog(false, root.statusMessage)
                root.scriptApplied()
                controlsView.refreshSliders()
                root.activeView = "ui"
                return
            }
        }
        root.compileAndApply()
    }

    function loadFileByName(fname) {
        fileTree.cancelRename()
        root.selectedFile = fname
        root.pluginName = fname

        // Si el archivo cargado coincide con el plugin abierto desde el rack, mantener el enlace
        if (fname === root.activeRackPluginFile && root.activeRackPluginIndex >= 0) {
            root.pluginIndex = root.activeRackPluginIndex
        } else {
            // Si es otro script o archivo diferente, desvincular del plugin de la pista
            root.pluginIndex = -1
        }

        var content = (root.customFileScripts && root.customFileScripts[fname])
                      ? root.customFileScripts[fname]
                      : templates.getTemplate(fname)
        editorArea.loadContent(content)
        root.isModified = false
        root.statusMessage = "Compiled successfully"
        root.hasError = false
        controlsView.refreshSliders()
    }

    function openForPlugin(trkIdx, plgIdx, plgNm) {
        fileTree.cancelRename()
        root.trackIndex = trkIdx
        root.pluginIndex = plgIdx
        root.pluginName = (plgNm.indexOf(".jsfx") !== -1) ? plgNm : (plgNm.toLowerCase().replace(/ /g, "_") + ".jsfx")
        root.selectedFile = root.pluginName
        root.activeRackPluginFile = root.pluginName
        root.activeRackPluginIndex = plgIdx
        root.isNewPlugin = false
        root.isModified = false

        if (typeof AudioEngine !== "undefined" && AudioEngine.tracks) {
            var src = AudioEngine.tracks.getPluginScript(trkIdx, plgIdx)
            if (src && src.length > 0) {
                editorArea.loadContent(src)
            } else {
                editorArea.loadContent(templates.getTemplate(root.pluginName))
            }
        } else {
            editorArea.loadContent(templates.getTemplate(root.pluginName))
        }

        root.isModified = false
        root.statusMessage = "Compiled successfully"
        root.hasError = false
        controlsView.refreshSliders()
    }

    function openNewScript(trkIdx) {
        fileTree.cancelRename()
        root.trackIndex = trkIdx
        root.pluginIndex = -1
        root.activeRackPluginFile = ""
        root.activeRackPluginIndex = -1
        root.pluginName = "ace_eq.jsfx"
        root.selectedFile = "ace_eq.jsfx"
        root.isNewPlugin = true
        editorArea.loadContent(templates.getTemplate("ace_eq.jsfx"))
        root.isModified = false
        root.statusMessage = "Compiled successfully"
        root.hasError = false
        editorArea.updateCursorPosition()
        controlsView.refreshSliders()
    }

    function formatScript() {
        editorArea.text = templates.formatScript(editorArea.text)
    }

    function loadCustomScriptCode(code, name) {
        fileTree.cancelRename()
        root.pluginIndex = -1
        root.pluginName = (name.indexOf(".jsfx") !== -1) ? name : (name + ".jsfx")
        root.selectedFile = root.pluginName
        if (!root.customFileScripts) root.customFileScripts = ({})
        root.customFileScripts[root.selectedFile] = code
        editorArea.loadContent(code)
        root.isModified = false
        root.statusMessage = "Cargado plugin ReaPack: " + name
        root.hasError = false
        controlsView.refreshSliders()
    }

    Component.onCompleted: {
        if (editorArea.text.length === 0) {
            editorArea.text = templates.getTemplate("ace_eq.jsfx")
        }
    }
}
