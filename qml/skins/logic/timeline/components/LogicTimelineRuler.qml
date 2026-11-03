import QtQuick

Rectangle {
    id: root
    property real barWidth: 80.0
    property real contentX: 0
    property int totalBars: 120

    implicitWidth: totalBars * barWidth
    implicitHeight: 30
    color: "#181A20"
    clip: true

    // Niveles de detalle (LOD) adaptativos según zoom (barWidth):
    // 1. Frecuencia de numeración de compases para evitar solapamientos
    property int labelEvery: barWidth >= 55 ? 1 : (barWidth >= 28 ? 2 : (barWidth >= 15 ? 4 : 8))

    // 2. Visualización de tiempos musicales (1.2, 1.3, 1.4):
    // - Con >= 75px por compás (~19px por tiempo), caben holgadamente todos los tiempos
    // - Entre 55px y 75px, mostramos solo el tiempo 3 (1.3) para mantener claridad sin saturar
    property bool showAllBeats: barWidth >= 75
    property bool showMidBeat: barWidth >= 55 && !showAllBeats

    // 3. Densidad de marcas de subdivisión (ticks):
    // - >= 80px: 16 marcas por compás (semicorcheas / 16th notes completas)
    // - >= 40px: 4 marcas por compás (tiempos / quarter notes)
    // - < 40px: 1 marca por compás (inicio de compás)
    property int ticksPerBar: barWidth >= 80 ? 16 : (barWidth >= 40 ? 4 : 1)

    // Borde superior sutil de iluminación y profundidad
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 1
        color: "#282C37"
    }

    // Borde inferior oscuro divisor de pistas
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: "#111215"
        z: 3
    }

    // ── BARRA VISUAL DE BUCLE (LOOP BAR) ──
    Rectangle {
        id: loopBar
        y: 2
        height: 6
        color: AudioEngine.loopEnabled ? "#F5C518" : "#3F4554"
        opacity: AudioEngine.loopEnabled ? 0.95 : 0.65
        radius: 2
        z: 2

        property real startBeat: AudioEngine.loopStartBeat
        property real endBeat: AudioEngine.loopEndBeat

        x: AudioEngine.beatToPixel(startBeat, root.barWidth) - root.contentX
        width: Math.max(16, AudioEngine.beatToPixel(endBeat - startBeat, root.barWidth))

        // Tirador izquierdo del bucle
        Rectangle {
            width: 5; height: parent.height
            anchors.left: parent.left
            color: "#FFFFFF"
            radius: 1

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.SizeHorCursor
                onPositionChanged: (mouse) => {
                    if (pressed) {
                        var pt = mapToItem(root, mouse.x, mouse.y)
                        var rawX = pt.x + root.contentX
                        var newStartBeat = Math.floor(AudioEngine.pixelToBeat(rawX, root.barWidth))
                        if (newStartBeat < AudioEngine.loopEndBeat) {
                            AudioEngine.setLoopRange(newStartBeat, AudioEngine.loopEndBeat)
                        }
                    }
                }
            }
        }

        // Tirador derecho del bucle
        Rectangle {
            width: 5; height: parent.height
            anchors.right: parent.right
            color: "#FFFFFF"
            radius: 1

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.SizeHorCursor
                onPositionChanged: (mouse) => {
                    if (pressed) {
                        var pt = mapToItem(root, mouse.x, mouse.y)
                        var rawX = pt.x + root.contentX
                        var newEndBeat = Math.max(AudioEngine.loopStartBeat + 1, Math.ceil(AudioEngine.pixelToBeat(rawX, root.barWidth)))
                        AudioEngine.setLoopRange(AudioEngine.loopStartBeat, newEndBeat)
                    }
                }
            }
        }

        MouseArea {
            anchors.fill: parent
            anchors.leftMargin: 6
            anchors.rightMargin: 6
            cursorShape: Qt.PointingHandCursor
            onClicked: AudioEngine.toggleLoop()
        }
    }

    // ── MARCAS DE COMPASES, TIEMPOS Y SUB-TICKS (16THS) ──
    Row {
        x: -root.contentX
        anchors.top: parent.top
        anchors.bottom: parent.bottom

        Repeater {
            model: root.totalBars

            Item {
                id: barItem
                property int barIndex: index
                width: root.barWidth
                height: parent.height
                clip: true

                // 1. Ticks jerárquicos (Compás, Beats, 8ths y 16ths)
                Repeater {
                    model: root.ticksPerBar

                    Rectangle {
                        property int tickIndex: index
                        x: root.ticksPerBar === 16 ? tickIndex * (root.barWidth / 16)
                         : (root.ticksPerBar === 4 ? tickIndex * (root.barWidth / 4) : 0)
                        anchors.bottom: parent.bottom
                        width: 1

                        // Alturas jerárquicas auténticas de DAW:
                        // - Compás principal (tick 0): 11px
                        // - Beat principal (ticks 4, 8, 12 en 16ths; o 1, 2, 3 en 4s): 6.5px
                        // - Corchea / 8th note (ticks 2, 6, 10, 14 en 16ths): 4.5px
                        // - Semicorchea / 16th note (ticks impares 1, 3, 5, 7, 9, 11, 13, 15): 3px
                        height: {
                            if (tickIndex === 0) return 11;
                            if (root.ticksPerBar === 4) return 6.5;
                            if (tickIndex % 4 === 0) return 6.5;
                            if (tickIndex % 2 === 0) return 4.5;
                            return 3.0;
                        }

                        // Colores jerárquicos con contraste profesional:
                        color: {
                            if (tickIndex === 0) return "#7E889D";
                            if (root.ticksPerBar === 4) return "#4F586C";
                            if (tickIndex % 4 === 0) return "#4F586C";
                            if (tickIndex % 2 === 0) return "#3B4252";
                            return "#2C313E";
                        }
                    }
                }

                // 2. Número de compás principal ("1", "2", "3"...)
                Text {
                    x: 4
                    anchors.top: parent.top
                    anchors.topMargin: 10
                    text: (barIndex + 1).toString()
                    visible: (barIndex % root.labelEvery === 0)
                    color: "#F1F5F9"
                    font.pixelSize: root.barWidth < 40 ? 9 : 10
                    font.bold: true
                }

                // 3. Sub-divisiones de tiempo estilo DAW ("1.2", "1.3", "1.4")
                // Se muestran cuando hay zoom suficiente para distinguirlos
                Repeater {
                    model: root.showAllBeats ? 3 : (root.showMidBeat ? 1 : 0)

                    Text {
                        property int targetBeat: root.showAllBeats ? (index + 2) : 3
                        x: (targetBeat - 1) * (root.barWidth / 4) + 3
                        anchors.top: parent.top
                        anchors.topMargin: 11
                        text: (barIndex + 1) + "." + targetBeat
                        visible: (barIndex % root.labelEvery === 0)
                        color: "#8F9BAE"
                        font.pixelSize: 9
                        font.bold: false
                    }
                }
            }
        }
    }

    // ── INTERACCIÓN CLIC Y ARRASTRE DE AGUJA (SCRUBBING) ──
    MouseArea {
        id: rulerScrubArea
        anchors.fill: parent
        anchors.topMargin: 9
        cursorShape: Qt.PointingHandCursor

        function updatePlayheadPosition(mouse) {
            var rawX = Math.max(0, mouse.x + root.contentX)
            var targetBeat = AudioEngine.pixelToBeat(rawX, root.barWidth)
            AudioEngine.locateBeat(targetBeat)
        }

        onPressed: (mouse) => updatePlayheadPosition(mouse)
        onPositionChanged: (mouse) => {
            if (pressed) updatePlayheadPosition(mouse)
        }
    }
}