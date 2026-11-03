import QtQuick

Item {
    id: root
    property real barWidth: 80.0
    property int totalBars: 60
    property int trackHeight: 74

    // Propiedades de Overlay Flotante del Clip Seleccionado (Estilo Honeydew)
    property var selectedClip: null
    property real selectedTapX: 120

    // Ancho total y alto calculado dinámicamente según el número de pistas
    width: barWidth * totalBars
    height: Math.max(1080, (AudioEngine.tracks ? AudioEngine.tracks.rowCount() * trackHeight : 0) + 200)

    // Fondo base
    Rectangle {
        anchors.fill: parent
        color: "#1C1D22"
    }

    // 1. CARRILES HORIZONTALES DINÁMICOS (Sincronizados con Ardour)
    Column {
        id: lanesColumn
        width: parent.width
        z: 10

        Repeater {
            model: AudioEngine.tracks
            delegate: LogicTrackLane {
                width: root.width
                barWidth: root.barWidth
                timelineGrid: root
            }
        }
    }

    // 2. LÍNEAS HORIZONTALES GUÍA SI NO HAY PISTAS O PARA EL ESPACIO VACÍO
    Column {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: lanesColumn.bottom
        anchors.bottom: parent.bottom
        z: 0

        Repeater {
            model: 20 // Fondo de grilla vacío estilo Logic
            Rectangle {
                width: parent.width
                height: root.trackHeight
                color: (index % 2 === 0) ? "#1A1B20" : "#17181C"
                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: 1
                    color: "#111216"
                }
            }
        }
    }

    // 3. CUADRÍCULA VERTICAL (Compases y Sub-divisiones / Beats)
    Row {
        anchors.fill: parent
        z: 1

        Repeater {
            model: root.totalBars

            Item {
                width: root.barWidth
                height: parent.height

                // Línea de compás principal
                Rectangle {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: 1
                    color: "#353945"
                    opacity: 0.4
                }

                // Sub-divisiones (Beats 2, 3, 4)
                Row {
                    anchors.fill: parent

                    Repeater {
                        model: 4
                        Item {
                            width: root.barWidth / 4
                            height: parent.height

                            Rectangle {
                                anchors.left: parent.left
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                width: 1
                                color: "#252833"
                                opacity: index === 0 ? 0.0 : 0.25
                            }
                        }
                    }
                }
            }
        }
    }

    // 4. 🚀 BARRA FLOTANTE DE ACCIONES MODULAR (OVERLAY GLOBAL ESTILO HONEYDEW / LOGIC)
    // Vive en la capa superior del timeline (z: 9999) para que NUNCA sea recortada ni tapada por ninguna pista o fondo.
    LogicClipBubbleBar {
        id: floatingBubbleBar
        z: 9999
        visible: root.selectedClip !== null &&
                 (typeof AudioEngine !== "undefined" && AudioEngine.regions && AudioEngine.regions.selectedRegionIndex >= 0) &&
                 !root.selectedClip.isLiveRecording &&
                 !root.selectedClip.isDragging

        clipIndex: root.selectedClip ? root.selectedClip.clipIndex : -1
        startBeat: root.selectedClip ? root.selectedClip.startBeat : 0.0
        snapGridBeats: root.selectedClip ? root.selectedClip.snapGridBeats : 1.0
        showAbove: root.selectedClip ? (root.selectedClip.trackIndex > 0) : false

        // Posicionamiento Vertical Inteligente:
        // En Pista 0: Se ubica DEBAJO del clip (+8px).
        // En Pistas 1+: Se ubica ARRIBA del clip (-height - 8px).
        y: {
            if (!root.selectedClip) return 0
            var clipPos = root.selectedClip.mapToItem(root, 0, 0)
            if (showAbove) {
                return clipPos.y - height - 8
            } else {
                return clipPos.y + root.selectedClip.height + 8
            }
        }

        // Posicionamiento Horizontal Dinámico Anclado al Click/Tap:
        // Centrado en selectedTapX (coordenada exacta mapeada al tocar la onda o título)
        x: {
            if (!root.selectedClip) return 0
            var halfW = width / 2
            var targetX = root.selectedTapX - halfW
            var minX = 8
            var maxX = root.width - width - 8
            return Math.max(minX, Math.min(maxX, targetX))
        }

        // Animación Pop-in y deslizamiento suave idéntico a Honeydew
        scale: visible ? 1.0 : 0.8
        opacity: visible ? 1.0 : 0.0
        Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutBack } }
        Behavior on opacity { NumberAnimation { duration: 100 } }
        Behavior on x { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
    }
}