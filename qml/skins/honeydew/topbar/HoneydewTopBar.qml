import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import "../"

Rectangle {
    id: root

    height: 56
    color: HoneydewTheme.bgSurface
    border.color: HoneydewTheme.borderSubtle
    border.width: 1

    signal homeRequested()
    signal fxRackToggled()
    signal addTrackRequested()
    signal jsfxEditorRequested()
    signal reapackRequested()

    property bool isFxRackOpen: false
    property int activeFxCount: 0

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        spacing: 8

        // 🍊 1. BOTÓN LOGO FL / MENÚ
        Rectangle {
            Layout.preferredWidth: 38
            Layout.preferredHeight: 38
            radius: 8
            color: logoMouse.pressed ? "#E67600" : (logoMouse.containsMouse ? "#FF951A" : HoneydewTheme.flOrange)

            Text {
                anchors.centerIn: parent
                text: "🍈"
                font.pixelSize: 18
            }

            MouseArea {
                id: logoMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.homeRequested()
            }
        }

        // ➕ BOTÓN "ADD TRACK"
        Rectangle {
            Layout.preferredHeight: 36
            Layout.preferredWidth: 92
            radius: 6
            color: addTrackMouse.pressed ? "#2B2E38" : (addTrackMouse.containsMouse ? "#262831" : HoneydewTheme.bgSurfaceLight)
            border.color: addTrackMouse.containsMouse ? HoneydewTheme.flOrange : HoneydewTheme.borderSubtle
            border.width: 1

            Row {
                anchors.centerIn: parent
                spacing: 6
                Text {
                    text: "+"
                    color: HoneydewTheme.flOrange
                    font.pixelSize: 16
                    font.bold: true
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    text: "TRACK"
                    color: HoneydewTheme.textPrimary
                    font.pixelSize: 11
                    font.bold: true
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            MouseArea {
                id: addTrackMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.addTrackRequested()
            }
        }

        // 📟 2. DISPLAY CENTRAL TÁCTIL (BPM + BAR/BEAT + SNAP)
        Rectangle {
            Layout.preferredHeight: 38
            Layout.preferredWidth: 260
            radius: 6
            color: "#0F1013"
            border.color: HoneydewTheme.borderSubtle
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 6

                // TEMPO / BPM
                Row {
                    spacing: 4
                    Layout.alignment: Qt.AlignVCenter
                    Text {
                        text: "BPM"
                        color: HoneydewTheme.flOrange
                        font.pixelSize: 9
                        font.bold: true
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Text {
                        text: typeof AudioEngine !== "undefined" ? AudioEngine.bpm.toFixed(0) : "120"
                        color: HoneydewTheme.textPrimary
                        font.pixelSize: 13
                        font.bold: true
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    // Botones BPM +/-
                    Column {
                        spacing: 1
                        anchors.verticalCenter: parent.verticalCenter
                        Rectangle {
                            width: 14; height: 10; radius: 2
                            color: bpmUpMouse.pressed ? HoneydewTheme.flOrange : "#242630"
                            Text { anchors.centerIn: parent; text: "▲"; font.pixelSize: 6; color: "#FFFFFF" }
                            MouseArea {
                                id: bpmUpMouse
                                anchors.fill: parent
                                onClicked: if (typeof AudioEngine !== "undefined") AudioEngine.bpm = AudioEngine.bpm + 1
                            }
                        }
                        Rectangle {
                            width: 14; height: 10; radius: 2
                            color: bpmDownMouse.pressed ? HoneydewTheme.flOrange : "#242630"
                            Text { anchors.centerIn: parent; text: "▼"; font.pixelSize: 6; color: "#FFFFFF" }
                            MouseArea {
                                id: bpmDownMouse
                                anchors.fill: parent
                                onClicked: if (typeof AudioEngine !== "undefined") AudioEngine.bpm = Math.max(20, AudioEngine.bpm - 1)
                            }
                        }
                    }
                }

                Rectangle { width: 1; height: 20; color: HoneydewTheme.borderSubtle }

                // COMPÁS / TIEMPO
                Row {
                    Layout.fillWidth: true
                    spacing: 4
                    Layout.alignment: Qt.AlignVCenter
                    Text {
                        text: "BAR"
                        color: HoneydewTheme.textSecondary
                        font.pixelSize: 9
                        font.bold: true
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Text {
                        text: {
                            if (typeof AudioEngine !== "undefined") {
                                var currentBeat = AudioEngine.currentBeat;
                                var bar = Math.floor(currentBeat / 4) + 1;
                                var beat = Math.floor(currentBeat % 4) + 1;
                                return bar + "." + beat;
                            }
                            return "1.1";
                        }
                        color: HoneydewTheme.flCyan
                        font.pixelSize: 14
                        font.bold: true
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }

                Rectangle { width: 1; height: 20; color: HoneydewTheme.borderSubtle }

                // SNAP SELECTOR
                Rectangle {
                    width: 48
                    height: 24
                    radius: 4
                    color: "#1C1D24"
                    border.color: HoneydewTheme.borderSubtle
                    anchors.verticalCenter: parent.verticalCenter

                    Text {
                        anchors.centerIn: parent
                        text: "1/16"
                        color: HoneydewTheme.textSecondary
                        font.pixelSize: 10
                        font.bold: true
                    }
                }
            }
        }

        Item { Layout.fillWidth: true }

        // ⏯️ 3. CONTROLES DE TRANSPORTE FL STUDIO MOBILE (ESTILO CÁPSULA NEÓN)
        Row {
            spacing: 6
            Layout.alignment: Qt.AlignVCenter

            // 🔁 LOOP
            Rectangle {
                width: 36
                height: 36
                radius: 8
                color: (typeof AudioEngine !== "undefined" && AudioEngine.loopEnabled) ? HoneydewTheme.flCyan : (loopMouse.containsMouse ? "#2B2E38" : HoneydewTheme.bgSurfaceLight)
                border.color: (typeof AudioEngine !== "undefined" && AudioEngine.loopEnabled) ? "#8000D2FF" : HoneydewTheme.borderSubtle

                Text {
                    anchors.centerIn: parent
                    text: "🔁"
                    font.pixelSize: 12
                }

                MouseArea {
                    id: loopMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (typeof AudioEngine !== "undefined") {
                            AudioEngine.toggleLoop()
                        }
                    }
                }
            }

            // ⏹️ STOP
            Rectangle {
                width: 36
                height: 36
                radius: 8
                color: stopMouse.pressed ? "#2B2E38" : (stopMouse.containsMouse ? "#262831" : HoneydewTheme.bgSurfaceLight)
                border.color: HoneydewTheme.borderSubtle

                Rectangle {
                    anchors.centerIn: parent
                    width: 12
                    height: 12
                    radius: 2
                    color: HoneydewTheme.textPrimary
                }

                MouseArea {
                    id: stopMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (typeof AudioEngine !== "undefined") AudioEngine.stop()
                    }
                }
            }

            // ▶️ PLAY / PAUSE (GRANDE CON VERDE NEÓN)
            Rectangle {
                width: 46
                height: 36
                radius: 8
                property bool isPlaying: typeof AudioEngine !== "undefined" && AudioEngine.isPlaying
                color: isPlaying ? HoneydewTheme.flGreen : (playMouse.containsMouse ? "#262831" : HoneydewTheme.bgSurfaceLight)
                border.color: isPlaying ? "#802FE575" : HoneydewTheme.borderSubtle
                border.width: isPlaying ? 2 : 1

                Text {
                    anchors.centerIn: parent
                    text: parent.isPlaying ? "❚❚" : "▶"
                    color: parent.isPlaying ? "#000000" : HoneydewTheme.flGreen
                    font.pixelSize: 14
                    font.bold: true
                }

                MouseArea {
                    id: playMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (typeof AudioEngine !== "undefined") {
                            if (AudioEngine.isPlaying) AudioEngine.stop()
                            else AudioEngine.play()
                        }
                    }
                }
            }

            // ⏺️ RECORD (CON PULSO ROJO NEÓN)
            Rectangle {
                width: 42
                height: 36
                radius: 8
                property bool isRecording: typeof AudioEngine !== "undefined" && AudioEngine.isRecording
                color: isRecording ? HoneydewTheme.flRed : (recMouse.containsMouse ? "#2B2E38" : HoneydewTheme.bgSurfaceLight)
                border.color: isRecording ? "#80FF334B" : HoneydewTheme.borderSubtle
                border.width: isRecording ? 2 : 1

                Rectangle {
                    anchors.centerIn: parent
                    width: 12
                    height: 12
                    radius: 6
                    color: parent.isRecording ? "#FFFFFF" : HoneydewTheme.flRed

                    SequentialAnimation on opacity {
                        running: parent.parent.isRecording
                        loops: Animation.Infinite
                        PropertyAnimation { to: 0.2; duration: 400 }
                        PropertyAnimation { to: 1.0; duration: 400 }
                    }
                }

                MouseArea {
                    id: recMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (typeof AudioEngine !== "undefined") {
                            AudioEngine.toggleRecord()
                        }
                    }
                }
            }
        }

        Item { Layout.fillWidth: true }

        // 🎚️ 4. BOTÓN RACK DE EFECTOS / JSFX (ESTILO FL STUDIO)
        Rectangle {
            Layout.preferredHeight: 36
            Layout.preferredWidth: 84
            radius: 8
            color: root.isFxRackOpen ? HoneydewTheme.flOrange : (fxBtnMouse.containsMouse ? "#262831" : HoneydewTheme.bgSurfaceLight)
            border.color: root.isFxRackOpen ? "#80FF8500" : HoneydewTheme.borderSubtle

            Row {
                anchors.centerIn: parent
                spacing: 5
                Text {
                    text: "🎚️"
                    font.pixelSize: 13
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    text: "FX RACK"
                    color: root.isFxRackOpen ? "#000000" : HoneydewTheme.textPrimary
                    font.pixelSize: 10
                    font.bold: true
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            MouseArea {
                id: fxBtnMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.fxRackToggled()
            }
        }

        // ⚡ BOTÓN JSFX EDITOR & REAPACK
        Rectangle {
            Layout.preferredWidth: 36
            Layout.preferredHeight: 36
            radius: 8
            color: jsfxBtnMouse.pressed ? "#2B2E38" : (jsfxBtnMouse.containsMouse ? "#262831" : HoneydewTheme.bgSurfaceLight)
            border.color: HoneydewTheme.borderSubtle

            Text {
                anchors.centerIn: parent
                text: "⚡"
                font.pixelSize: 14
            }

            MouseArea {
                id: jsfxBtnMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.jsfxEditorRequested()
            }
        }
    }
}
