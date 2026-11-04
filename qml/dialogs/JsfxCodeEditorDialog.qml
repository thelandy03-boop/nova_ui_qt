import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

/**
 * Editor de Código en Vivo para Scripts JSFX (REAPER EEL2).
 * Permite ver, modificar, compilar en caliente y crear efectos DSP
 * sin interrumpir la reproducción de audio.
 */
Rectangle {
    id: root

    property int trackIndex: -1
    property int pluginIndex: -1
    property string pluginName: "Script JSFX"
    property bool isNewPlugin: false

    signal scriptApplied()
    signal closed()

    anchors.fill: parent
    color: "#D90B0D13" // Fondo semitransparente oscuro
    z: 10000

    // Evitar que los clics traspasen al fondo
    MouseArea {
        anchors.fill: parent
        onClicked: {}
    }

    // Atajos de teclado para flujo de trabajo rápido
    Shortcut {
        sequence: "Ctrl+S"
        onActivated: root.compileAndApply()
    }
    Shortcut {
        sequence: "F5"
        onActivated: root.compileAndApply()
    }
    Shortcut {
        sequence: "Escape"
        onActivated: root.closeDialog()
    }

    // Contenedor principal de la ventana del editor
    Rectangle {
        id: windowCard
        anchors.centerIn: parent
        width: Math.min(parent.width - 40, 960)
        height: Math.min(parent.height - 40, 680)
        radius: 10
        color: "#181B22"
        border.color: "#2C313E"
        border.width: 1
        clip: true

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            // ── CABECERA DEL EDITOR ─────────────────────────────────────────────
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 52
                color: "#12141A"
                border.color: "#232733"
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    spacing: 12

                    // Icono de código
                    Rectangle {
                        Layout.preferredWidth: 32
                        Layout.preferredHeight: 32
                        radius: 6
                        color: "#2563EB"

                        Text {
                            anchors.centerIn: parent
                            text: "</>"
                            color: "#FFFFFF"
                            font.pixelSize: 13
                            font.bold: true
                            font.family: "Monospace"
                        }
                    }

                    ColumnLayout {
                        spacing: 1
                        Text {
                            text: root.isNewPlugin ? "Nuevo Script JSFX (EEL2)" : ("Editor JSFX — " + root.pluginName)
                            color: "#F1F5F9"
                            font.pixelSize: 14
                            font.bold: true
                        }
                        Text {
                            text: "Procesador matemático en tiempo real • Compatible con REAPER JSFX"
                            color: "#94A3B8"
                            font.pixelSize: 11
                        }
                    }

                    Item { Layout.fillWidth: true }

                    // Selector de Plantillas
                    Text {
                        text: "Cargar plantilla:"
                        color: "#94A3B8"
                        font.pixelSize: 11
                    }

                    ComboBox {
                        id: templateCombo
                        Layout.preferredWidth: 200
                        Layout.preferredHeight: 32
                        model: [
                            "-- Seleccionar plantilla --",
                            "Saturador de Tubos analógico",
                            "Filtro Pasa-Bajos con resonancia",
                            "Stereo Widener Mid/Side",
                            "Delay Estéreo simple",
                            "Ganancia y Balance básico"
                        ]

                        onActivated: (index) => {
                            if (index === 1) root.loadTemplateTube()
                            else if (index === 2) root.loadTemplateFilter()
                            else if (index === 3) root.loadTemplateWidener()
                            else if (index === 4) root.loadTemplateDelay()
                            else if (index === 5) root.loadTemplateGain()
                            templateCombo.currentIndex = 0
                        }
                    }

                    // Botón Cerrar
                    Rectangle {
                        Layout.preferredWidth: 30
                        Layout.preferredHeight: 30
                        radius: 6
                        color: closeArea.containsMouse ? "#EF4444" : "#232733"

                        Text {
                            anchors.centerIn: parent
                            text: "✕"
                            color: "#FFFFFF"
                            font.pixelSize: 13
                            font.bold: true
                        }

                        MouseArea {
                            id: closeArea
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.closeDialog()
                        }
                    }
                }
            }

            // ── CUERPO DEL EDITOR DE CÓDIGO CON NÚMEROS DE LÍNEA ────────────────
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "#0F1117"

                RowLayout {
                    anchors.fill: parent
                    spacing: 0

                    // Columna de Números de Línea
                    Rectangle {
                        Layout.preferredWidth: 46
                        Layout.fillHeight: true
                        color: "#141720"
                        border.color: "#1E222D"
                        border.width: 1

                        Flickable {
                            id: lineFlick
                            anchors.fill: parent
                            anchors.topMargin: 12
                            anchors.bottomMargin: 12
                            contentY: codeFlick.contentY
                            interactive: false
                            clip: true

                            Text {
                                width: parent.width
                                text: root.calculateLineNumbers(codeEditor.text)
                                color: "#4B5563"
                                font.family: "Monospace"
                                font.pixelSize: 12
                                horizontalAlignment: Text.AlignRight
                                rightPadding: 10
                            }
                        }
                    }

                    // Área de texto del código fuente
                    Flickable {
                        id: codeFlick
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        contentWidth: codeEditor.paintedWidth + 40
                        contentHeight: codeEditor.paintedHeight + 40
                        clip: true
                        boundsBehavior: Flickable.StopAtBounds

                        ScrollBar.vertical: ScrollBar {
                            active: true
                            policy: ScrollBar.AsNeeded
                        }
                        ScrollBar.horizontal: ScrollBar {
                            active: true
                            policy: ScrollBar.AsNeeded
                        }

                        TextEdit {
                            id: codeEditor
                            x: 14
                            y: 12
                            width: Math.max(codeFlick.width - 28, paintedWidth)
                            color: "#E2E8F0"
                            selectionColor: "#2563EB"
                            selectedTextColor: "#FFFFFF"
                            font.family: "Monospace"
                            font.pixelSize: 12
                            font.weight: Font.Normal
                            selectByMouse: true
                            wrapMode: TextEdit.NoWrap
                            tabStopDistance: 32
                            cursorVisible: true

                            onTextChanged: {
                                statusBanner.text = "Modificado (Presiona Ctrl+S para compilar)"
                                statusBanner.color = "#94A3B8"
                                statusBannerBox.color = "#1E222D"
                            }
                        }
                    }
                }
            }

            // ── BARRA INFERIOR: CONSOLA DE COMPILACIÓN Y ACCIONES ───────────────
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 52
                color: "#12141A"
                border.color: "#232733"
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    spacing: 12

                    // Banner de estado de compilación
                    Rectangle {
                        id: statusBannerBox
                        Layout.fillWidth: true
                        Layout.preferredHeight: 34
                        radius: 6
                        color: "#16281E"
                        border.color: statusBanner.isError ? "#EF4444" : "#10B981"
                        border.width: 1

                        property bool isError: false

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            spacing: 8

                            Text {
                                text: statusBannerBox.isError ? "⚠" : "✓"
                                color: statusBannerBox.isError ? "#EF4444" : "#10B981"
                                font.pixelSize: 14
                                font.bold: true
                            }

                            Text {
                                id: statusBanner
                                text: "Código listo para ejecutar en tiempo real"
                                color: "#10B981"
                                font.pixelSize: 12
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                                property bool isError: false
                            }
                        }
                    }

                    // Botón Compilar y Aplicar en Caliente
                    Rectangle {
                        Layout.preferredWidth: 180
                        Layout.preferredHeight: 34
                        radius: 6
                        color: compileArea.containsMouse ? "#1D4ED8" : "#2563EB"

                        Row {
                            anchors.centerIn: parent
                            spacing: 6
                            Text { text: "▶"; color: "#FFFFFF"; font.pixelSize: 12 }
                            Text { text: "Compilar y Aplicar"; color: "#FFFFFF"; font.pixelSize: 12; font.bold: true }
                        }

                        MouseArea {
                            id: compileArea
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.compileAndApply()
                        }
                    }
                }
            }
        }
    }

    function calculateLineNumbers(text) {
        var count = (text.match(/\n/g) || []).length + 1
        var result = ""
        for (var i = 1; i <= count; ++i) {
            result += i + "\n"
        }
        return result
    }

    function openForPlugin(trkIdx, plgIdx, plgName) {
        root.trackIndex = trkIdx
        root.pluginIndex = plgIdx
        root.pluginName = plgName
        root.isNewPlugin = false

        if (typeof AudioEngine !== "undefined" && AudioEngine.tracks) {
            var src = AudioEngine.tracks.getPluginScript(trkIdx, plgIdx)
            codeEditor.text = src.length > 0 ? src : root.defaultScript()
        } else {
            codeEditor.text = root.defaultScript()
        }

        statusBanner.text = "Código cargado correctamente. Listo para editar."
        statusBanner.color = "#10B981"
        statusBannerBox.isError = false
        statusBannerBox.color = "#16281E"
        root.visible = true
    }

    function openNewScript(trkIdx) {
        root.trackIndex = trkIdx
        root.pluginIndex = -1
        root.pluginName = "Nuevo Efecto JSFX"
        root.isNewPlugin = true
        root.loadTemplateTube()
        root.visible = true
    }

    function compileAndApply() {
        if (typeof AudioEngine === "undefined" || !AudioEngine.tracks) {
            statusBanner.text = "Error: El motor de audio no está activo"
            statusBanner.color = "#EF4444"
            statusBannerBox.isError = true
            statusBannerBox.color = "#2E1517"
            return
        }

        if (root.isNewPlugin) {
            var ok = AudioEngine.tracks.createCustomJsfxPlugin(root.trackIndex, "Nuevo Script JSFX", codeEditor.text)
            if (ok) {
                statusBanner.text = "✓ Script creado y conectado a la pista con éxito"
                statusBanner.color = "#10B981"
                statusBannerBox.isError = false
                statusBannerBox.color = "#16281E"
                root.scriptApplied()
                root.closeDialog()
            } else {
                statusBanner.text = "Error al compilar el nuevo script"
                statusBanner.color = "#EF4444"
                statusBannerBox.isError = true
                statusBannerBox.color = "#2E1517"
            }
        } else {
            var err = AudioEngine.tracks.updatePluginScript(root.trackIndex, root.pluginIndex, codeEditor.text)
            if (!err || err.length === 0) {
                statusBanner.text = "✓ Compilación exitosa: audio actualizado en tiempo real"
                statusBanner.color = "#10B981"
                statusBannerBox.isError = false
                statusBannerBox.color = "#16281E"
                root.scriptApplied()
            } else {
                statusBanner.text = "⚠ " + err
                statusBanner.color = "#EF4444"
                statusBannerBox.isError = true
                statusBannerBox.color = "#2E1517"
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
    }

    function loadTemplateGain() {
        codeEditor.text = defaultScript()
    }
}
