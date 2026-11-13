import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import "../"

Rectangle {
    id: root

    property bool isPlaying: typeof AudioEngine !== "undefined" && AudioEngine.isPlaying
    property bool isRecording: typeof AudioEngine !== "undefined" && AudioEngine.isRecording
    property bool isLooping: typeof AudioEngine !== "undefined" && AudioEngine.loopEnabled
    property bool isKeyboardOpen: true
    property bool isFxRackOpen: false

    signal togglePlay()
    signal toggleRecord()
    signal rewindRequested()
    signal toggleLoop()
    signal toggleKeyboard()
    signal toggleFxRack()
    signal tempoRequested()

    height: HoneydewTheme.transportH
    color: HoneydewTheme.controlFill
    border.color: HoneydewTheme.menuFill
    border.width: 1

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        spacing: 6

        // ══════════════════════════════════════════════
        // GRUPO IZQUIERDO: MIXER & KEYBOARD SETTINGS
        // (En Rust main_screen.rs: mixer and keyboard settings in Play's ring)
        // ══════════════════════════════════════════════
        // Botón Keyboard / Piano Panel Toggle
        Rectangle {
            width: 40
            height: 40
            radius: 20
            color: root.isKeyboardOpen ? HoneydewTheme.accent : "#242B30"
            border.color: HoneydewTheme.menuFill
            border.width: 2

            Text {
                anchors.centerIn: parent
                text: "🎹"
                font.pixelSize: 16
            }

            MouseArea {
                id: keysMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.toggleKeyboard()
            }
        }

        // Tonalidad / Escala
        Column {
            Layout.leftMargin: 4
            spacing: 1
            Text {
                text: "SCALE"
                font.pixelSize: 8
                font.bold: true
                color: Qt.rgba(HoneydewTheme.text.r, HoneydewTheme.text.g, HoneydewTheme.text.b, HoneydewTheme.scaleAlpha)
            }
            Text {
                text: "Chromatic"
                font.pixelSize: 11
                font.bold: true
                color: HoneydewTheme.text
            }
        }

        Item { Layout.fillWidth: true }

        // ══════════════════════════════════════════════
        // GRUPO CENTRAL CANÓNICO: REC, REWIND, PLAY, BPM, CONTROL
        // (En Rust main_screen.rs: slots.rec, slots.rewind, slots.play, slots.bpm, slots.control)
        // ══════════════════════════════════════════════

        // 1. REC (Círculo rojo fijo RECORD_COLOR = #FF0000 en Rust)
        Rectangle {
            width: 48
            height: 40
            radius: 6
            color: recMouse.pressed ? "#22272B" : (recMouse.containsMouse ? "#3A4248" : HoneydewTheme.moduleFill)
            border.color: root.isRecording ? HoneydewTheme.recordColor : HoneydewTheme.menuFill
            border.width: root.isRecording ? 2 : 1

            Column {
                anchors.centerIn: parent
                spacing: 2

                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 12
                    height: 12
                    radius: 6
                    color: HoneydewTheme.recordColor

                    SequentialAnimation on opacity {
                        running: root.isRecording
                        loops: Animation.Infinite
                        PropertyAnimation { to: 0.2; duration: 350 }
                        PropertyAnimation { to: 1.0; duration: 350 }
                    }
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "REC"
                    font.pixelSize: 8
                    font.bold: true
                    color: root.isRecording ? HoneydewTheme.recordColor : HoneydewTheme.text
                }
            }

            MouseArea {
                id: recMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.toggleRecord()
            }
        }

        // 2. REWIND (Rebobinar al inicio)
        Rectangle {
            width: 48
            height: 40
            radius: 6
            color: rewMouse.pressed ? "#22272B" : (rewMouse.containsMouse ? "#3A4248" : HoneydewTheme.moduleFill)
            border.color: HoneydewTheme.menuFill
            border.width: 1

            Column {
                anchors.centerIn: parent
                spacing: 2

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "⏮"
                    font.pixelSize: 13
                    color: HoneydewTheme.text
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "REWIND"
                    font.pixelSize: 8
                    font.bold: true
                    color: HoneydewTheme.text
                }
            }

            MouseArea {
                id: rewMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.rewindRequested()
            }
        }

        // 3. PLAY (Con anillo exterior PLAY_RING característico de Honeydew)
        Item {
            width: 48
            height: 48

            Rectangle {
                id: playRingRect
                anchors.fill: parent
                radius: width / 2
                color: "transparent"
                border.color: root.isPlaying ? HoneydewTheme.accent : HoneydewTheme.text
                border.width: 2.5

                Rectangle {
                    anchors.centerIn: parent
                    width: 38
                    height: 38
                    radius: width / 2
                    color: playMouse.pressed ? "#22272B" : (root.isPlaying ? HoneydewTheme.accent : HoneydewTheme.moduleFill)

                    Text {
                        anchors.centerIn: parent
                        text: root.isPlaying ? "■" : "▶"
                        font.pixelSize: root.isPlaying ? 14 : 16
                        font.bold: true
                        color: root.isPlaying ? HoneydewTheme.background : HoneydewTheme.text
                    }
                }
            }

            MouseArea {
                id: playMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.togglePlay()
            }
        }

        // 4. BPM / TEMPO (Display numérico Pt12 con etiqueta inferior)
        Rectangle {
            width: 58
            height: 40
            radius: 6
            color: bpmMouse.pressed ? "#22272B" : (bpmMouse.containsMouse ? "#3A4248" : HoneydewTheme.moduleFill)
            border.color: HoneydewTheme.menuFill
            border.width: 1

            Column {
                anchors.centerIn: parent
                spacing: 1

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: typeof AudioEngine !== "undefined" ? Math.round(AudioEngine.bpm).toString() : "120"
                    font.pixelSize: 14
                    font.bold: true
                    color: HoneydewTheme.text
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "BPM"
                    font.pixelSize: 8
                    font.bold: true
                    color: HoneydewTheme.accent
                }
            }

            MouseArea {
                id: bpmMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.tempoRequested()
            }
        }

        // 5. CONTROL / SNAP
        Rectangle {
            width: 52
            height: 40
            radius: 6
            color: ctrlMouse.pressed ? "#22272B" : (ctrlMouse.containsMouse ? "#3A4248" : HoneydewTheme.moduleFill)
            border.color: HoneydewTheme.menuFill
            border.width: 1

            Column {
                anchors.centerIn: parent
                spacing: 2

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "🧲"
                    font.pixelSize: 12
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "SNAP"
                    font.pixelSize: 8
                    font.bold: true
                    color: HoneydewTheme.text
                }
            }

            MouseArea {
                id: ctrlMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
            }
        }

        Item { Layout.fillWidth: true }

        // ══════════════════════════════════════════════
        // GRUPO DERECHO: LOOP Y FX RACK
        // ══════════════════════════════════════════════
        // Botón Loop
        Rectangle {
            width: 40
            height: 40
            radius: 6
            color: root.isLooping ? HoneydewTheme.accent : (loopMouse.containsMouse ? "#3A4248" : HoneydewTheme.moduleFill)
            border.color: root.isLooping ? HoneydewTheme.accent : HoneydewTheme.menuFill
            border.width: 1

            Text {
                anchors.centerIn: parent
                text: "🔁"
                font.pixelSize: 14
            }

            MouseArea {
                id: loopMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.toggleLoop()
            }
        }

        // Botón FX Rack Toggle
        Rectangle {
            width: 68
            height: 40
            radius: 6
            color: root.isFxRackOpen ? HoneydewTheme.accent : (fxMouse.containsMouse ? "#3A4248" : HoneydewTheme.moduleFill)
            border.color: root.isFxRackOpen ? HoneydewTheme.accent : HoneydewTheme.menuFill
            border.width: 1

            Row {
                anchors.centerIn: parent
                spacing: 4
                Text {
                    text: "🎚️"
                    font.pixelSize: 13
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    text: "RACK"
                    font.pixelSize: 9
                    font.bold: true
                    color: root.isFxRackOpen ? HoneydewTheme.background : HoneydewTheme.text
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            MouseArea {
                id: fxMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.toggleFxRack()
            }
        }
    }
}
