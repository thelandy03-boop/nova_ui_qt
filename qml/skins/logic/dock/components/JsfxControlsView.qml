import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

/**
 * Vista de interfaz gráfica (UI) para scripts JSFX.
 * Renderiza dinámicamente los controles (sliders) definidos en el script JSFX
 * con estética de consola de estudio profesional, permitiendo manipular
 * los parámetros en tiempo real en el motor de audio.
 */
Rectangle {
    id: root

    property int trackIndex: -1
    property int pluginIndex: -1
    property string scriptSource: ""
    property string pluginTitle: "JSFX Plugin"
    property bool isBypassed: false
    property var slidersList: []

    signal editCodeRequested()
    signal parameterChanged(int paramIndex, real value)
    signal insertExampleSliderRequested()

    color: "#161820"

    // Parseo reactivo de sliders cuando cambia el código fuente
    onScriptSourceChanged: refreshSliders()
    Component.onCompleted: refreshSliders()

    function refreshSliders() {
        var list = []
        var detectedTitle = ""

        // 1. Intentar parsear a través del motor C++ si está disponible
        if (typeof AudioEngine !== "undefined" && AudioEngine.tracks &&
            typeof AudioEngine.tracks.parseJsfxSliders === "function") {
            try {
                list = AudioEngine.tracks.parseJsfxSliders(root.scriptSource)
                if (typeof AudioEngine.tracks.getJsfxDescription === "function") {
                    detectedTitle = AudioEngine.tracks.getJsfxDescription(root.scriptSource)
                }
            } catch (e) {
                console.warn("[JsfxControlsView] Error al parsear sliders en C++:", e)
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
                    // Fin del encabezado de sliders
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
                        var rawName = m[6] ? m[6].trim() : ("Control " + sNum)
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

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // =====================================================================
        // 1. ENCABEZADO DEL RACK DE PROCESAMIENTO
        // =====================================================================
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 44
            color: "#1C1E26"
            border.color: "#282B37"
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                spacing: 12

                // Icono decorativo de hardware
                Rectangle {
                    Layout.preferredWidth: 26
                    Layout.preferredHeight: 26
                    radius: 5
                    color: "#242836"
                    border.color: "#3B82F6"
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: "🎛️"
                        font.pixelSize: 13
                    }
                }

                // Título y categoría del plugin
                ColumnLayout {
                    spacing: 1
                    Text {
                        text: root.pluginTitle || "Efecto JSFX"
                        color: "#F1F5F9"
                        font.pixelSize: 13
                        font.bold: true
                        font.family: "Segoe UI, Inter, Sans-serif"
                    }
                    Row {
                        spacing: 6
                        Rectangle {
                            width: 6; height: 6
                            radius: 3
                            color: root.isBypassed ? "#64748B" : "#10B981"
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Text {
                            text: root.isBypassed ? "BYPASS" : (root.slidersList.length + (root.slidersList.length === 1 ? " parámetro" : " parámetros"))
                            color: "#94A3B8"
                            font.pixelSize: 10
                            font.family: "Segoe UI, Inter, Sans-serif"
                        }
                    }
                }

                Item { Layout.fillWidth: true }

                // Botón On / Bypass
                Rectangle {
                    Layout.preferredHeight: 26
                    Layout.preferredWidth: bypassRow.implicitWidth + 18
                    radius: 4
                    color: root.isBypassed ? "#2D3139" : "#1E3A8A"
                    border.color: root.isBypassed ? "#475569" : "#3B82F6"
                    border.width: 1

                    Row {
                        id: bypassRow
                        anchors.centerIn: parent
                        spacing: 6
                        Rectangle {
                            width: 8; height: 8
                            radius: 4
                            color: root.isBypassed ? "#94A3B8" : "#38BDF8"
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Text {
                            text: root.isBypassed ? "Bypass" : "Activo"
                            color: root.isBypassed ? "#94A3B8" : "#FFFFFF"
                            font.pixelSize: 11
                            font.bold: true
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            root.isBypassed = !root.isBypassed
                            if (typeof AudioEngine !== "undefined" && AudioEngine.tracks &&
                                root.trackIndex >= 0 && root.pluginIndex >= 0) {
                                AudioEngine.tracks.setPluginBypassed(root.trackIndex, root.pluginIndex, root.isBypassed)
                            }
                        }
                    }
                }

                // Botón volver al código
                Rectangle {
                    Layout.preferredHeight: 26
                    Layout.preferredWidth: editCodeRow.implicitWidth + 18
                    radius: 4
                    color: editCodeMouse.containsMouse ? "#323846" : "#222530"
                    border.color: "#38BDF8"
                    border.width: 1

                    Row {
                        id: editCodeRow
                        anchors.centerIn: parent
                        spacing: 6
                        Text {
                            text: "</>"
                            color: "#38BDF8"
                            font.pixelSize: 11
                            font.bold: true
                            font.family: "Monospace"
                        }
                        Text {
                            text: "Editar Código"
                            color: "#E8EAED"
                            font.pixelSize: 11
                            font.bold: true
                        }
                    }

                    MouseArea {
                        id: editCodeMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.editCodeRequested()
                    }
                }
            }
        }

        // =====================================================================
        // 2. CUERPO DE CONTROLES: SLIDERS O ESTADO EDUCATIVO VACÍO
        // =====================================================================
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            // Estado sin sliders definidos
            Item {
                anchors.fill: parent
                visible: root.slidersList.length === 0

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 12
                    width: Math.min(parent.width - 40, 520)

                    Rectangle {
                        Layout.alignment: Qt.AlignHCenter
                        width: 52; height: 52
                        radius: 26
                        color: "#1E222D"
                        border.color: "#33394A"
                        border.width: 1

                        Text {
                            anchors.centerIn: parent
                            text: "🎚️"
                            font.pixelSize: 26
                        }
                    }

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: "Sin controles visuales definidos"
                        color: "#F1F5F9"
                        font.pixelSize: 15
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                    }

                    Text {
                        Layout.fillWidth: true
                        text: "Este script JSFX procesa audio pero aún no declara sliders gráficos para interactuar con sus variables."
                        color: "#94A3B8"
                        font.pixelSize: 12
                        wrapMode: Text.WordWrap
                        horizontalAlignment: Text.AlignHCenter
                    }

                    // Caja de ejemplo de código JSFX
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 70
                        radius: 6
                        color: "#111318"
                        border.color: "#282B37"
                        border.width: 1

                        Column {
                            anchors.centerIn: parent
                            spacing: 4
                            Text {
                                text: "// Sintaxis de sliders en el encabezado JSFX:"
                                color: "#64748B"
                                font.pixelSize: 11
                                font.family: "Monospace, Consolas"
                            }
                            Text {
                                text: "slider1:250<10,2000,10>Delay Time (ms)\nslider2:0.4<0,0.95,0.01>Feedback"
                                color: "#38BDF8"
                                font.pixelSize: 11
                                font.family: "Monospace, Consolas"
                            }
                        }
                    }

                    Rectangle {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredHeight: 32
                        Layout.preferredWidth: addExampleRow.implicitWidth + 24
                        radius: 5
                        color: addExMouse.containsMouse ? "#2563EB" : "#1D4ED8"

                        Row {
                            id: addExampleRow
                            anchors.centerIn: parent
                            spacing: 8
                            Text { text: "➕"; color: "#FFFFFF"; font.pixelSize: 11 }
                            Text {
                                text: "Añadir sliders de ejemplo al código"
                                color: "#FFFFFF"
                                font.pixelSize: 12
                                font.bold: true
                            }
                        }

                        MouseArea {
                            id: addExMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.insertExampleSliderRequested()
                        }
                    }
                }
            }

            // Lista con scroll de sliders generados
            ScrollView {
                anchors.fill: parent
                visible: root.slidersList.length > 0
                clip: true
                ScrollBar.vertical.policy: ScrollBar.AsNeeded
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                Flow {
                    id: slidersFlow
                    width: parent.width - 32
                    x: 16
                    y: 16
                    spacing: 16

                    Repeater {
                        model: root.slidersList

                        delegate: Rectangle {
                            id: sliderCard
                            width: (slidersFlow.width > 700) ? ((slidersFlow.width - 32) / 3) : ((slidersFlow.width > 420) ? ((slidersFlow.width - 16) / 2) : slidersFlow.width)
                            height: 104
                            radius: 8
                            color: "#1A1C24"
                            border.color: sMouse.containsMouse ? "#3B82F6" : "#2A2D3A"
                            border.width: 1

                            property var param: modelData
                            property real currentVal: param.value
                            property real normVal: (param.max > param.min) ? Math.max(0.0, Math.min(1.0, (currentVal - param.min) / (param.max - param.min))) : 0.0

                            function updateFromNormalized(nVal) {
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

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 12
                                spacing: 8

                                // Etiqueta superior del parámetro y valor actual
                                RowLayout {
                                    Layout.fillWidth: true

                                    Text {
                                        Layout.fillWidth: true
                                        text: sliderCard.param.name
                                        color: "#E2E8F0"
                                        font.pixelSize: 12
                                        font.bold: true
                                        elide: Text.ElideRight
                                    }

                                    // Indicador numérico formateado
                                    Rectangle {
                                        Layout.preferredHeight: 20
                                        Layout.preferredWidth: valText.implicitWidth + 12
                                        radius: 4
                                        color: "#111318"
                                        border.color: "#333846"
                                        border.width: 1

                                        Text {
                                            id: valText
                                            anchors.centerIn: parent
                                            text: {
                                                var num = sliderCard.currentVal
                                                var formatted = (Math.abs(num) >= 100) ? num.toFixed(0) : ((Math.abs(num) >= 10) ? num.toFixed(1) : num.toFixed(2))
                                                return formatted + (sliderCard.param.unit ? (" " + sliderCard.param.unit) : "")
                                            }
                                            color: "#38BDF8"
                                            font.pixelSize: 11
                                            font.bold: true
                                            font.family: "Monospace, Consolas"
                                        }
                                    }
                                }

                                // Barra deslizante interactiva
                                Item {
                                    id: trackBar
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 24

                                    // Pista base
                                    Rectangle {
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        anchors.verticalCenter: parent.verticalCenter
                                        height: 8
                                        radius: 4
                                        color: "#0F1015"
                                        border.color: "#282B37"
                                        border.width: 1

                                        // Relleno activo con degradado
                                        Rectangle {
                                            anchors.left: parent.left
                                            anchors.top: parent.top
                                            anchors.bottom: parent.bottom
                                            width: Math.max(8, parent.width * sliderCard.normVal)
                                            radius: 4
                                            gradient: Gradient {
                                                orientation: Gradient.Horizontal
                                                GradientStop { position: 0.0; color: "#2563EB" }
                                                GradientStop { position: 1.0; color: "#38BDF8" }
                                            }
                                        }
                                    }

                                    // Manija deslizante táctil (Thumb)
                                    Rectangle {
                                        x: Math.max(0, Math.min(parent.width - width, (parent.width - width) * sliderCard.normVal))
                                        anchors.verticalCenter: parent.verticalCenter
                                        width: 16; height: 20
                                        radius: 3
                                        color: sMouse.drag.active ? "#FFFFFF" : "#E2E8F0"
                                        border.color: "#3B82F6"
                                        border.width: 1.5

                                        // Líneas táctiles de relieve
                                        Row {
                                            anchors.centerIn: parent
                                            spacing: 2
                                            Rectangle { width: 1; height: 8; color: "#64748B" }
                                            Rectangle { width: 1; height: 8; color: "#64748B" }
                                        }
                                    }

                                    MouseArea {
                                        id: sMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor

                                        function updatePos(mouse) {
                                            var norm = mouse.x / width
                                            sliderCard.updateFromNormalized(norm)
                                        }

                                        onPressed: (mouse) => updatePos(mouse)
                                        onPositionChanged: (mouse) => {
                                            if (pressed) updatePos(mouse)
                                        }
                                        onWheel: (wheel) => {
                                            var delta = (wheel.angleDelta.y > 0) ? 0.05 : -0.05
                                            sliderCard.updateFromNormalized(sliderCard.normVal + delta)
                                        }
                                        onDoubleClicked: {
                                            // Doble clic restaura el valor por defecto
                                            sliderCard.currentVal = sliderCard.param.value
                                            sliderCard.updateFromNormalized((sliderCard.param.value - sliderCard.param.min) / (sliderCard.param.max - sliderCard.param.min))
                                        }
                                    }
                                }

                                // Indicadores de límites mínimo y máximo
                                RowLayout {
                                    Layout.fillWidth: true
                                    Text {
                                        text: sliderCard.param.min + (sliderCard.param.unit ? (" " + sliderCard.param.unit) : "")
                                        color: "#64748B"
                                        font.pixelSize: 10
                                    }
                                    Item { Layout.fillWidth: true }
                                    Text {
                                        text: sliderCard.param.max + (sliderCard.param.unit ? (" " + sliderCard.param.unit) : "")
                                        color: "#64748B"
                                        font.pixelSize: 10
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
