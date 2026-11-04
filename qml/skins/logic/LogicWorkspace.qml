import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "topbar"
import "tcp"
import "timeline"
import "dialogs"
import "rack"
import "dock"

Rectangle {
    id: root
    anchors.fill: parent
    color: "#1E1F24"

    property bool showFxRack: false
    property bool showJsfxDock: false
    property int activeFxTrackIndex: -1
    property string activeFxTrackName: ""

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ZONA 1: TOP BAR (70px)
        LogicTopBar {
            id: topBar
            Layout.fillWidth: true
            Layout.preferredHeight: 70

            onCollaborationRequested: inviteCollaboratorsDialog.open()
            onJsfxEditorRequested: {
                var currentTrack = (root.activeFxTrackIndex >= 0) ? root.activeFxTrackIndex : 0
                root.showJsfxDock = true
                jsfxDock.openNewScript(currentTrack)
            }
            onHomeRequested: {
                if (root.parent && root.parent.parent && root.parent.parent.goBack) {
                    root.parent.parent.goBack()
                }
            }
        }

        // ZONA MEDIA: TCP + ARRANGER
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // ZONA 2: TCP PANEL (260px)
            LogicTcpPanel {
                id: tcpPanel
                Layout.preferredWidth: 260
                Layout.fillHeight: true
                activeFxTrackIndex: root.activeFxTrackIndex
                isFxRackOpen: root.showFxRack
                onFxTrackRequested: (trackIdx, trackNm) => {
                    if (root.showFxRack && root.activeFxTrackIndex === trackIdx) {
                        root.showFxRack = false
                    } else {
                        root.activeFxTrackIndex = trackIdx
                        root.activeFxTrackName = trackNm
                        root.showFxRack = true
                    }
                }
            }

            // ZONA 3: TIMELINE / ARRANGER + DOCK DE JSFX ACOPLADO
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0

                LogicTimelinePanel {
                    id: timelinePanel
                    Layout.fillWidth: true
                    Layout.fillHeight: !jsfxDock.isMaximized
                    visible: !jsfxDock.isMaximized

                    // 🔄 SINCRONIZACIÓN DE SCROLL VERTICAL PERFECTA
                    Binding {
                        target: tcpPanel
                        property: "scrollY"
                        value: timelinePanel.scrollY
                    }
                    Binding {
                        target: timelinePanel
                        property: "scrollY"
                        value: tcpPanel.scrollY
                    }
                }

                // DOCK INTEGRADO DEL EDITOR JSFX (IDÉNTICO A LA CAPTURA)
                LogicJsfxDock {
                    id: jsfxDock
                    Layout.fillWidth: true
                    Layout.fillHeight: root.showJsfxDock && jsfxDock.isMaximized
                    Layout.preferredHeight: root.showJsfxDock ? (jsfxDock.isMaximized ? parent.height : Math.min(root.height * 0.72, 450)) : 0
                    visible: root.showJsfxDock
                    clip: true

                    onCloseRequested: {
                        root.showJsfxDock = false
                    }
                    onScriptApplied: {
                        if (fxRackPanel) fxRackPanel.refreshPlugins()
                    }

                    Behavior on Layout.preferredHeight {
                        NumberAnimation { duration: 180; easing.type: Easing.InOutQuad }
                    }
                }
            }
        }

        // ZONA DE EFECTOS: RACK INSPECTOR DE LA PISTA SELECCIONADA
        LogicFxRackPanel {
            id: fxRackPanel
            Layout.fillWidth: true
            Layout.preferredHeight: root.showFxRack ? (root.height < 500 ? 175 : 210) : 0
            visible: root.showFxRack
            clip: true
            trackIndex: root.activeFxTrackIndex
            trackName: root.activeFxTrackName
            onCloseRequested: {
                root.showFxRack = false
            }
            onEditJsfxRequested: (trkIdx, plgIdx, plgNm) => {
                root.showJsfxDock = true
                jsfxDock.openForPlugin(trkIdx, plgIdx, plgNm)
            }

            Behavior on Layout.preferredHeight {
                NumberAnimation { duration: 160; easing.type: Easing.InOutQuad }
            }
        }

        // ZONA 4: BOTTOM BAR
        Rectangle {
            Layout.fillWidth: true
            height: 28
            color: "#2D3139"
            border.color: "#1A1C22"
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12

                Text {
                    text: "Sample Rate: 44.1 kHz / 24-bit"
                    color: "#8F94A0"
                    font.pixelSize: 11
                }

                Item { Layout.fillWidth: true }

                Text {
                    text: "BPM: " + (typeof AudioEngine !== "undefined" ? AudioEngine.bpm.toFixed(2) : "120.00") + "  |  Snap: 1/16"
                    color: "#8F94A0"
                    font.pixelSize: 11
                }
            }
        }
    }

    // DIÁLOGO MODAL DE COLABORADORES
    InviteCollaboratorsDialog {
        id: inviteCollaboratorsDialog
        anchors.fill: parent
        z: 9999

        onInviteSent: (user, message) => {
            console.log(" Invitación enviada a:", user, "| Mensaje:", message)
        }

        onLinkCopied: (link) => {
            console.log(" Enlace copiado al portapapeles:", link)
        }
    }
}