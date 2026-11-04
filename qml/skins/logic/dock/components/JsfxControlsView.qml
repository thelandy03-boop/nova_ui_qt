import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

/**
 * Vista de Vista Previa / Controles para plugins en el Dock de NOVA Studio.
 * Diseñada con la misma estética limpia, elegante y profesional que el Rack de FX:
 * - Faders verticales esbeltos con riel azul (#3B82F6) y pomo cromado (#CBD5E1)
 * - Rótulo superior del parámetro y lectura numérica en tiempo real
 * - Sincronización bidireccional automática con la pista activa y con scripts JSFX
 * - Captura táctil exclusiva, micro-ajuste táctil (▲ y ▼) y háptica
 */
Rectangle {
    id: root

    property int trackIndex: -1
    property int pluginIndex: -1
    property string scriptSource: ""
    property string pluginTitle: "Plug-in"
    property var slidersList: []

    signal editCodeRequested()
    signal parameterChanged(int paramIndex, real value)
    signal insertExampleSliderRequested()

    color: "#161822"

    onScriptSourceChanged: refreshSliders()
    onTrackIndexChanged: refreshSliders()
    onPluginIndexChanged: refreshSliders()
    Component.onCompleted: refreshSliders()

    // Conexión reactiva si cambian los parámetros de la pista
    Connections {
        target: (typeof AudioEngine !== "undefined") ? AudioEngine.tracks : null
        function onTrackPluginsChanged(tRow) {
            if (tRow === root.trackIndex) {
                root.refreshSliders()
            }
        }
    }

    function refreshSliders() {
        var list = []
        var detectedTitle = ""

        // 1. Si el plugin está activo en una pista, tomar directamente los parámetros reales de la pista
        if (typeof AudioEngine !== "undefined" && AudioEngine.tracks && root.trackIndex >= 0 && root.pluginIndex >= 0) {
            try {
                var trackPlugins = AudioEngine.tracks.trackPlugins(root.trackIndex)
                if (trackPlugins && root.pluginIndex >= 0 && root.pluginIndex < trackPlugins.length) {
                    var plg = trackPlugins[root.pluginIndex]
                    if (plg) {
                        detectedTitle = plg.name || ""
                        if (plg.parameters && plg.parameters.length > 0) {
                            for (var p = 0; p < plg.parameters.length; p++) {
                                var pr = plg.parameters[p]
                                list.push({
                                    index: (pr.index !== undefined) ? pr.index : p,
                                    sliderNum: p + 1,
                                    name: pr.name || ("P" + (p + 1)),
                                    value: (pr.value !== undefined) ? pr.value : 0.0,
                                    min: (pr.min !== undefined) ? pr.min : 0.0,
                                    max: (pr.max !== undefined) ? pr.max : 1.0,
                                    step: (pr.step !== undefined) ? pr.step : 0.01,
                                    unit: pr.unit || ""
                                })
                            }
                        }
                    }
                }
            } catch (e) {
                console.warn("[JsfxControlsView] Error al leer parámetros de la pista:", e)
            }
        }

        // 2. Si no vinieron de la pista, intentar parser C++ sobre scriptSource
        if (list.length === 0 && typeof AudioEngine !== "undefined" && AudioEngine.tracks &&
            typeof AudioEngine.tracks.parseJsfxSliders === "function" && root.scriptSource) {
            try {
                list = AudioEngine.tracks.parseJsfxSliders(root.scriptSource)
                if (typeof AudioEngine.tracks.getJsfxDescription === "function") {
                    var desc = AudioEngine.tracks.getJsfxDescription(root.scriptSource)
                    if (desc && desc.length > 0) detectedTitle = desc
                }
            } catch (e) {
                console.warn("[JsfxControlsView] Error parse C++:", e)
            }
        }

        // 3. Parser JavaScript de respaldo por expresiones regulares
        if (list.length === 0 && root.scriptSource && root.scriptSource.length > 0) {
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
        // 1. CABECERA LIMPIA Y PROFESIONAL
        // =====================================================================
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 38
            color: "#1A1D26"
            border.color: "#252936"
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                spacing: 10

                // Badge de tipo
                Rectangle {
                    Layout.preferredHeight: 18
                    Layout.preferredWidth: badgeText.implicitWidth + 8
                    radius: 4
                    color: Qt.rgba(0.23, 0.51, 0.96, 0.18)
                    border.color: "#3B82F6"
                    border.width: 1

                    Text {
                        id: badgeText
                        anchors.centerIn: parent
                        text: "DSP"
                        color: "#38BDF8"
                        font.pixelSize: 8
                        font.bold: true
                    }
                }

                // Título del plugin
                Text {
                    text: root.pluginTitle || "Plug-in"
                    color: "#F1F5F9"
                    font.pixelSize: 12
                    font.bold: true
                    font.family: "Segoe UI, Inter, Sans-serif"
                }

                Text {
                    text: "(" + root.slidersList.length + " parámetros)"
                    color: "#64748B"
                    font.pixelSize: 10
                    visible: root.slidersList.length > 0
                }

                Item { Layout.fillWidth: true }
            }
        }

        // =====================================================================
        // 2. CONTENIDO PRINCIPAL: FADERS VERTICALES ESTILIZADOS COMO FX RACK
        // =====================================================================
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            // Estado vacío sin sliders
            Item {
                anchors.fill: parent
                visible: root.slidersList.length === 0

                Column {
                    anchors.centerIn: parent
                    spacing: 8
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "🎛️"
                        font.pixelSize: 26
                    }
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "Este plug-in no expone sliders ajustables"
                        color: "#64748B"
                        font.pixelSize: 12
                    }
                }
            }

            // Tiras verticales alineadas en fila horizontal con desplazamiento suave
            Flickable {
                id: fadersScroll
                anchors.fill: parent
                anchors.margins: 12
                contentWidth: fadersRow.implicitWidth + 24
                contentHeight: parent.height - 24
                clip: true
                boundsBehavior: (Qt.platform.os === "android") ? Flickable.DragOverBounds : Flickable.StopAtBounds
                flickableDirection: Flickable.HorizontalFlick
                visible: root.slidersList.length > 0

                Row {
                    id: fadersRow
                    spacing: 10
                    height: parent.height

                    Repeater {
                        model: root.slidersList

                        delegate: Rectangle {
                            id: stripCard
                            width: (Qt.platform.os === "android") ? 64 : 58
                            height: fadersRow.height
                            radius: 8
                            color: hoverArea.containsMouse ? "#1E222D" : "#181B24"
                            border.color: hoverArea.containsMouse ? "#3B82F6" : "#262A37"
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
                                anchors.margins: 6
                                spacing: 4

                                // 1. Rótulo superior del parámetro (igual a LogicFxSlider)
                                Text {
                                    Layout.fillWidth: true
                                    text: stripCard.param.name
                                    color: "#8E94A5"
                                    font.pixelSize: 9
                                    font.bold: true
                                    font.letterSpacing: 0.3
                                    horizontalAlignment: Text.AlignHCenter
                                    elide: Text.ElideRight
                                }

                                // 2. Valor numérico (igual a LogicFxSlider)
                                Text {
                                    Layout.fillWidth: true
                                    text: {
                                        var num = stripCard.currentVal
                                        var fmt = (Math.abs(num) >= 100) ? num.toFixed(0) : ((Math.abs(num) >= 10) ? num.toFixed(1) : num.toFixed(2))
                                        return fmt + (stripCard.param.unit ? (" " + stripCard.param.unit) : "")
                                    }
                                    color: "#E2E8F0"
                                    font.pixelSize: 10
                                    font.bold: true
                                    horizontalAlignment: Text.AlignHCenter
                                    elide: Text.ElideRight
                                }

                                // 3. Riel vertical interactivo (diseño 100% fiel a LogicFxSlider)
                                Item {
                                    id: verticalTrackSlot
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true

                                    // Riel vertical oscuro
                                    Rectangle {
                                        id: trackRect
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        anchors.top: parent.top
                                        anchors.bottom: parent.bottom
                                        anchors.topMargin: 4
                                        anchors.bottomMargin: 4
                                        width: 8
                                        radius: 4
                                        color: "#181A22"
                                        border.color: "#2C2E3C"
                                        border.width: 1

                                        // Relleno activo azul (#3B82F6) que sube desde el fondo
                                        Rectangle {
                                            anchors.bottom: parent.bottom
                                            anchors.horizontalCenter: parent.horizontalCenter
                                            width: parent.width
                                            height: Math.max(4, parent.height * stripCard.normVal)
                                            radius: 4
                                            color: "#3B82F6"
                                        }

                                        // Pomo deslizador metálico (Thumb) idéntico a LogicFxSlider
                                        Rectangle {
                                            id: thumb
                                            anchors.horizontalCenter: parent.horizontalCenter
                                            y: Math.max(0, Math.min(trackRect.height - height, (trackRect.height - height) * (1.0 - stripCard.normVal)))
                                            width: 20
                                            height: 13
                                            radius: 3
                                            color: dragArea.containsPress ? "#FFFFFF" : "#CBD5E1"
                                            border.color: "#0F172A"
                                            border.width: 1

                                            // Muesca central oscura
                                            Rectangle {
                                                anchors.centerIn: parent
                                                width: 8
                                                height: 2
                                                color: "#334155"
                                            }
                                        }
                                    }

                                    // Área táctil de arrastre sobre todo el espacio vertical
                                    MouseArea {
                                        id: dragArea
                                        anchors.fill: parent
                                        anchors.margins: (Qt.platform.os === "android") ? -14 : -6
                                        preventStealing: true
                                        hoverEnabled: true
                                        cursorShape: Qt.SizeVerCursor

                                        function applyY(mouseY) {
                                            var usable = Math.max(1, trackRect.height - thumb.height)
                                            var n = 1.0 - ((mouseY - trackRect.y - thumb.height / 2) / usable)
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

                                // 4. Botones discretos de micro-ajuste táctil (▼ y ▲)
                                RowLayout {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: (Qt.platform.os === "android") ? 22 : 18
                                    spacing: 3

                                    Rectangle {
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        radius: 3
                                        color: downMouse.containsMouse ? "#2E3344" : "#20232E"
                                        border.color: "#2C3140"
                                        border.width: 1

                                        Text {
                                            anchors.centerIn: parent
                                            text: "▼"
                                            color: "#94A3B8"
                                            font.pixelSize: 7
                                        }

                                        MouseArea {
                                            id: downMouse
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: stripCard.stepChange(-1)
                                        }
                                    }

                                    Rectangle {
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        radius: 3
                                        color: upMouse.containsMouse ? "#2E3344" : "#20232E"
                                        border.color: "#2C3140"
                                        border.width: 1

                                        Text {
                                            anchors.centerIn: parent
                                            text: "▲"
                                            color: "#94A3B8"
                                            font.pixelSize: 7
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

                            MouseArea {
                                id: hoverArea
                                anchors.fill: parent
                                hoverEnabled: true
                                acceptedButtons: Qt.NoButton
                            }
                        }
                    }
                }
            }
        }
    }
}
