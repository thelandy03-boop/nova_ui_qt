import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

/**
 * Vista de interfaz gráfica (UI) para scripts JSFX.
 * Diseño sobrio, profesional y alineado estilo DAW real (Reaper / Ardour).
 * Renderiza los parámetros sliderN en filas ordenadas que ocupan el ancho
 * completo de la ventana con controles de alta precisión.
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

    color: "#14151B"

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
                console.warn("[JsfxControlsView] Error en parse C++:", e)
            }
        }

        // 2. Parser JavaScript de respaldo
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
        // ENCABEZADO MINIMALISTA Y SOBRIO
        // =====================================================================
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 34
            color: "#181A22"
            border.color: "#222530"
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 20
                anchors.rightMargin: 20
                spacing: 10

                Text {
                    text: root.pluginTitle || "Efecto JSFX"
                    color: "#E2E8F0"
                    font.pixelSize: 12
                    font.bold: true
                    font.family: "Segoe UI, Inter, Sans-serif"
                }

                Item { Layout.fillWidth: true }
            }
        }

        // =====================================================================
        // LISTA DE PARÁMETROS / SLIDERS ALINEADOS
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
                    text: "El script actual no define ningún slider (slider1, slider2...)"
                    color: "#64748B"
                    font.pixelSize: 12
                    font.family: "Segoe UI, Inter, Sans-serif"
                }
            }

            // Lista estructurada de sliders
            ListView {
                id: slidersView
                anchors.fill: parent
                anchors.margins: 16
                visible: root.slidersList.length > 0
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                model: root.slidersList
                spacing: 6

                delegate: Rectangle {
                    id: rowDelegate
                    width: slidersView.width
                    height: 42
                    radius: 4
                    color: rowMouse.containsMouse ? "#1A1D27" : "#161820"
                    border.color: rowMouse.containsMouse ? "#2C3142" : "#1F222C"
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
                        anchors.leftMargin: 16
                        anchors.rightMargin: 16
                        spacing: 16

                        // 1. Nombre del parámetro a la izquierda
                        Text {
                            Layout.preferredWidth: (parent.width > 500) ? 180 : 130
                            text: rowDelegate.param.name
                            color: "#CBD5E1"
                            font.pixelSize: 12
                            font.bold: false
                            font.family: "Segoe UI, Inter, Sans-serif"
                            elide: Text.ElideRight
                        }

                        // 2. Fader / Slider horizontal central expansivo
                        Item {
                            id: faderTrack
                            Layout.fillWidth: true
                            Layout.preferredHeight: 28

                            // Canal del fader
                            Rectangle {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                height: 6
                                radius: 3
                                color: "#0F1015"
                                border.color: "#272A36"
                                border.width: 1

                                // Relleno activo
                                Rectangle {
                                    anchors.left: parent.left
                                    anchors.top: parent.top
                                    anchors.bottom: parent.bottom
                                    width: Math.max(6, parent.width * rowDelegate.normVal)
                                    radius: 3
                                    color: "#2563EB"
                                }
                            }

                            // Manija (thumb) sobria y precisa
                            Rectangle {
                                x: Math.max(0, Math.min(parent.width - width, (parent.width - width) * rowDelegate.normVal))
                                anchors.verticalCenter: parent.verticalCenter
                                width: 12; height: 18
                                radius: 2
                                color: rowMouse.drag.active ? "#FFFFFF" : "#E2E8F0"
                                border.color: "#3B82F6"
                                border.width: 1

                                Rectangle {
                                    anchors.centerIn: parent
                                    width: 2; height: 8
                                    color: "#64748B"
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

                        // 3. Valor numérico formateado a la derecha
                        Rectangle {
                            Layout.preferredWidth: 84
                            Layout.preferredHeight: 22
                            radius: 3
                            color: "#0F1015"
                            border.color: "#272B38"
                            border.width: 1

                            Text {
                                anchors.centerIn: parent
                                text: {
                                    var num = rowDelegate.currentVal
                                    var fmt = (Math.abs(num) >= 100) ? num.toFixed(0) : ((Math.abs(num) >= 10) ? num.toFixed(1) : num.toFixed(2))
                                    return fmt + (rowDelegate.param.unit ? (" " + rowDelegate.param.unit) : "")
                                }
                                color: "#38BDF8"
                                font.pixelSize: 11
                                font.family: "Monospace, Consolas"
                            }
                        }
                    }
                }
            }
        }
    }
}
