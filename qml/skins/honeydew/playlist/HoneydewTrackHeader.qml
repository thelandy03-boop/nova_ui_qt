import QtQuick
import QtQuick.Layouts
import "../"

Rectangle {
    id: root

    property int trackIndex: 0
    property string trackName: "Track"
    property color trackColor: HoneydewTheme.getTrackColor(trackIndex)
    property bool isSelected: false
    property bool isMuted: false
    property bool isSolo: false
    property bool isArmed: false
    property real volumeVal: 1.0

    signal trackSelected(int idx)
    signal fxRequested(int idx, string name)

    width: 220
    height: 68
    radius: 8
    color: isSelected ? "#232630" : (headerMouse.containsMouse ? "#1F2128" : HoneydewTheme.bgCard)
    border.color: isSelected ? HoneydewTheme.flOrange : HoneydewTheme.borderSubtle
    border.width: isSelected ? 2 : 1

    // 🎨 Indicador de Color Lateral Izquierdo (Píldora Neón)
    Rectangle {
        id: colorPill
        width: 5
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.margins: 4
        radius: 3
        color: root.trackColor
    }

    MouseArea {
        id: headerMouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.trackSelected(root.trackIndex)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 15
        anchors.rightMargin: 8
        anchors.topMargin: 6
        anchors.bottomMargin: 6
        spacing: 4

        // FILA 1: Nombre de Pista + Botón FX rápido
        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            Text {
                text: (root.trackIndex + 1 < 10 ? "0" : "") + (root.trackIndex + 1)
                color: root.trackColor
                font.pixelSize: 10
                font.bold: true
            }

            Text {
                Layout.fillWidth: true
                text: root.trackName
                color: root.isSelected ? "#FFFFFF" : HoneydewTheme.textPrimary
                font.pixelSize: 12
                font.bold: true
                elide: Text.ElideRight
            }

            // Botón FX en cabecera
            Rectangle {
                width: 28
                height: 18
                radius: 4
                color: fxHeadMouse.containsMouse ? HoneydewTheme.flOrange : "#2A2D38"

                Text {
                    anchors.centerIn: parent
                    text: "FX"
                    color: fxHeadMouse.containsMouse ? "#000000" : HoneydewTheme.textSecondary
                    font.pixelSize: 8
                    font.bold: true
                }

                MouseArea {
                    id: fxHeadMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.fxRequested(root.trackIndex, root.trackName)
                }
            }
        }

        // FILA 2: Controles M / S / REC + Slider de Volumen Táctil
        RowLayout {
            Layout.fillWidth: true
            spacing: 4

            // MUTE (M)
            Rectangle {
                width: 22
                height: 22
                radius: 4
                color: root.isMuted ? "#E57373" : (muteMouse.containsMouse ? "#333744" : "#242730")
                border.color: root.isMuted ? "#E57373" : HoneydewTheme.borderSubtle

                Text {
                    anchors.centerIn: parent
                    text: "M"
                    color: root.isMuted ? "#000000" : HoneydewTheme.textSecondary
                    font.pixelSize: 9
                    font.bold: true
                }

                MouseArea {
                    id: muteMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (typeof AudioEngine !== "undefined" && AudioEngine.tracks) {
                            AudioEngine.tracks.setMute(root.trackIndex, !root.isMuted)
                        }
                    }
                }
            }

            // SOLO (S)
            Rectangle {
                width: 22
                height: 22
                radius: 4
                color: root.isSolo ? HoneydewTheme.flYellow : (soloMouse.containsMouse ? "#333744" : "#242730")
                border.color: root.isSolo ? HoneydewTheme.flYellow : HoneydewTheme.borderSubtle

                Text {
                    anchors.centerIn: parent
                    text: "S"
                    color: root.isSolo ? "#000000" : HoneydewTheme.textSecondary
                    font.pixelSize: 9
                    font.bold: true
                }

                MouseArea {
                    id: soloMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (typeof AudioEngine !== "undefined" && AudioEngine.tracks) {
                            AudioEngine.tracks.setSolo(root.trackIndex, !root.isSolo)
                        }
                    }
                }
            }

            // REC / ARM (R)
            Rectangle {
                width: 22
                height: 22
                radius: 4
                color: root.isArmed ? HoneydewTheme.flRed : (armMouse.containsMouse ? "#333744" : "#242730")
                border.color: root.isArmed ? "#FF334B" : HoneydewTheme.borderSubtle

                Text {
                    anchors.centerIn: parent
                    text: "●"
                    color: root.isArmed ? "#FFFFFF" : HoneydewTheme.textSecondary
                    font.pixelSize: 8
                    font.bold: true
                }

                MouseArea {
                    id: armMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (typeof AudioEngine !== "undefined" && AudioEngine.tracks) {
                            AudioEngine.tracks.setRecEnable(root.trackIndex, !root.isArmed)
                        }
                    }
                }
            }

            // Barra Slider de Volumen Compacto FL Studio
            Rectangle {
                Layout.fillWidth: true
                height: 14
                radius: 3
                color: "#16171D"
                border.color: HoneydewTheme.borderSubtle
                border.width: 1

                // Nivel de llenado
                Rectangle {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.margins: 1
                    radius: 2
                    width: Math.max(0, (parent.width - 2) * Math.min(1.0, root.volumeVal))
                    color: root.trackColor
                    opacity: 0.8
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.SizeHorCursor
                    onPositionChanged: (mouse) => {
                        if (pressed) {
                            var norm = Math.max(0.0, Math.min(1.0, mouse.x / width))
                            root.volumeVal = norm
                            if (typeof AudioEngine !== "undefined" && AudioEngine.tracks) {
                                AudioEngine.tracks.setGain(root.trackIndex, norm)
                            }
                        }
                    }
                }
            }
        }
    }
}
