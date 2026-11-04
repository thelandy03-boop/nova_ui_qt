import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

/**
 * Vista de Interfaz Gráfica (UI) para plugins JSFX en disposición VERTICAL.
 * Reinterpretación auténtica del entorno Cockos REAPER / Consola de estudio:
 * - Tiras verticales de fader (channel strips de parámetros) en fila horizontal
 * - Faders verticales con riel 3D biselado y Fader Caps metálicos auténticos
 * - Display numérico biselado hundido en la base de cada fader
 * - Arrastre vertical ergonómico, rueda de ratón y doble clic para restaurar
 */
Rectangle {
    id: root

    property int trackIndex: -1
    property int pluginIndex: -1
    property string scriptSource: ""
    property string pluginTitle: "JSFX Plugin"
    property var slidersList: []

    signal editCodeRequested()
    signal parameterChanged(int paramIndex, real value)
    signal insertExampleSliderRequested()

    color: "#181A20"

    onScriptSourceChanged: refreshSliders()
    Component.onCompleted: refreshSliders()

    function refreshSliders() {
        var list = []
        var detectedTitle = ""

        // 1. Parser C++ si está disponible
        if (typeof AudioEngine !== "undefined" && AudioEngine.tracks &&
            typeof AudioEngine.tracks.parseJsfxSliders === "function") {
            try {
                list = AudioEngine.tracks.parseJsfxSliders(root.scriptSource)
                if (typeof AudioEngine.tracks.getJsfxDescription === "function") {
                    detectedTitle = AudioEngine.tracks.getJsfxDescription(root.scriptSource)
                }
            } catch (e) {
                console.warn("[JsfxControlsView] Error parse C++:", e)
            }
        }

        // 2. Parser JavaScript de respaldo por expresiones regulares
        if ((!list || list.length === 0) && root.scriptSource && root.scriptSource.length > 0) {
            var lines = root.scriptSource.split(/\r?\n/)
            var sRegex = /^slider(\d+)\s*:\s*([-\d.]+)\s*<\s*([-\d.]+)\s*,\s*([-\d.]+)(?:\s*,\s*([-\d.]+))?[^>]*>\s*(.*)$/
            var idx = 0
            for (var i = 0; i < lines.length; i++) {
                var line = lines[i].trim()
                if (line.toLowerCase().indexOf("desc:") === 0) {
                    detectedTitle = line.substring(5).trim()
                } else if (line.indexOf("@") === 0) {
                    if (line.indexOf("@init") === 0 || line.indexOf("@sample") === 0 || line.indexOf("@slider") === 0) {
                        break
                    }
                } else {
                    var m = line.match(sRegex)
                    if (m) {
                        var sNum = parseInt(m[1])
                        var defVal = parseFloat(m[2])
                        var minVal = parseFloat(m[3])
                        var maxVal = parseFloat(m[4])
                        var stepVal = m[5] ? parseFloat(m[5]) : 0.01
                        var rawName = m[6] ? m[6].trim() : ("slider" + sNum)
                        var unitStr = ""
                        var pOpen = rawName.lastIndexOf("(")
                        if (pOpen !== -1 && rawName.endsWith(")")) {
                            unitStr = rawName.substring(pOpen + 1, rawName.length - 1).trim()
                        }

                        list.push({
                            index: idx++,
                            sliderNum: sNum,
                            name: rawName,
                            value: defVal,
                            min: minVal,
                            max: maxVal,
                            step: stepVal,
                            unit: unitStr
                        })
                    }
                }
            }
        }

        if (detectedTitle && detectedTitle.length > 0) {
            root.pluginTitle = detectedTitle
        }
        root.slidersList = list
    }

    function resetAllSliders() {
        for (var i = 0; i < slidersList.length; i++) {
            var sl = slidersList[i]
            root.parameterChanged(sl.index, sl.value)
            if (typeof AudioEngine !== "undefined" && AudioEngine.tracks &&
                root.trackIndex >= 0 && root.pluginIndex >= 0) {
                AudioEngine.tracks.setPluginParameter(root.trackIndex, root.pluginIndex, sl.index, sl.value)
            }
        }
        refreshSliders()
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // =====================================================================
        // 1. CABECERA REAPER FX WINDOW (JS: Título y botón Reset)
        // =====================================================================
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 36
            border.color: "#16171E"
            border.width: 1

            gradient: Gradient {
                GradientStop { position: 0.0; color: "#2B2D37" }
                GradientStop { position: 1.0; color: "#20222A" }
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                spacing: 10

                // Título con prefijo "JS:" sobrio
                Row {
                    spacing: 6
                    Layout.alignment: Qt.AlignVCenter

                    Text {
                        text: "JS:"
                        color: "#E5A93C"
                        font.pixelSize: 12
                        font.bold: true
                        font.family: "Segoe UI, Inter, Sans-serif"
                    }

                    Text {
                        text: root.pluginTitle || "Effect"
                        color: "#F1F5F9"
                        font.pixelSize: 12
                        font.bold: true
                        font.family: "Segoe UI, Inter, Sans-serif"
                    }
                }

                Item { Layout.fillWidth: true }
            }
        }

        // =====================================================================
        // 2. CONTENIDO PRINCIPAL: TIRAS VERTICALES DE FADERS ESTILO REAPER
        // =====================================================================
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            // Estado sin sliders
            Item {
                anchors.fill: parent
                visible: root.slidersList.length === 0

                Text {
                    anchors.centerIn: parent
                    text: "Este script JSFX no define sliders (slider1, slider2...)"
                    color: "#64748B"
                    font.pixelSize: 12
                    font.family: "Segoe UI, Inter, Sans-serif"
                }
            }

            // Tiras verticales alineadas en fila horizontal
            ListView {
                id: stripsList
                anchors.fill: parent
                anchors.margins: 14
                orientation: ListView.Horizontal
                flickableDirection: Flickable.HorizontalFlick
                boundsBehavior: (Qt.platform.os === "android") ? Flickable.DragOverBounds : Flickable.StopAtBounds
                visible: root.slidersList.length > 0
                clip: true
                model: root.slidersList
                spacing: 12

                delegate: Rectangle {
                    id: stripCard
                    width: Math.max(108, Math.min(150, (stripsList.width - (root.slidersList.length - 1) * 12) / Math.max(1, root.slidersList.length)))
                    height: Math.max(160, stripsList.height - 4)
                    radius: 4
                    color: stripMouse.containsMouse ? "#232631" : "#1D1F27"
                    border.color: stripMouse.containsMouse ? "#3B4154" : "#272A36"
                    border.width: 1

                    property var param: modelData
                    property real currentVal: param.value
                    property real normVal: (param.max > param.min) ? Math.max(0.0, Math.min(1.0, (currentVal - param.min) / (param.max - param.min))) : 0.0

                    function triggerHaptic() {
                        if (typeof AudioEngine !== "undefined" && AudioEngine.hapticFeedback) {
                            AudioEngine.hapticFeedback()
                        }
                    }

                    function updateFromNorm(nVal) {
                        nVal = Math.max(0.0, Math.min(1.0, nVal))
                        var realVal = param.min + nVal * (param.max - param.min)
                        if (param.step > 0) {
                            realVal = Math.round(realVal / param.step) * param.step
                        }
                        currentVal = realVal
                        root.parameterChanged(param.index, realVal)

                        if (typeof AudioEngine !== "undefined" && AudioEngine.tracks &&
                            root.trackIndex >= 0 && root.pluginIndex >= 0) {
                            AudioEngine.tracks.setPluginParameter(root.trackIndex, root.pluginIndex, param.index, realVal)
                        }
                    }

                    function stepChange(direction) {
                        var delta = (param.step > 0) ? (param.step * direction) : ((param.max - param.min) * 0.02 * direction)
                        var newVal = Math.max(param.min, Math.min(param.max, currentVal + delta))
                        if (param.step > 0) {
                            newVal = Math.round(newVal / param.step) * param.step
                        }
                        currentVal = newVal
                        triggerHaptic()
                        root.parameterChanged(param.index, newVal)

                        if (typeof AudioEngine !== "undefined" && AudioEngine.tracks &&
                            root.trackIndex >= 0 && root.pluginIndex >= 0) {
                            AudioEngine.tracks.setPluginParameter(root.trackIndex, root.pluginIndex, param.index, newVal)
                        }
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 6

                        // 1. Rótulo superior del parámetro
                        Text {
                            Layout.fillWidth: true
                            text: stripCard.param.name
                            color: "#D8DCE6"
                            font.pixelSize: 11
                            font.bold: true
                            font.family: "Segoe UI, Inter, Sans-serif"
                            horizontalAlignment: Text.AlignHCenter
                            elide: Text.ElideRight
                        }

                        // 2. Fader vertical de REAPER con Touch Target ampliado
                        Item {
                            id: verticalFader
                            Layout.fillWidth: true
                            Layout.fillHeight: true

                            // Riel vertical 3D centrado
                            Rectangle {
                                id: vertRail
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                width: 6
                                radius: 3
                                color: "#0C0D11"
                                border.color: "#050608"
                                border.width: 1

                                // Micro-bisel derecho para profundidad 3D
                                Rectangle {
                                    anchors.left: parent.right
                                    anchors.top: parent.top
                                    anchors.bottom: parent.bottom
                                    width: 1
                                    color: "#383D4E"
                                    opacity: 0.6
                                }

                                // Barra de relleno vertical desde el fondo (0% abajo, 100% arriba)
                                Rectangle {
                                    anchors.bottom: parent.bottom
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    width: parent.width
                                    height: Math.max(4, parent.height * stripCard.normVal)
                                    radius: 3
                                    color: "#3A6EA5"
                                }
                            }

                            // Fader Cap metálico horizontal de REAPER
                            Rectangle {
                                id: vertCap
                                anchors.horizontalCenter: parent.horizontalCenter
                                y: Math.max(0, Math.min(parent.height - height, (parent.height - height) * (1.0 - stripCard.normVal)))
                                width: 30
                                height: 16
                                radius: 2
                                border.color: "#111216"
                                border.width: 1
                                antialiasing: true

                                // Gradiente metálico horizontal estilo Cockos REAPER
                                gradient: Gradient {
                                    GradientStop { position: 0.00; color: "#E0E2E8" }
                                    GradientStop { position: 0.15; color: "#FFFFFF" }
                                    GradientStop { position: 0.35; color: "#7B8090" }
                                    GradientStop { position: 0.48; color: "#3B3F4A" }
                                    GradientStop { position: 0.50; color: "#000000" } // Muesca central
                                    GradientStop { position: 0.52; color: "#3B3F4A" }
                                    GradientStop { position: 0.65; color: "#7B8090" }
                                    GradientStop { position: 0.85; color: "#CBD0DC" }
                                    GradientStop { position: 1.00; color: "#585D6C" }
                                }

                                // Muesca horizontal central de posición
                                Rectangle {
                                    anchors.centerIn: parent
                                    width: 20
                                    height: 1.5
                                    color: stripMouse.drag.active ? "#60A5FA" : "#000000"

                                    // Micro-bisel inferior
                                    Rectangle {
                                        anchors.top: parent.bottom
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        height: 1
                                        color: "#FFFFFF"
                                        opacity: 0.35
                                    }
                                }

                                // Bisel luminoso superior
                                Rectangle {
                                    anchors.top: parent.top
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.topMargin: 1
                                    anchors.leftMargin: 1
                                    anchors.rightMargin: 1
                                    height: 1
                                    color: "#FFFFFF"
                                    opacity: 0.4
                                }
                            }

                            // Área táctil del fader optimizada para Android
                            MouseArea {
                                id: stripMouse
                                anchors.fill: parent
                                anchors.margins: (Qt.platform.os === "android") ? -16 : -4
                                preventStealing: true
                                hoverEnabled: true
                                cursorShape: Qt.SizeVerCursor

                                function applyY(mouseY) {
                                    var usable = Math.max(1, verticalFader.height - vertCap.height)
                                    var n = 1.0 - ((mouseY - vertCap.height / 2) / usable)
                                    stripCard.updateFromNorm(n)
                                }

                                onPressed: (mouse) => {
                                    stripCard.triggerHaptic()
                                    applyY(mouse.y)
                                }
                                onPositionChanged: (mouse) => {
                                    if (pressed) applyY(mouse.y)
                                }
                                onWheel: (wheel) => {
                                    var delta = (wheel.angleDelta.y > 0) ? 0.02 : -0.02
                                    stripCard.updateFromNorm(stripCard.normVal + delta)
                                }
                                onDoubleClicked: {
                                    stripCard.triggerHaptic()
                                    stripCard.currentVal = stripCard.param.value
                                    stripCard.updateFromNorm((stripCard.param.value - stripCard.param.min) / (stripCard.param.max - stripCard.param.min))
                                }
                            }
                        }

                        // 3. Fila de Micro-Ajuste táctil paso a paso (▲ y ▼) + Display numérico
                        RowLayout {
                            Layout.fillWidth: true
                            Layout.preferredHeight: (Qt.platform.os === "android") ? 28 : 22
                            spacing: 4

                            // Botón paso abajo ▼
                            Rectangle {
                                Layout.preferredWidth: (Qt.platform.os === "android") ? 24 : 18
                                Layout.fillHeight: true
                                radius: 2
                                color: downMouse.containsMouse ? "#3A3F50" : "#242732"
                                border.color: "#323746"
                                border.width: 1

                                Text {
                                    anchors.centerIn: parent
                                    text: "▼"
                                    color: "#94A3B8"
                                    font.pixelSize: 8
                                }

                                MouseArea {
                                    id: downMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: stripCard.stepChange(-1)
                                }
                            }

                            // Display numérico biselado central
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                radius: 2
                                color: "#0E0F14"
                                border.color: "#282B37"
                                border.width: 1

                                Rectangle {
                                    anchors.top: parent.top
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    height: 1
                                    color: "#000000"
                                    opacity: 0.6
                                }

                                Text {
                                    anchors.centerIn: parent
                                    text: {
                                        var num = stripCard.currentVal
                                        var fmt = (Math.abs(num) >= 100) ? num.toFixed(0) : ((Math.abs(num) >= 10) ? num.toFixed(1) : num.toFixed(2))
                                        return fmt + (stripCard.param.unit ? (" " + stripCard.param.unit) : "")
                                    }
                                    color: "#60A5FA"
                                    font.pixelSize: (Qt.platform.os === "android") ? 11 : 10
                                    font.family: "Monospace, Consolas, Courier New"
                                    horizontalAlignment: Text.AlignHCenter
                                }
                            }

                            // Botón paso arriba ▲
                            Rectangle {
                                Layout.preferredWidth: (Qt.platform.os === "android") ? 24 : 18
                                Layout.fillHeight: true
                                radius: 2
                                color: upMouse.containsMouse ? "#3A3F50" : "#242732"
                                border.color: "#323746"
                                border.width: 1

                                Text {
                                    anchors.centerIn: parent
                                    text: "▲"
                                    color: "#94A3B8"
                                    font.pixelSize: 8
                                }

                                MouseArea {
                                    id: upMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: stripCard.stepChange(1)
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
