import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "topbar"
import "playlist"
import "rack"
import "../logic/dock"
import "../../dialogs"

Rectangle {
    id: root
    anchors.fill: parent
    color: HoneydewTheme.bgMain

    property bool showFxRack: false
    property bool showJsfxDock: false
    property int activeTrackIndex: 0
    property string activeTrackName: "Audio 1"

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // 🍊 ZONA 1: TOP BAR FL STUDIO MOBILE (56px)
        HoneydewTopBar {
            id: topBar
            Layout.fillWidth: true
            Layout.preferredHeight: 56
            isFxRackOpen: root.showFxRack

            onHomeRequested: {
                if (root.parent && root.parent.parent && root.parent.parent.goBack) {
                    root.parent.parent.goBack()
                }
            }
            onFxRackToggled: {
                root.showFxRack = !root.showFxRack
            }
            onAddTrackRequested: {
                if (typeof AudioEngine !== "undefined" && AudioEngine.tracks) {
                    AudioEngine.tracks.addAudioTrack("Audio " + (AudioEngine.tracks.rowCount() + 1))
                }
            }
            onJsfxEditorRequested: {
                root.showJsfxDock = true
                if (jsfxDock) {
                    jsfxDock.openNewScript(root.activeTrackIndex)
                }
            }
            onReapackRequested: {
                jsfxPackageBrowserDialog.open()
            }
        }

        // 🎹 ZONA 2: PLAYLIST TIMELINE + FX RACK DESLIZANTE
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            RowLayout {
                anchors.fill: parent
                spacing: 0

                // Panel Principal: Playlist & Timeline
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 0

                    HoneydewPlaylistPanel {
                        id: playlistPanel
                        Layout.fillWidth: true
                        Layout.fillHeight: !jsfxDock.isMaximized
                        visible: !jsfxDock.isMaximized

                        onTrackSelected: (idx, name) => {
                            root.activeTrackIndex = idx
                            root.activeTrackName = name
                        }
                    }

                    // Dock de Código JSFX Integrado (cuando se abre)
                    LogicJsfxDock {
                        id: jsfxDock
                        Layout.fillWidth: true
                        Layout.fillHeight: root.showJsfxDock && jsfxDock.isMaximized
                        Layout.preferredHeight: root.showJsfxDock ? (jsfxDock.isMaximized ? parent.height : Math.min(root.height * 0.72, 420)) : 0
                        visible: root.showJsfxDock
                        clip: true

                        onCloseRequested: {
                            root.showJsfxDock = false
                        }
                        onReapackRequested: {
                            jsfxPackageBrowserDialog.open()
                        }

                        Behavior on Layout.preferredHeight {
                            NumberAnimation { duration: 180; easing.type: Easing.InOutQuad }
                        }
                    }
                }

                // 🎚️ Panel Lateral Derecho: FX Rack Deslizante
                HoneydewFxRack {
                    id: fxRack
                    Layout.preferredWidth: root.showFxRack ? 320 : 0
                    Layout.fillHeight: true
                    visible: root.showFxRack || width > 0
                    clip: true
                    trackIndex: root.activeTrackIndex
                    trackName: root.activeTrackName

                    onCloseRequested: {
                        root.showFxRack = false
                    }
                    onEditJsfxRequested: (trkIdx, plgIdx, plgNm) => {
                        root.showJsfxDock = true
                        if (jsfxDock) {
                            jsfxDock.openForPlugin(trkIdx, plgIdx, plgNm)
                        }
                    }
                    onReapackRequested: {
                        jsfxPackageBrowserDialog.open()
                    }

                    Behavior on Layout.preferredWidth {
                        NumberAnimation { duration: 200; easing.type: Easing.InOutQuad }
                    }
                }
            }
        }

        // 📟 ZONA 3: STATUS BAR INFERIOR
        Rectangle {
            Layout.fillWidth: true
            height: 24
            color: "#111215"
            border.color: HoneydewTheme.borderSubtle
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12

                Text {
                    text: "🍈 HONEYDEW ENGINE  •  44.1 kHz / 32-bit float"
                    color: HoneydewTheme.textMuted
                    font.pixelSize: 10
                }

                Item { Layout.fillWidth: true }

                Text {
                    text: "BPM: " + (typeof AudioEngine !== "undefined" ? AudioEngine.bpm.toFixed(1) : "120.0") + "  |  FL STUDIO MOBILE STYLE"
                    color: HoneydewTheme.flOrange
                    font.pixelSize: 10
                    font.bold: true
                }
            }
        }
    }

    // Diálogo ReaPack JSFX
    JsfxPackageBrowserDialog {
        id: jsfxPackageBrowserDialog
        anchors.fill: parent
        z: 99999

        onOpenCodeRequested: (code, name) => {
            root.showJsfxDock = true
            if (jsfxDock) {
                jsfxDock.loadCustomScriptCode(code, name)
            }
        }
    }
}
