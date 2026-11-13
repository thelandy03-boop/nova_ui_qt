import QtQuick
import NovaStudio 1.0
import "../"

Item {
    id: root

    property int clipIndex: index
    property string clipId: model.regionId || ""
    property string clipName: model.name || "Audio Clip"
    property double startBeat: model.startBeat || 0.0
    property double lengthBeats: model.lengthBeats || 4.0
    property int trackIndex: model.trackIndex || 0
    property color clipColor: HoneydewTheme.getTrackColor(trackIndex)
    property bool isSelected: AudioEngine.regions && AudioEngine.regions.selectedRegionIndex === index
    property bool isLiveRecording: model.isLiveRecording || false
    property real barWidth: 160.0

    x: AudioEngine.beatToPixel(startBeat, barWidth)
    width: isLiveRecording ? Math.max(16, AudioEngine.beatToPixel(lengthBeats, barWidth)) : Math.max(32, AudioEngine.beatToPixel(lengthBeats, barWidth))
    height: 64

    // Contenedor Visual del Clip FL Studio
    Rectangle {
        id: clipBody
        anchors.fill: parent
        radius: 6
        color: root.isLiveRecording ? "#D32F2F" : Qt.darker(root.clipColor, 1.4)
        border.color: root.isSelected ? "#FFFFFF" : (root.isLiveRecording ? "#FF1744" : root.clipColor)
        border.width: root.isSelected ? 2 : 1
        clip: true

        // Cabecera Translúcida Superior
        Rectangle {
            id: headerBar
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 18
            color: root.isSelected ? "#40FFFFFF" : "#30000000"

            Row {
                anchors.left: parent.left
                anchors.leftMargin: 6
                anchors.verticalCenter: parent.verticalCenter
                spacing: 4

                Text {
                    text: root.isLiveRecording ? "🔴" : "🎵"
                    font.pixelSize: 9
                    anchors.verticalCenter: parent.verticalCenter
                }

                Text {
                    text: root.clipName
                    color: "#FFFFFF"
                    font.pixelSize: 10
                    font.bold: true
                    elide: Text.ElideRight
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }

        // Forma de Onda Vectorial HD (NovaWaveformItem)
        NovaWaveformItem {
            id: waveform
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: headerBar.bottom
            anchors.bottom: parent.bottom
            regionIndex: root.clipIndex
            regionId: root.clipId
            isLiveRecording: root.isLiveRecording
            peakColor: "#FFFFFF"
            rmsColor: Qt.rgba(1, 1, 1, 0.4)
            startBeat: root.startBeat
            lengthBeats: root.lengthBeats
            pixelsPerBeat: root.barWidth / 4.0
            visible: true
        }

        // MouseArea para selección y arrastre
        MouseArea {
            id: dragArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor

            onClicked: {
                if (typeof AudioEngine !== "undefined" && AudioEngine.regions) {
                    AudioEngine.regions.selectedRegionIndex = root.clipIndex
                }
            }
        }
    }
}
