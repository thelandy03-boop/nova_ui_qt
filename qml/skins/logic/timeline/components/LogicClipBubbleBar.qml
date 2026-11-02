import QtQuick

Item {
    id: root

    property int clipIndex: -1
    property real startBeat: 0.0
    property real snapGridBeats: 1.0

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
        anchors.bottom: bubbleRow.top
        anchors.bottomMargin: 6
        width: 168
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
        }
    }
}
