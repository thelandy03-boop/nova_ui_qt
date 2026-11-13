import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import "../"

Rectangle {
    id: root

    property int baseOctave: 3 // C3 = MIDI 48
    property int numWhiteKeys: 14 // 2 octavas visibles
    property color channelColor: HoneydewTheme.accent

    signal noteTriggered(int midiNote, real velocity)
    signal noteReleased(int midiNote)

    color: HoneydewTheme.controlFill
    border.color: HoneydewTheme.menuFill
    border.width: 1

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ══════════════════════════════════════════════
        // TIRA DE AJUSTES SUPERIOR (STRIP_H = 3.0 U)
        // ══════════════════════════════════════════════
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 34
            color: HoneydewTheme.moduleFill
            border.color: HoneydewTheme.background
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 8

                // Botón RESET
                Rectangle {
                    width: 52
                    height: 24
                    radius: 12
                    color: resetMouse.pressed ? HoneydewTheme.accent : "#2C3238"
                    border.color: HoneydewTheme.menuFill
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: "RESET"
                        font.pixelSize: 9
                        font.bold: true
                        color: resetMouse.pressed ? HoneydewTheme.background : HoneydewTheme.text
                    }

                    MouseArea {
                        id: resetMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            root.baseOctave = 3
                        }
                    }
                }

                // Selector de Octava (- / +)
                Row {
                    spacing: 4
                    Layout.alignment: Qt.AlignVCenter

                    Text {
                        text: "OCTAVE"
                        font.pixelSize: 9
                        font.bold: true
                        color: HoneydewTheme.textSecondary
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Rectangle {
                        width: 22
                        height: 22
                        radius: 4
                        color: octDownMouse.pressed ? HoneydewTheme.accent : "#2A3136"
                        Text { anchors.centerIn: parent; text: "◀"; font.pixelSize: 8; color: HoneydewTheme.text }
                        MouseArea {
                            id: octDownMouse
                            anchors.fill: parent
                            onClicked: root.baseOctave = Math.max(1, root.baseOctave - 1)
                        }
                    }

                    Text {
                        text: "C" + root.baseOctave
                        font.pixelSize: 11
                        font.bold: true
                        color: HoneydewTheme.accent
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Rectangle {
                        width: 22
                        height: 22
                        radius: 4
                        color: octUpMouse.pressed ? HoneydewTheme.accent : "#2A3136"
                        Text { anchors.centerIn: parent; text: "▶"; font.pixelSize: 8; color: HoneydewTheme.text }
                        MouseArea {
                            id: octUpMouse
                            anchors.fill: parent
                            onClicked: root.baseOctave = Math.min(6, root.baseOctave + 1)
                        }
                    }
                }

                Item { Layout.fillWidth: true }

                // Indicador de modo de teclado
                Text {
                    text: "CLASSIC PIANO"
                    font.pixelSize: 9
                    font.bold: true
                    color: Qt.rgba(HoneydewTheme.text.r, HoneydewTheme.text.g, HoneydewTheme.text.b, 0.5)
                }
            }
        }

        // ══════════════════════════════════════════════
        // TECLADO PIANO (BLANCAS + NEGRAS)
        // Proporciones de keyboard.rs:
        // BLACK_KEY_W = 0.7 de una blanca
        // BLACK_KEY_H = 0.6 de la altura total
        // ══════════════════════════════════════════════
        Item {
            id: keysArea
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            readonly property real whiteKeyWidth: width / root.numWhiteKeys
            readonly property real blackKeyWidth: whiteKeyWidth * 0.70
            readonly property real blackKeyHeight: height * 0.60

            // 1. CAPA DE TECLAS BLANCAS
            Row {
                anchors.fill: parent

                Repeater {
                    model: root.numWhiteKeys

                    Rectangle {
                        id: whiteKey
                        width: keysArea.whiteKeyWidth
                        height: keysArea.height
                        color: keyMouse.pressed ? root.channelColor : HoneydewTheme.whiteKey
                        border.color: HoneydewTheme.background
                        border.width: 1
                        radius: 2

                        // Cálculo del número MIDI para tecla blanca
                        readonly property int octaveOffset: Math.floor(index / 7)
                        readonly property int noteInOctave: [0, 2, 4, 5, 7, 9, 11][index % 7]
                        readonly property int midiNote: (root.baseOctave * 12) + (octaveOffset * 12) + noteInOctave

                        // Etiqueta de la nota C (Do)
                        Text {
                            anchors.bottom: parent.bottom
                            anchors.bottomMargin: 8
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: (index % 7 === 0) ? ("C" + (root.baseOctave + Math.floor(index / 7))) : ""
                            font.pixelSize: 10
                            font.bold: true
                            color: HoneydewTheme.background
                        }

                        MouseArea {
                            id: keyMouse
                            anchors.fill: parent
                            onPressed: root.noteTriggered(whiteKey.midiNote, 0.85)
                            onReleased: root.noteReleased(whiteKey.midiNote)
                        }
                    }
                }
            }

            // 2. CAPA DE TECLAS NEGRAS (C#, D#, F#, G#, A#)
            Item {
                anchors.fill: parent

                Repeater {
                    // Índices de teclas blancas que llevan tecla negra a su derecha (0=C, 1=D, 3=F, 4=G, 5=A)
                    model: [
                        // Primera octava
                        { whiteIdx: 0, note: 1 },  // C#
                        { whiteIdx: 1, note: 3 },  // D#
                        { whiteIdx: 3, note: 6 },  // F#
                        { whiteIdx: 4, note: 8 },  // G#
                        { whiteIdx: 5, note: 10 }, // A#
                        // Segunda octava
                        { whiteIdx: 7, note: 13 }, // C#
                        { whiteIdx: 8, note: 15 }, // D#
                        { whiteIdx: 10, note: 18 },// F#
                        { whiteIdx: 11, note: 20 },// G#
                        { whiteIdx: 12, note: 22 } // A#
                    ]

                    Rectangle {
                        id: blackKey
                        x: (modelData.whiteIdx + 1) * keysArea.whiteKeyWidth - (keysArea.blackKeyWidth / 2)
                        y: 0
                        width: keysArea.blackKeyWidth
                        height: keysArea.blackKeyHeight
                        radius: 2
                        color: blackMouse.pressed ? root.channelColor : HoneydewTheme.blackKey
                        border.color: HoneydewTheme.background
                        border.width: 1
                        z: 10

                        readonly property int midiNote: (root.baseOctave * 12) + modelData.note

                        MouseArea {
                            id: blackMouse
                            anchors.fill: parent
                            onPressed: root.noteTriggered(blackKey.midiNote, 0.90)
                            onReleased: root.noteReleased(blackKey.midiNote)
                        }
                    }
                }
            }
        }
    }
}
