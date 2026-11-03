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
            Layout.preferredHeight: 30
            contentX: gridFlickable.contentX
            barWidth: root.barWidth
            totalBars: timelineGrid.totalBars
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

                // 🚀 Auto-scroll al arrastrar clips cerca del borde de la pantalla
                Timer {
                    id: autoScrollTimer
                    interval: 30
                    repeat: true
                    running: timelineGrid.selectedClip && timelineGrid.selectedClip.isDragging
                    onTriggered: {
                        var clip = timelineGrid.selectedClip
                        if (!clip || !gridFlickable) return
                        var clipGlobalX = clip.x - gridFlickable.contentX
                        var viewW = gridFlickable.width
                        // Borde derecho: avanzar hacia adelante
                        if (clipGlobalX + clip.width > viewW - 60) {
                            var maxScroll = Math.max(0, gridFlickable.contentWidth - viewW)
                            gridFlickable.contentX = Math.min(maxScroll, gridFlickable.contentX + 24)
                        } else if (clipGlobalX < 60 && gridFlickable.contentX > 0) {
                            // Borde izquierdo: retroceder
                            gridFlickable.contentX = Math.max(0, gridFlickable.contentX - 24)
                        }
                    }
                }

                LogicTimelineGrid {
                    id: timelineGrid
                    barWidth: root.barWidth
                }

                // 🤏 GESTO MULTITÁCTIL: ZOOM PINCH-TO-ZOOM CON DOS DEDOS (MÓVIL / TABLET)
                PinchHandler {
                    id: pinchHandler
                    target: null
                    grabPermissions: PointerHandler.CanTakeOverFromAnything | PointerHandler.ApprovesTakeOverByAnything
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

    // 🔍 CONTROLES FLOTANTES DE ZOOM RÁPIDO PARA CELULARES (1-TOQUE)
    Rectangle {
        id: zoomControlsBar
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 14
        width: 108
        height: 36
        radius: 18
        color: "#E6141824"
        border.color: "#2C344B"
        border.width: 1
        z: 60

        Row {
            anchors.centerIn: parent
            spacing: 2

            // Botón Zoom Out (-)
            Rectangle {
                width: 34
                height: 30
                radius: 15
                color: btnZoomOutMouse.pressed ? "#40FFFFFF" : "transparent"

                Text {
                    anchors.centerIn: parent
                    text: "－"
                    color: root.barWidth <= root.minBarWidth ? "#64748B" : "#F1F5F9"
                    font.pixelSize: 16
                    font.bold: true
                }

                MouseArea {
                    id: btnZoomOutMouse
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (typeof AudioEngine !== "undefined" && AudioEngine.hapticFeedback) {
                            AudioEngine.hapticFeedback()
                        }
                        root.barWidth = Math.max(root.minBarWidth, root.barWidth * 0.8)
                    }
                }
            }

            // Botón Reset 1:1
            Rectangle {
                width: 32
                height: 30
                radius: 15
                color: btnZoomResetMouse.pressed ? "#40FFFFFF" : "transparent"

                Text {
                    anchors.centerIn: parent
                    text: "1:1"
                    color: Math.abs(root.barWidth - 80.0) < 1.0 ? "#38BDF8" : "#94A3B8"
                    font.pixelSize: 11
                    font.bold: true
                }

                MouseArea {
                    id: btnZoomResetMouse
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (typeof AudioEngine !== "undefined" && AudioEngine.hapticFeedback) {
                            AudioEngine.hapticFeedback()
                        }
                        root.barWidth = 80.0
                    }
                }
            }

            // Botón Zoom In (+)
            Rectangle {
                width: 34
                height: 30
                radius: 15
                color: btnZoomInMouse.pressed ? "#40FFFFFF" : "transparent"

                Text {
                    anchors.centerIn: parent
                    text: "＋"
                    color: root.barWidth >= root.maxBarWidth ? "#64748B" : "#F1F5F9"
                    font.pixelSize: 16
                    font.bold: true
                }

                MouseArea {
                    id: btnZoomInMouse
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (typeof AudioEngine !== "undefined" && AudioEngine.hapticFeedback) {
                            AudioEngine.hapticFeedback()
                        }
                        root.barWidth = Math.min(root.maxBarWidth, root.barWidth * 1.25)
                    }
                }
            }
        }
    }
}