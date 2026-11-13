import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import "../"

Rectangle {
    id: root

    property real barWidth: 140.0
    property real trackHeight: 74.0
    property int selectedTrackIndex: 0
    property string selectedTrackName: "Audio 1"

    signal trackSelected(int idx, string name)

    color: HoneydewTheme.bgMain

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // ══════════════════════════════════════════════
        // COLUMNA IZQUIERDA: CABECERAS DE PISTAS (TCP)
        // ══════════════════════════════════════════════
        Rectangle {
            Layout.preferredWidth: 230
            Layout.fillHeight: true
            color: HoneydewTheme.bgSurface
            border.color: HoneydewTheme.borderSubtle
            border.width: 1
            z: 2

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                // Cabecera superior de la columna de pistas
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 38
                    color: HoneydewTheme.bgSurfaceLight
                    border.color: HoneydewTheme.borderSubtle
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 8

                        Text {
                            text: "TRACKS"
                            color: HoneydewTheme.textSecondary
                            font.pixelSize: 11
                            font.bold: true
                            Layout.fillWidth: true
                        }

                        // Botón Añadir Pista
                        Rectangle {
                            width: 24
                            height: 24
                            radius: 4
                            color: addTrkMouse.pressed ? HoneydewTheme.flOrange : "#2A2D38"

                            Text {
                                anchors.centerIn: parent
                                text: "+"
                                color: addTrkMouse.pressed ? "#000000" : HoneydewTheme.flOrange
                                font.pixelSize: 14
                                font.bold: true
                            }

                            MouseArea {
                                id: addTrkMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (typeof AudioEngine !== "undefined" && AudioEngine.tracks) {
                                        AudioEngine.tracks.addAudioTrack("Audio " + (AudioEngine.tracks.rowCount() + 1))
                                    }
                                }
                            }
                        }
                    }
                }

                // Lista de Cabeceras con ScrollView
                ScrollView {
                    id: headerScrollView
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                    ScrollBar.vertical.policy: ScrollBar.AsNeeded
                    clip: true

                    Column {
                        width: parent.width
                        spacing: 4
                        topPadding: 4
                        bottomPadding: 4

                        Repeater {
                            model: typeof AudioEngine !== "undefined" ? AudioEngine.tracks : null

                            HoneydewTrackHeader {
                                width: headerScrollView.width - 8
                                height: root.trackHeight - 4
                                anchors.horizontalCenter: parent.horizontalCenter
                                trackIndex: index
                                trackName: model.name || ("Track " + (index + 1))
                                isSelected: root.selectedTrackIndex === index
                                isMuted: model.muted || false
                                isSolo: model.soloed || false
                                isArmed: model.recordArmed || false
                                volumeVal: model.volume || 1.0

                                onTrackSelected: (idx) => {
                                    root.selectedTrackIndex = idx
                                    root.selectedTrackName = model.name || ("Track " + (idx + 1))
                                    root.trackSelected(idx, root.selectedTrackName)
                                }

                                onFxRequested: (idx, nm) => {
                                    root.selectedTrackIndex = idx
                                    root.selectedTrackName = nm
                                    root.trackSelected(idx, nm)
                                }
                            }
                        }
                    }
                }
            }
        }

        // ══════════════════════════════════════════════
        // COLUMNA DERECHA: TIMELINE / PLAYLIST GRID
        // ══════════════════════════════════════════════
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: HoneydewTheme.bgMain
            clip: true

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                // 📏 REGLA DE COMPASES (RULER)
                Rectangle {
                    id: rulerArea
                    Layout.fillWidth: true
                    Layout.preferredHeight: 38
                    color: "#181920"
                    border.color: HoneydewTheme.borderSubtle
                    border.width: 1

                    Flickable {
                        id: rulerFlickable
                        anchors.fill: parent
                        contentWidth: 32 * root.barWidth // 32 compases iniciales
                        contentX: gridFlickable.contentX
                        interactive: false

                        Canvas {
                            anchors.fill: parent
                            antialiasing: true

                            onPaint: {
                                var ctx = getContext("2d");
                                ctx.reset();

                                var totalBars = 32;
                                ctx.font = "10px sans-serif";

                                for (var b = 0; b < totalBars; ++b) {
                                    var x = b * root.barWidth;

                                    // Línea principal de compás
                                    ctx.beginPath();
                                    ctx.moveTo(x, 18);
                                    ctx.lineTo(x, height);
                                    ctx.lineWidth = 1;
                                    ctx.strokeStyle = "#3D4150";
                                    ctx.stroke();

                                    // Número de compás
                                    ctx.fillStyle = "#A0A6B5";
                                    ctx.fillText((b + 1).toString(), x + 6, 26);

                                    // Subdivisiones de tiempos (4 beats por compás)
                                    for (var sub = 1; sub < 4; ++sub) {
                                        var subX = x + sub * (root.barWidth / 4);
                                        ctx.beginPath();
                                        ctx.moveTo(subX, 26);
                                        ctx.lineTo(subX, height);
                                        ctx.lineWidth = 0.5;
                                        ctx.strokeStyle = "#2B2E38";
                                        ctx.stroke();
                                    }
                                }
                            }
                        }
                    }

                    // Clic en la regla mueve el cabezal de reproducción
                    MouseArea {
                        anchors.fill: parent
                        onClicked: (mouse) => {
                            if (typeof AudioEngine !== "undefined") {
                                var clickX = mouse.x + gridFlickable.contentX;
                                var beat = clickX / (root.barWidth / 4.0);
                                AudioEngine.locateBeat(beat);
                            }
                        }
                    }
                }

                // 🎹 ÁREA DE GRID / CLIPS
                Flickable {
                    id: gridFlickable
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    contentWidth: 32 * root.barWidth
                    contentHeight: Math.max(height, (typeof AudioEngine !== "undefined" && AudioEngine.tracks ? AudioEngine.tracks.rowCount() : 1) * root.trackHeight)
                    contentY: headerScrollView.contentItem ? headerScrollView.contentItem.contentY : 0
                    clip: true

                    // Fondo de cuadrícula de pistas y compases
                    Canvas {
                        anchors.fill: parent
                        onPaint: {
                            var ctx = getContext("2d");
                            ctx.reset();

                            var numTracks = typeof AudioEngine !== "undefined" && AudioEngine.tracks ? AudioEngine.tracks.rowCount() : 4;
                            var totalBars = 32;

                            // 1. Carriles de pistas horizontales
                            for (var t = 0; t < numTracks; ++t) {
                                var y = t * root.trackHeight;
                                ctx.fillStyle = (t % 2 === 0) ? "#141519" : "#17181D";
                                ctx.fillRect(0, y, width, root.trackHeight);

                                ctx.beginPath();
                                ctx.moveTo(0, y + root.trackHeight);
                                ctx.lineTo(width, y + root.trackHeight);
                                ctx.lineWidth = 1;
                                ctx.strokeStyle = "#20222A";
                                ctx.stroke();
                            }

                            // 2. Líneas verticales de compás
                            for (var b = 0; b < totalBars; ++b) {
                                var x = b * root.barWidth;
                                ctx.beginPath();
                                ctx.moveTo(x, 0);
                                ctx.lineTo(x, height);
                                ctx.lineWidth = 1;
                                ctx.strokeStyle = "#252833";
                                ctx.stroke();
                            }
                        }
                    }

                    // Renderizado de Clips
                    Item {
                        anchors.fill: parent

                        Repeater {
                            model: typeof AudioEngine !== "undefined" ? AudioEngine.regions : null

                            HoneydewClip {
                                clipIndex: index
                                clipId: model.regionId || ""
                                clipName: model.name || "Audio"
                                startBeat: model.startBeat || 0.0
                                lengthBeats: model.lengthBeats || 4.0
                                trackIndex: model.trackIndex || 0
                                isLiveRecording: model.isLiveRecording || false
                                barWidth: root.barWidth
                                y: (model.trackIndex || 0) * root.trackHeight + 5
                                height: root.trackHeight - 10
                            }
                        }

                        // 🔴 CABEZAL DE REPRODUCCIÓN (PLAYHEAD)
                        Rectangle {
                            id: playhead
                            width: 2
                            height: parent.height
                            color: HoneydewTheme.flOrange
                            z: 99
                            x: typeof AudioEngine !== "undefined" ? AudioEngine.beatToPixel(AudioEngine.currentBeat, root.barWidth) : 0

                            // Marcador triangular superior del cabezal
                            Canvas {
                                width: 14
                                height: 12
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.top: parent.top

                                onPaint: {
                                    var ctx = getContext("2d");
                                    ctx.reset();
                                    ctx.beginPath();
                                    ctx.moveTo(0, 0);
                                    ctx.lineTo(width, 0);
                                    ctx.lineTo(width / 2, height);
                                    ctx.closePath();
                                    ctx.fillStyle = HoneydewTheme.flOrange;
                                    ctx.fill();
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
