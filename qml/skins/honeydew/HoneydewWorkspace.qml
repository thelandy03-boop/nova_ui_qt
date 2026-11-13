import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "topbar"
import "playlist"
import "rack"
import "transport"
import "keyboard"
import "../logic/dock"
import "../../dialogs"

Rectangle {
    id: root
    anchors.fill: parent
    color: HoneydewTheme.background

    property bool showFxRack: false
    property bool showJsfxDock: false
    property bool showKeyboard: true
    property int activeTrackIndex: 0
    property string activeTrackName: "Audio 1"

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ══════════════════════════════════════════════
        // ZONA 1: PLAYLIST + TIMELINE (ÁREA PRINCIPAL)
        // ══════════════════════════════════════════════
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            RowLayout {
                anchors.fill: parent
                spacing: 0

                // Panel Central: Playlist & Timeline
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

            // ══════════════════════════════════════════════
            // BOTONES FLOTANTES SUPERIORES DE HONEYDEW (CORNER BUTTONS)
            // (En Rust main_screen.rs: CORNER_W = 2.1 * U, logo, rack tab, detach)
            // ══════════════════════════════════════════════
            Row {
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.topMargin: 8
                anchors.rightMargin: 12
                spacing: 8
                z: 100

                // Botón Logo / Home
                Rectangle {
                    width: 36
                    height: 36
                    radius: 18
                    color: logoMouse.pressed ? HoneydewTheme.background : (logoMouse.containsMouse ? "#3A4248" : HoneydewTheme.moduleFill)
                    border.color: HoneydewTheme.menuFill
                    border.width: 1

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
                        onClicked: {
                            if (root.parent && root.parent.parent && root.parent.parent.goBack) {
                                root.parent.parent.goBack()
                            }
                        }
                    }
                }

                // Botón Pestaña Rack (RackTab ≡)
                Rectangle {
                    width: 36
                    height: 36
                    radius: 18
                    color: root.showFxRack ? HoneydewTheme.accent : (rackTabMouse.containsMouse ? "#3A4248" : HoneydewTheme.moduleFill)
                    border.color: HoneydewTheme.menuFill
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: "🎚️"
                        font.pixelSize: 14
                    }

                    MouseArea {
                        id: rackTabMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.showFxRack = !root.showFxRack
                    }
                }

                // Botón Editor JSFX (⚡)
                Rectangle {
                    width: 36
                    height: 36
                    radius: 18
                    color: jsfxQuickMouse.pressed ? HoneydewTheme.background : (jsfxQuickMouse.containsMouse ? "#3A4248" : HoneydewTheme.moduleFill)
                    border.color: HoneydewTheme.menuFill
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: "⚡"
                        font.pixelSize: 14
                    }

                    MouseArea {
                        id: jsfxQuickMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            root.showJsfxDock = true
                            if (jsfxDock) {
                                jsfxDock.openNewScript(root.activeTrackIndex)
                            }
                        }
                    }
                }
            }
        }

        // ══════════════════════════════════════════════
        // ZONA 2: BARRA DE TRANSPORTE CANÓNICA (TRANSPORT_H = 1.75 U)
        // ══════════════════════════════════════════════
        HoneydewTransportBar {
            id: transportBar
            Layout.fillWidth: true
            isKeyboardOpen: root.showKeyboard
            isFxRackOpen: root.showFxRack

            onTogglePlay: {
                if (typeof AudioEngine !== "undefined") {
                    if (AudioEngine.isPlaying) AudioEngine.stop()
                    else AudioEngine.play()
                }
            }
            onToggleRecord: {
                if (typeof AudioEngine !== "undefined") {
                    AudioEngine.toggleRecord()
                }
            }
            onRewindRequested: {
                if (typeof AudioEngine !== "undefined") {
                    AudioEngine.locateBeat(0.0)
                }
            }
            onToggleLoop: {
                if (typeof AudioEngine !== "undefined") {
                    AudioEngine.toggleLoop()
                }
            }
            onToggleKeyboard: {
                root.showKeyboard = !root.showKeyboard
            }
            onToggleFxRack: {
                root.showFxRack = !root.showFxRack
            }
        }

        // ══════════════════════════════════════════════
        // ZONA 3: PANEL INFERIOR (TECLADO PIANO / DRUM PADS)
        // ══════════════════════════════════════════════
        HoneydewKeyboardPanel {
            id: keyboardPanel
            Layout.fillWidth: true
            Layout.preferredHeight: root.showKeyboard ? 160 : 0
            visible: root.showKeyboard || height > 0
            clip: true
            channelColor: HoneydewTheme.channelColor(root.activeTrackIndex)

            Behavior on Layout.preferredHeight {
                NumberAnimation { duration: 180; easing.type: Easing.InOutQuad }
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
