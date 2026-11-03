import QtQuick

Item {
    id: root

    property int clipIndex: -1
    property real startBeat: 0.0
    property real snapGridBeats: 1.0
    property bool showAbove: true

    width: bubbleRow.width
    height: 36

    // ── FILA HORIZONTAL DE BURBUJAS CIRCULARES (ESTILO FL STUDIO / LOGIC MOBILE) ──
    Row {
        id: bubbleRow
        spacing: 8
        anchors.verticalCenter: parent.verticalCenter

        // 📋 Botón Circular: Copy (Duplicar)
        Rectangle {
            width: 34
            height: 34
            radius: 17
            color: copyMouse.pressed ? "#BAC7D5" : (copyMouse.containsMouse ? "#E2E8F0" : "#D5DBE1")
            border.color: "#8A9BA8"
            border.width: 1

            Text {
                anchors.centerIn: parent
                text: "Copy"
                font.pixelSize: 10
                font.bold: true
                color: "#1E293B"
            }

            MouseArea {
                id: copyMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    if (root.clipIndex >= 0) {
                        AudioEngine.regions.duplicateRegion(root.clipIndex)
                    }
                }
            }
        }

        // 🗑️ Botón Circular: Delete (Eliminar)
        Rectangle {
            width: 34
            height: 34
            radius: 17
            color: deleteMouse.pressed ? "#BAC7D5" : (deleteMouse.containsMouse ? "#E2E8F0" : "#D5DBE1")
            border.color: "#8A9BA8"
            border.width: 1

            Text {
                anchors.centerIn: parent
                text: "Delete"
                font.pixelSize: 9
                font.bold: true
                color: "#1E293B"
            }

            MouseArea {
                id: deleteMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    if (root.clipIndex >= 0) {
                        AudioEngine.regions.removeRegion(root.clipIndex)
                    }
                }
            }
        }

        // 🧲 Botón Circular: Snap (Ajustar a Rejilla)
        Rectangle {
            width: 34
            height: 34
            radius: 17
            color: snapMouse.pressed ? "#BAC7D5" : (snapMouse.containsMouse ? "#E2E8F0" : "#D5DBE1")
            border.color: "#8A9BA8"
            border.width: 1

            Text {
                anchors.centerIn: parent
                text: "Snap"
                font.pixelSize: 10
                font.bold: true
                color: "#1E293B"
            }

            MouseArea {
                id: snapMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    if (root.clipIndex >= 0) {
                        var grid = Math.max(0.01, root.snapGridBeats)
                        var snappedBeat = Math.max(0, Math.round(root.startBeat / grid) * grid)
                        AudioEngine.regions.moveRegion(root.clipIndex, snappedBeat)
                    }
                }
            }
        }

        // ✂️ Botón Circular: Edit (Dividir / Editar en Cursor)
        Rectangle {
            width: 34
            height: 34
            radius: 17
            color: editMouse.pressed ? "#BAC7D5" : (editMouse.containsMouse ? "#E2E8F0" : "#D5DBE1")
            border.color: "#8A9BA8"
            border.width: 1

            Text {
                anchors.centerIn: parent
                text: "Edit"
                font.pixelSize: 10
                font.bold: true
                color: "#1E293B"
            }

            MouseArea {
                id: editMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    if (root.clipIndex >= 0) {
                        AudioEngine.splitAtPlayhead(root.clipIndex)
                    }
                }
            }
        }

        // ⋯ Botón Circular: More... (Menú de Opciones Adicionales)
        Rectangle {
            id: moreBtn
            width: 34
            height: 34
            radius: 17
            color: moreMenuPopup.visible || moreMouse.pressed ? "#BAC7D5" : (moreMouse.containsMouse ? "#E2E8F0" : "#D5DBE1")
            border.color: "#8A9BA8"
            border.width: 1

            Text {
                anchors.centerIn: parent
                text: "More..."
                font.pixelSize: 8
                font.bold: true
                color: "#1E293B"
            }

            MouseArea {
                id: moreMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    moreMenuPopup.visible = !moreMenuPopup.visible
                }
            }
        }
    }

    // ── SUBMENÚ FLOTANTE PARA "More..." ──
    Rectangle {
        id: moreMenuPopup
        visible: false
        z: 300
        anchors.right: bubbleRow.right
        anchors.bottom: root.showAbove ? bubbleRow.top : undefined
        anchors.top: !root.showAbove ? bubbleRow.bottom : undefined
        anchors.bottomMargin: root.showAbove ? 6 : 0
        anchors.topMargin: !root.showAbove ? 6 : 0
        width: 190
        height: moreColumn.implicitHeight + 12
        radius: 8
        color: "#1E222A"
        border.color: "#383F4D"
        border.width: 1

        Column {
            id: moreColumn
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.topMargin: 6
            spacing: 2

            // Opción: Normalizar (Auto-Gain)
            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: 4
                height: 26
                radius: 4
                color: autoGainOptMouse.containsMouse ? "#333A48" : "transparent"

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    text: "⚡ Auto-Gain (Normalizar)"
                    color: "#FFFFFF"
                    font.pixelSize: 10
                }

                MouseArea {
                    id: autoGainOptMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (root.clipIndex >= 0) {
                            AudioEngine.regions.normalizeClip(root.clipIndex)
                        }
                        moreMenuPopup.visible = false
                    }
                }
            }

            // Opción: Dividir en Playhead
            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: 4
                height: 26
                radius: 4
                color: splitOptMouse.containsMouse ? "#333A48" : "transparent"

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    text: "✂ Dividir en cursor"
                    color: "#FFFFFF"
                    font.pixelSize: 10
                }

                MouseArea {
                    id: splitOptMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (root.clipIndex >= 0) {
                            AudioEngine.splitAtPlayhead(root.clipIndex)
                        }
                        moreMenuPopup.visible = false
                    }
                }
            }

            // Opción: Invertir Audio (Reverse)
            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: 4
                height: 26
                radius: 4
                color: revOptMouse.containsMouse ? "#333A48" : "transparent"

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    text: "🔄 Invertir Audio (Reverse)"
                    color: "#FFFFFF"
                    font.pixelSize: 10
                }

                MouseArea {
                    id: revOptMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (root.clipIndex >= 0) {
                            AudioEngine.regions.reverseRegion(root.clipIndex)
                        }
                        moreMenuPopup.visible = false
                    }
                }
            }

            // Opción: Silenciar / Activar Clip (Mute)
            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: 4
                height: 26
                radius: 4
                color: muteOptMouse.containsMouse ? "#333A48" : "transparent"

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    text: "🔇 Silenciar / Activar (Mute)"
                    color: "#FFFFFF"
                    font.pixelSize: 10
                }

                MouseArea {
                    id: muteOptMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (root.clipIndex >= 0) {
                            AudioEngine.regions.toggleMuteRegion(root.clipIndex)
                        }
                        moreMenuPopup.visible = false
                    }
                }
            }

            // Opción: Resetear Fades
            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: 4
                height: 26
                radius: 4
                color: resetFadesOptMouse.containsMouse ? "#333A48" : "transparent"

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    text: "↺ Resetear Fundidos"
                    color: "#FFFFFF"
                    font.pixelSize: 10
                }

                MouseArea {
                    id: resetFadesOptMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (root.clipIndex >= 0) {
                            AudioEngine.regions.setFadeInBeats(root.clipIndex, 0.0)
                            AudioEngine.regions.setFadeOutBeats(root.clipIndex, 0.0)
                        }
                        moreMenuPopup.visible = false
                    }
                }
            }

            // Separador
            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: 6
                height: 1
                color: "#2E3644"
            }

            // Fila de Tono / Pitch Semitonos
            Item {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: 4
                height: 28

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    text: "🎵 Tono:"
                    color: "#A0AEC0"
                    font.pixelSize: 10
                }

                Row {
                    anchors.right: parent.right
                    anchors.rightMargin: 4
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 4

                    // -1 semitono
                    Rectangle {
                        width: 24; height: 20; radius: 3
                        color: pitchDownMouse.pressed ? "#4A5568" : (pitchDownMouse.containsMouse ? "#374151" : "#2D3748")
                        Text { anchors.centerIn: parent; text: "-1"; color: "#FFFFFF"; font.pixelSize: 9; font.bold: true }
                        MouseArea {
                            id: pitchDownMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (root.clipIndex >= 0) {
                                    var currentPitch = 0
                                    var idx = AudioEngine.regions.index(root.clipIndex, 0)
                                    var val = AudioEngine.regions.data(idx, 269) // PitchSemitonesRole
                                    if (typeof val === "number") currentPitch = val
                                    AudioEngine.regions.setPitchSemitones(root.clipIndex, currentPitch - 1)
                                }
                            }
                        }
                    }

                    // Reset 0
                    Rectangle {
                        width: 28; height: 20; radius: 3
                        color: pitchResetMouse.pressed ? "#4A5568" : (pitchResetMouse.containsMouse ? "#374151" : "#2D3748")
                        Text { anchors.centerIn: parent; text: "0 st"; color: "#FFCC00"; font.pixelSize: 8; font.bold: true }
                        MouseArea {
                            id: pitchResetMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (root.clipIndex >= 0) {
                                    AudioEngine.regions.setPitchSemitones(root.clipIndex, 0)
                                }
                            }
                        }
                    }

                    // +1 semitono
                    Rectangle {
                        width: 24; height: 20; radius: 3
                        color: pitchUpMouse.pressed ? "#4A5568" : (pitchUpMouse.containsMouse ? "#374151" : "#2D3748")
                        Text { anchors.centerIn: parent; text: "+1"; color: "#FFFFFF"; font.pixelSize: 9; font.bold: true }
                        MouseArea {
                            id: pitchUpMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (root.clipIndex >= 0) {
                                    var currentPitch = 0
                                    var idx = AudioEngine.regions.index(root.clipIndex, 0)
                                    var val = AudioEngine.regions.data(idx, 269) // PitchSemitonesRole
                                    if (typeof val === "number") currentPitch = val
                                    AudioEngine.regions.setPitchSemitones(root.clipIndex, currentPitch + 1)
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
