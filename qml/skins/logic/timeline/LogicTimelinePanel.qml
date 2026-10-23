import QtQuick
import QtQuick.Layouts
import "components"

Rectangle {
    id: root
    color: "#1E1F24"

    property real barWidth: 80.0          // Ancho base de cada compás (80px)
    property real minBarWidth: 35.0       // Zoom máximo alejado
    property real maxBarWidth: 240.0      // Zoom máximo acercado
    property real currentPlayheadX: 0     // Inicia en el Compás 1 (x = 0)

    property alias scrollY: gridFlickable.contentY

    // 🧹 CONEXIÓN LIMPIA C++ PARA SINCRO DE AGUJA
    Connections {
        target: typeof AudioEngine !== "undefined" ? AudioEngine : null

        function onPositionChanged() {
            root.currentPlayheadX = AudioEngine.beatToPixel(AudioEngine.currentBeat, root.barWidth);
            root.followPlayhead();
        }
    }

    // 🎥 LA CÁMARA SIGUE AL PLAYHEAD (como en cualquier DAW)
    // Sin esto, cuando el playhead avanza más allá de la parte visible se
    // sale de la pantalla y da la impresión de que el transporte se ha
    // quedado clavado (pasaba en el compás 14, el final de la ventana).
    function followPlayhead() {
        if (!gridFlickable || root.currentPlayheadX <= 0) return;

        const viewW = gridFlickable.width;
        const margin = 80;                 // px de margen antes del borde
        const x = root.currentPlayheadX;
        const maxX = Math.max(0, gridFlickable.contentWidth - viewW);

        if (x > gridFlickable.contentX + viewW) {
            // Se ha salido por la derecha: la vista avanza con él
            gridFlickable.contentX = Math.max(0, Math.min(x - viewW + margin, maxX));
        } else if (x < gridFlickable.contentX) {
            // Se ha ido por la izquierda: la vista vuelve
            gridFlickable.contentX = Math.max(0, Math.min(x - margin, maxX));
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // 1. Regla
        LogicTimelineRuler {
            Layout.fillWidth: true
            Layout.preferredHeight: 28
            contentX: gridFlickable.contentX
            barWidth: root.barWidth
        }

        // 2. Grilla y Playhead
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            Flickable {
                id: gridFlickable
                anchors.fill: parent
                contentWidth: timelineGrid.width
                contentHeight: timelineGrid.height
                boundsBehavior: Flickable.StopAtBounds
                flickableDirection: Flickable.HorizontalAndVerticalFlick

                LogicTimelineGrid {
                    id: timelineGrid
                    barWidth: root.barWidth
                }

                // 🤏 GESTO MULTITÁCTIL: ZOOM PINCH-TO-ZOOM CON DOS DEDOS (MÓVIL / TABLET)
                PinchHandler {
                    id: pinchHandler
                    target: null
                    property real initialBarWidth: 80.0

                    onActiveChanged: {
                        if (active) {
                            initialBarWidth = root.barWidth
                        }
                    }

                    onScaleChanged: {
                        if (active) {
                            var newWidth = initialBarWidth * scale
                            root.barWidth = Math.max(root.minBarWidth, Math.min(root.maxBarWidth, newWidth))
                        }
                    }
                }

                // 🖱️ MOUSE AREA: DESPLAZAMIENTO + ZOOM CTRL+RUEDA (ESCRITORIO)
                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.NoButton

                    onWheel: (wheel) => {
                        if (wheel.modifiers & Qt.ControlModifier) {
                            // Zoom horizontal con Ctrl + Rueda
                            var zoomFactor = wheel.angleDelta.y > 0 ? 1.15 : 0.88
                            var updatedBarWidth = root.barWidth * zoomFactor
                            root.barWidth = Math.max(root.minBarWidth, Math.min(root.maxBarWidth, updatedBarWidth))
                        } else if (wheel.modifiers & Qt.ShiftModifier) {
                            // Desplazamiento Vertical
                            var newY = gridFlickable.contentY - wheel.angleDelta.y
                            gridFlickable.contentY = Math.max(0, Math.min(gridFlickable.contentHeight - gridFlickable.height, newY))
                        } else {
                            // Desplazamiento Horizontal
                            var delta = wheel.angleDelta.y !== 0 ? wheel.angleDelta.y : wheel.angleDelta.x
                            var newX = gridFlickable.contentX - delta
                            gridFlickable.contentX = Math.max(0, Math.min(gridFlickable.contentWidth - gridFlickable.width, newX))
                        }
                    }
                }
            }

        }
    }

    // 🎯 AGUJA DE TIEMPO INTERACTIVA
    // Vive por encima de la regla (no dentro del Flickable, que recorta) para
    // que la línea suba hasta la altura de los números de compás.
    Item {
        id: playheadLayer
        anchors.fill: parent
        clip: true
        z: 50

        LogicPlayhead {
            timelineX: root.currentPlayheadX
            contentX: gridFlickable.contentX
            barWidth: root.barWidth
        }
    }
}