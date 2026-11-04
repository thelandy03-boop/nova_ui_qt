import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../rack"

/**
 * Vista previa de controles para plug-ins en el Dock de NOVA Studio.
 * Diseñada para tener EXACTAMENTE la misma UI, proporciones, estilo y componentes
 * que el Rack de FX (LogicFxCard + LogicFxSlider):
 * - Altura canónica de 165 px, radio de 10 px y paleta oscura profesional (#161820 / #2A2E3D)
 * - Cabecera con badge de categoría (EQ, DINÁMICA, ESPACIO, DELAY, etc.) y conmutador ON/BYPASS
 * - Faders verticales compactos de 48x110 px con riel azul y pomo cromado (#CBD5E1)
 * - Sincronización en tiempo real bidireccional con la pista activa y con scripts JSFX
 */
Rectangle {
    id: root

    property int trackIndex: -1
    property int pluginIndex: -1
    property string scriptSource: ""
    property string pluginTitle: "Plug-in"
    property string pluginCategory: "DSP"
    property bool isBypassed: false
    property var slidersList: []

    signal editCodeRequested()
    signal parameterChanged(int paramIndex, real value)
    signal insertExampleSliderRequested()

    color: "#13151D"

    onScriptSourceChanged: refreshSliders()
    onTrackIndexChanged: refreshSliders()
    onPluginIndexChanged: refreshSliders()
    Component.onCompleted: refreshSliders()

    // Conexión reactiva si cambian los plug-ins de la pista
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
        var detectedCategory = ""

        // 1. Si el plug-in está asignado en una pista, tomar directamente sus datos reales
        if (typeof AudioEngine !== "undefined" && AudioEngine.tracks && root.trackIndex >= 0 && root.pluginIndex >= 0) {
            try {
                var trackPlugins = AudioEngine.tracks.trackPlugins(root.trackIndex)
                if (trackPlugins && root.pluginIndex >= 0 && root.pluginIndex < trackPlugins.length) {
                    var plg = trackPlugins[root.pluginIndex]
                    if (plg) {
                        detectedTitle = plg.name || ""
                        if (plg.category) detectedCategory = plg.category
                        root.isBypassed = !!plg.bypassed
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

        // Deducir categoría temática para el color de acento y badge
        if (detectedCategory && detectedCategory.length > 0) {
            root.pluginCategory = detectedCategory
        } else {
            var nLower = (root.pluginTitle || "").toLowerCase()
            if (nLower.indexOf("eq") !== -1 || nLower.indexOf("equal") !== -1) {
                root.pluginCategory = "EQ"
            } else if (nLower.indexOf("comp") !== -1 || nLower.indexOf("gate") !== -1 || nLower.indexOf("limit") !== -1) {
                root.pluginCategory = "DINÁMICA"
            } else if (nLower.indexOf("reverb") !== -1 || nLower.indexOf("room") !== -1 || nLower.indexOf("space") !== -1) {
                root.pluginCategory = "ESPACIO"
            } else if (nLower.indexOf("delay") !== -1 || nLower.indexOf("echo") !== -1) {
                root.pluginCategory = "DELAY"
            } else {
                root.pluginCategory = "EFECTO"
            }
        }

        root.slidersList = list
    }

    function resetAllSliders() {
        for (var i = 0; i < root.slidersList.length; i++) {
            var sl = root.slidersList[i]
            var defVal = (sl.min + sl.max) / 2.0
            sl.value = defVal
            root.parameterChanged(sl.index, defVal)
            if (typeof AudioEngine !== "undefined" && AudioEngine.tracks &&
                root.trackIndex >= 0 && root.pluginIndex >= 0) {
                AudioEngine.tracks.setPluginParameter(root.trackIndex, root.pluginIndex, sl.index, defVal)
            }
        }
        refreshSliders()
    }

    // Contenedor principal con centrado y scroll horizontal fluido
    Flickable {
        id: cardFlickable
        anchors.fill: parent
        contentWidth: Math.max(width, cardHolder.implicitWidth + 32)
        contentHeight: Math.max(height, cardHolder.implicitHeight + 32)
        clip: true
        boundsBehavior: (Qt.platform.os === "android") ? Flickable.DragOverBounds : Flickable.StopAtBounds

        Item {
            id: cardHolder
            width: Math.max(cardFlickable.width, fxCard.implicitWidth + 32)
            height: Math.max(cardFlickable.height, fxCard.implicitHeight + 32)
            implicitWidth: fxCard.implicitWidth + 32
            implicitHeight: fxCard.implicitHeight + 32

            // Tarjeta de plug-in 100% idéntica al Rack de FX
            LogicFxCard {
                id: fxCard
                anchors.centerIn: parent
                title: root.pluginTitle || "Plug-in"
                category: root.pluginCategory || "DSP"
                accentColor: {
                    if (category === "EQ") return "#3B82F6"
                    if (category === "DINÁMICA") return "#F59E0B"
                    if (category === "ESPACIO") return "#A855F7"
                    if (category === "DELAY") return "#10B981"
                    return "#38BDF8"
                }
                enabledFx: !root.isBypassed
                isJsfx: true
                showHeaderActions: false

                onEditCodeRequested: {
                    root.editCodeRequested()
                }

                onToggled: (isEnabled) => {
                    root.isBypassed = !isEnabled
                    if (typeof AudioEngine !== "undefined" && AudioEngine.tracks &&
                        root.trackIndex >= 0 && root.pluginIndex >= 0) {
                        AudioEngine.tracks.setPluginBypassed(root.trackIndex, root.pluginIndex, !isEnabled)
                    }
                }

                onRemoveRequested: {
                    root.resetAllSliders()
                }

                // Sliders dinámicos idénticos a los del Rack de FX
                Row {
                    id: slidersRow
                    anchors.centerIn: parent
                    spacing: 10
                    visible: root.slidersList.length > 0

                    Repeater {
                        model: root.slidersList

                        delegate: LogicFxSlider {
                            property var p: modelData
                            label: p.name || ("P" + (index + 1))
                            value: (p.max > p.min) ? Math.max(0.0, Math.min(1.0, (p.value - p.min) / (p.max - p.min))) : 0.5
                            displayValue: {
                                var num = p.value
                                var fmt = (Math.abs(num) >= 100) ? num.toFixed(0) : ((Math.abs(num) >= 10) ? num.toFixed(1) : num.toFixed(2))
                                return fmt + (p.unit ? " " + p.unit : "")
                            }
                            activeColor: fxCard.accentColor

                            onValueChangedByUser: (newVal) => {
                                var realVal = p.min + newVal * (p.max - p.min)
                                if (p.step > 0) {
                                    realVal = Math.round(realVal / p.step) * p.step
                                }
                                p.value = realVal
                                root.parameterChanged(p.index !== undefined ? p.index : index, realVal)
                                if (typeof AudioEngine !== "undefined" && AudioEngine.tracks &&
                                    root.trackIndex >= 0 && root.pluginIndex >= 0) {
                                    AudioEngine.tracks.setPluginParameter(root.trackIndex, root.pluginIndex, p.index !== undefined ? p.index : index, realVal)
                                }
                            }
                        }
                    }
                }

                // Mensaje en caso de no exponer sliders ajustables
                Text {
                    anchors.centerIn: parent
                    visible: root.slidersList.length === 0
                    text: "Plug-in activo y procesando señal"
                    color: "#64748B"
                    font.pixelSize: 10
                    font.italic: true
                }
            }
        }
    }
}
