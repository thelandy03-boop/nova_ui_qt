import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

/**
 * Vista de Interfaz Gráfica (UI) para plugins JSFX.
 * Reinterpretación auténtica del entorno Cockos REAPER FX:
 * - Cabecera industrial de ventana de plugin (prefijo JS, botón Param, Presets y Reset)
 * - Faders horizontales estilo REAPER con riel biselado 3D y fader caps metálicos
 * - Displays de valor numérico biselados hundidos
 * - Control de alta precisión con arrastre, rueda de ratón y doble clic para resetear
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
        // 1. CABECERA REAPER FX WINDOW (Bypass [✓], JS: Título, Param, Preset)
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

                // Botón [ ↺ Reset ] 100% funcional (restablece los faders a sus valores por defecto)
                Rectangle {
                    Layout.preferredHeight: 22
                    Layout.preferredWidth: resetRow.implicitWidth + 16
                    radius: 3
                    color: resetMouse.containsMouse ? "#3E4250" : "#2A2D37"
                    border.color: "#434757"
                    border.width: 1

                    Row {
                        id: resetRow
                        anchors.centerIn: parent
                        spacing: 5
                        Text { text: "↺"; color: "#94A3B8"; font.pixelSize: 10 }
                        Text { text: "Reset"; color: "#CBD5E1"; font.pixelSize: 10; font.bold: true }
                    }

                    MouseArea {
                        id: resetMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.resetAllSliders()
                    }
                }
            }
        }

        // =====================================================================
        // 2. CONTENIDO PRINCIPAL: FADERS JSFX DE REAPER
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

            // Lista estructurada de Faders estilo REAPER
            ListView {
                id: slidersView
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                anchors.topMargin: 14
                anchors.bottomMargin: 14
                visible: root.slidersList.length > 0
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                model: root.slidersList
                spacing: 6

                delegate: Rectangle {
                    id: rowDelegate
                    width: slidersView.width
                    height: 40
                    radius: 3
                    color: rowMouse.containsMouse ? "#242732" : "#1E2028"
                    border.color: rowMouse.containsMouse ? "#3A3F50" : "#282B37"
                    border.width: 1

                    property var param: modelData
                    property real currentVal: param.value
                    property real normVal: (param.max > param.min) ? Math.max(0.0, Math.min(1.0, (currentVal - param.min) / (param.max - param.min))) : 0.0

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

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 14
                        anchors.rightMargin: 14
                        spacing: 14

                        // 1. Etiqueta del slider estilo Reaper
                        Text {
                            Layout.preferredWidth: (parent.width > 600) ? 190 : 140
                            text: rowDelegate.param.name
                            color: "#D8DCE6"
                            font.pixelSize: 11
                            font.bold: true
                            font.family: "Segoe UI, Inter, Sans-serif"
                            elide: Text.ElideRight
                        }

                        // 2. Riel y Fader metálico de REAPER
                        Item {
                            id: faderTrack
                            Layout.fillWidth: true
                            Layout.preferredHeight: 28

                            // Canal / Riel horizontal con profundidad 3D biselada
                            Rectangle {
                                id: slotBase
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                height: 5
                                radius: 2
                                color: "#0D0E12"
                                border.color: "#050608"
                                border.width: 1

                                // Micro-bisel inferior para profundidad de hardware
                                Rectangle {
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.bottom: parent.bottom
                                    anchors.bottomMargin: -1
                                    height: 1
                                    color: "#383D4E"
                                    opacity: 0.6
                                }

                                // Barra de relleno azul clásico de REAPER
                                Rectangle {
                                    anchors.left: parent.left
                                    anchors.top: parent.top
                                    anchors.bottom: parent.bottom
                                    width: Math.max(4, parent.width * rowDelegate.normVal)
                                    radius: 2
                                    color: "#3A6EA5"
                                }
                            }

                            // Fader Cap metálico auténtico de REAPER
                            Rectangle {
                                id: faderCap
                                x: Math.max(0, Math.min(parent.width - width, (parent.width - width) * rowDelegate.normVal))
                                anchors.verticalCenter: parent.verticalCenter
                                width: 16
                                height: 24
                                radius: 2
                                border.color: "#111216"
                                border.width: 1
                                antialiasing: true

                                // Gradiente metálico de 9 paradas estilo Cockos REAPER
                                gradient: Gradient {
                                    GradientStop { position: 0.00; color: "#E0E2E8" }
                                    GradientStop { position: 0.15; color: "#FFFFFF" }
                                    GradientStop { position: 0.35; color: "#7B8090" }
                                    GradientStop { position: 0.48; color: "#3B3F4A" }
                                    GradientStop { position: 0.50; color: "#000000" } // Ranura central
                                    GradientStop { position: 0.52; color: "#3B3F4A" }
                                    GradientStop { position: 0.65; color: "#7B8090" }
                                    GradientStop { position: 0.85; color: "#CBD0DC" }
                                    GradientStop { position: 1.00; color: "#585D6C" }
                                }

                                // Línea de posición central con micro-relieve
                                Rectangle {
                                    anchors.centerIn: parent
                                    width: 1.5
                                    height: 14
                                    color: rowMouse.drag.active ? "#60A5FA" : "#000000"

                                    // Micro bisel blanco a la derecha
                                    Rectangle {
                                        anchors.left: parent.right
                                        anchors.top: parent.top
                                        anchors.bottom: parent.bottom
                                        width: 1
                                        color: "#FFFFFF"
                                        opacity: 0.35
                                    }
                                }

                                // Bisel luminoso izquierdo
                                Rectangle {
                                    anchors.left: parent.left
                                    anchors.top: parent.top
                                    anchors.bottom: parent.bottom
                                    anchors.leftMargin: 1
                                    anchors.topMargin: 1
                                    anchors.bottomMargin: 1
                                    width: 1
                                    color: "#FFFFFF"
                                    opacity: 0.4
                                }
                            }

                            MouseArea {
                                id: rowMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor

                                function applyPos(mouse) {
                                    var n = mouse.x / width
                                    rowDelegate.updateFromNorm(n)
                                }

                                onPressed: (mouse) => applyPos(mouse)
                                onPositionChanged: (mouse) => {
                                    if (pressed) applyPos(mouse)
                                }
                                onWheel: (wheel) => {
                                    var delta = (wheel.angleDelta.y > 0) ? 0.02 : -0.02
                                    rowDelegate.updateFromNorm(rowDelegate.normVal + delta)
                                }
                                onDoubleClicked: {
                                    rowDelegate.currentVal = rowDelegate.param.value
                                    rowDelegate.updateFromNorm((rowDelegate.param.value - rowDelegate.param.min) / (rowDelegate.param.max - rowDelegate.param.min))
                                }
                            }
                        }

                        // 3. Display numérico biselado hundido estilo REAPER
                        Rectangle {
                            Layout.preferredWidth: 85
                            Layout.preferredHeight: 22
                            radius: 2
                            color: "#0E0F14"
                            border.color: "#282B37"
                            border.width: 1

                            // Sombra superior interior
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
                                    var num = rowDelegate.currentVal
                                    var fmt = (Math.abs(num) >= 100) ? num.toFixed(0) : ((Math.abs(num) >= 10) ? num.toFixed(1) : num.toFixed(2))
                                    return fmt + (rowDelegate.param.unit ? (" " + rowDelegate.param.unit) : "")
                                }
                                color: "#E0E8F2"
                                font.pixelSize: 11
                                font.family: "Monospace, Consolas, Courier New"
                            }
                        }
                    }
                }
            }
        }
    }
}
