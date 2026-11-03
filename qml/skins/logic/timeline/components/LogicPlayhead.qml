import QtQuick

Item {
    id: root

    property real timelineX: 0
    property real contentX: 0
    property real barWidth: 80.0
    property bool isDragging: headMouseArea.pressed || bodyMouseArea.pressed

    // 🔒 Posición X blindada con hitbox táctil ampliada (56px total, centro en 28px)
    x: isDragging ? (dragScreenX - 28) : (Math.max(0, root.timelineX) - root.contentX - 28)
    property real dragScreenX: 0

    anchors.top: parent.top
    anchors.bottom: parent.bottom
    width: 56
    z: 50

    // 1. Halo brillante de arrastre activo (feedback visual inmediato al tocar en pantalla)
    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 14
        radius: 7
        color: "#30FFCC00"
        visible: root.isDragging
    }

    // 2. Aguja vertical (roja en reposo, dorada activa durante el arrastre)
    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 10
        anchors.bottom: parent.bottom
        width: root.isDragging ? 3 : 2
        color: root.isDragging ? "#FFCC00" : "#FF3B30"
        opacity: 0.95
    }

    // 3. Indicador de cabeza táctil (Triángulo / Icono estilizado con base ampliada)
    Item {
        id: headContainer
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 1
        width: 32
        height: 28

        // Halo circular de toque
        Rectangle {
            anchors.centerIn: parent
            width: root.isDragging ? 32 : 24
            height: root.isDragging ? 32 : 24
            radius: 16
            color: root.isDragging ? "#60FFCC00" : (headMouseArea.containsMouse ? "#30FF3B30" : "transparent")
            scale: root.isDragging ? 1.1 : 1.0
            Behavior on scale { NumberAnimation { duration: 120 } }
        }

        Image {
            id: headIcon
            anchors.centerIn: parent
            width: root.isDragging ? 22 : 20
            height: root.isDragging ? 22 : 20
            source: Qt.resolvedUrl("../icons/svg/LogicPlayheadHead.svg")
            fillMode: Image.PreserveAspectFit
            smooth: true
            antialiasing: true
        }
    }

    // 4. Área táctil del cabezal superior (Hitbox ergonómica de 56x50px para móviles)
    MouseArea {
        id: headMouseArea
        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        width: parent.width
        height: 50
        hoverEnabled: true
        cursorShape: Qt.SizeHorCursor

        onPressed: (mouse) => {
            if (typeof AudioEngine !== "undefined" && AudioEngine.hapticFeedback) {
                AudioEngine.hapticFeedback()
            }
            updatePosition(mouse)
        }

        onPositionChanged: (mouse) => {
            if (pressed) updatePosition(mouse)
        }

        onReleased: finalizePosition()
    }

    // 5. Área táctil de cuerpo completo (permite agarrar la aguja desde cualquier pista en el celular)
    MouseArea {
        id: bodyMouseArea
        anchors.top: headMouseArea.bottom
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        width: 44
        hoverEnabled: true
        cursorShape: Qt.SizeHorCursor

        onPressed: (mouse) => {
            if (typeof AudioEngine !== "undefined" && AudioEngine.hapticFeedback) {
                AudioEngine.hapticFeedback()
            }
            updatePositionFromBody(mouse)
        }

        onPositionChanged: (mouse) => {
            if (pressed) updatePositionFromBody(mouse)
        }

        onReleased: finalizePosition()
    }

    function updatePosition(mouse) {
        if (typeof AudioEngine === "undefined") return
        var parentContainer = root.parent
        if (!parentContainer) return

        var screenPt = mapToItem(parentContainer, mouse.x, mouse.y)
        var timelinePixelX = Math.max(0, screenPt.x + root.contentX)
        root.dragScreenX = timelinePixelX - root.contentX

        var targetBeat = AudioEngine.pixelToBeat(timelinePixelX, root.barWidth)
        AudioEngine.locateBeat(targetBeat)
    }

    function updatePositionFromBody(mouse) {
        if (typeof AudioEngine === "undefined") return
        var parentContainer = root.parent
        if (!parentContainer) return

        var screenPt = bodyMouseArea.mapToItem(parentContainer, mouse.x, mouse.y)
        var timelinePixelX = Math.max(0, screenPt.x + root.contentX)
        root.dragScreenX = timelinePixelX - root.contentX

        var targetBeat = AudioEngine.pixelToBeat(timelinePixelX, root.barWidth)
        AudioEngine.locateBeat(targetBeat)
    }

    function finalizePosition() {
        if (typeof AudioEngine !== "undefined") {
            var timelinePixelX = Math.max(0, (root.x + 28) + root.contentX)
            var targetBeat = AudioEngine.pixelToBeat(timelinePixelX, root.barWidth)
            AudioEngine.locateBeat(targetBeat)
        }
    }
}