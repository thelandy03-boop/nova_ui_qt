import QtQuick

Rectangle {
    id: root
    width: parent ? parent.width : 4800
    height: 74
    color: (index % 2 === 0) ? "#1E1F24" : "#191A1E"

    property int trackIndex: index

    // Si algún clip de este carril está seleccionado, este carril tiene prioridad visual (z: 100)
    // para que la barra de acciones flotante (arriba o abajo) se dibuje por encima de todas las demás pistas.
    z: {
        var sel = AudioEngine.regions ? AudioEngine.regions.selectedRegionIndex : -1
        if (sel >= 0) {
            var idx = AudioEngine.regions.index(sel, 0)
            var track = AudioEngine.regions.data(idx, 258) // TrackIndexRole
            if (track === root.trackIndex) return 100
        }
        return 1
    }

    // Ancho de compás real (lo propaga el panel): con 80 fijo los clips no
    // escalaban al hacer zoom ni se alineaban con las líneas de la rejilla.
    property real barWidth: 80.0

    // Clic en espacio vacío del carril deselecciona cualquier clip activo (estilo Honeydew)
    MouseArea {
        anchors.fill: parent
        z: 0
        onPressed: {
            if (AudioEngine.regions) {
                AudioEngine.regions.selectedRegionIndex = -1
            }
        }
    }

    // Franja lateral de color
    Rectangle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 3
        color: model.trackColor || "#3498db"
    }

    // Separador inferior
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: "#111216"
    }

    // Nombre de marca de agua
    Text {
        anchors.left: parent.left
        anchors.leftMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        text: model.trackName
        color: "#2C2F38"
        font.pixelSize: 18
        font.bold: true
    }

    // 🎵 RENDERIZADO DINÁMICO DE CLIPS DE AUDIO CORRESPONDIENTES A ESTA PISTA
    Repeater {
        model: AudioEngine.regions
        delegate: LogicAudioClip {
            // Se dibuja solo si el clip pertenece a este carril/pista
            visible: model.trackIndex === root.trackIndex
            barWidth: root.barWidth
        }
    }
}