import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Item {
    id: root
    anchors.fill: parent
    visible: false
    opacity: visible ? 1.0 : 0.0

    Behavior on opacity {
        NumberAnimation { duration: 150 }
    }

    signal closed()
    signal inviteSent(string user, string message)
    signal linkCopied(string link)

    property string projectLink: "https://novastudio.org/join/nova-project-123"
    property int currentTab: 0 // 0: Paquete Local (.novapatch), 1: Enlace Web

    // Estado del parche
    property string statusMessage: ""
    property bool statusSuccess: true
    property var inspectedPatch: null
    property string selectedPatchPath: ""

    Connections {
        target: AudioEngine
        function onPatchExportFinished(success, message) {
            root.statusSuccess = success;
            root.statusMessage = message;
        }
        function onPatchApplyFinished(success, message) {
            root.statusSuccess = success;
            root.statusMessage = message;
            if (success) {
                root.inspectedPatch = null;
                root.selectedPatchPath = "";
            }
        }
    }

    function open() {
        root.visible = true;
        root.statusMessage = "";
        root.inspectedPatch = null;
    }

    function close() {
        root.visible = false;
        root.closed();
    }

    // Fondo oscuro con clic para cerrar
    MouseArea {
        anchors.fill: parent
        onClicked: root.close()

        Rectangle {
            anchors.fill: parent
            color: "#000000"
            opacity: 0.70
        }
    }

    Rectangle {
        id: modalCard
        width: Math.min(540, parent.width - 32)
        height: Math.min(mainLayout.implicitHeight + 48, parent.height - 40)
        anchors.centerIn: parent

        radius: 8
        color: "#1F2228"
        border.color: "#2D313B"
        border.width: 1

        MouseArea {
            anchors.fill: parent
            onClicked: {}
        }

        // Botón Cerrar (✕)
        Rectangle {
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 12
            width: 28
            height: 28
            radius: 14
            color: closeMouse.containsMouse ? "#2D313B" : "transparent"
            z: 10

            Text {
                anchors.centerIn: parent
                text: "✕"
                color: "#9A9EAB"
                font.pixelSize: 14
            }

            MouseArea {
                id: closeMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.close()
            }
        }

        ColumnLayout {
            id: mainLayout
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 22
            spacing: 14

            Text {
                text: "Colaboración y Compartición"
                color: "#FFFFFF"
                font.pixelSize: 18
                font.bold: true
                Layout.fillWidth: true
            }

            // PESTAÑAS (TABS)
            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                // Pestaña 0: Paquete Local (.novapatch)
                Rectangle {
                    Layout.fillWidth: true
                    height: 34
                    radius: 4
                    color: root.currentTab === 0 ? "#2B313E" : "#14161A"
                    border.color: root.currentTab === 0 ? "#00A3FF" : "#2D313B"
                    border.width: 1

                    RowLayout {
                        anchors.centerIn: parent
                        spacing: 6
                        Text { text: "📦"; font.pixelSize: 14 }
                        Text {
                            text: "Paquete Local (.novapatch)"
                            color: root.currentTab === 0 ? "#FFFFFF" : "#8A909E"
                            font.pixelSize: 12
                            font.bold: root.currentTab === 0
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            root.currentTab = 0;
                            root.statusMessage = "";
                        }
                    }
                }

                // Pestaña 1: Enlace Web
                Rectangle {
                    Layout.fillWidth: true
                    height: 34
                    radius: 4
                    color: root.currentTab === 1 ? "#2B313E" : "#14161A"
                    border.color: root.currentTab === 1 ? "#00A3FF" : "#2D313B"
                    border.width: 1

                    RowLayout {
                        anchors.centerIn: parent
                        spacing: 6
                        Text { text: "🔗"; font.pixelSize: 14 }
                        Text {
                            text: "Enlace Web / Invitación"
                            color: root.currentTab === 1 ? "#FFFFFF" : "#8A909E"
                            font.pixelSize: 12
                            font.bold: root.currentTab === 1
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            root.currentTab = 1;
                            root.statusMessage = "";
                        }
                    }
                }
            }

            // ══════════════════════════════════════════════════════════════
            // VISTA 1: PAQUETE LOCAL (.novapatch)
            // ══════════════════════════════════════════════════════════════
            ColumnLayout {
                visible: root.currentTab === 0
                Layout.fillWidth: true
                spacing: 12

                Text {
                    text: "Colabora localmente sin servidores: exporta un archivo .novapatch con tus pistas y audios comprimidos para compartirlo por WhatsApp, Telegram o Drive."
                    color: "#9A9EAB"
                    font.pixelSize: 11
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

                // SUB-SECCIÓN: EXPORTAR
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: exportCol.implicitHeight + 20
                    color: "#16181D"
                    radius: 6
                    border.color: "#282C35"
                    border.width: 1

                    ColumnLayout {
                        id: exportCol
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 12
                        spacing: 8

                        Text {
                            text: "Exportar mis cambios en un parche"
                            color: "#FFFFFF"
                            font.pixelSize: 12
                            font.bold: true
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Rectangle {
                                Layout.preferredWidth: 140
                                height: 32
                                radius: 4
                                color: "#101215"
                                border.color: "#2D313B"

                                TextInput {
                                    id: authorInput
                                    anchors.fill: parent
                                    anchors.margins: 6
                                    verticalAlignment: TextInput.AlignVCenter
                                    color: "#FFFFFF"
                                    font.pixelSize: 11
                                    text: "Landy"
                                    selectByMouse: true

                                    Text {
                                        text: "Tu nombre / autor"
                                        color: "#6B707E"
                                        font.pixelSize: 11
                                        visible: !authorInput.text && !authorInput.activeFocus
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                height: 32
                                radius: 4
                                color: "#101215"
                                border.color: "#2D313B"

                                TextInput {
                                    id: patchNotesInput
                                    anchors.fill: parent
                                    anchors.margins: 6
                                    verticalAlignment: TextInput.AlignVCenter
                                    color: "#FFFFFF"
                                    font.pixelSize: 11
                                    text: "Añadí arreglos y clips de audio"
                                    selectByMouse: true

                                    Text {
                                        text: "Descripción del cambio"
                                        color: "#6B707E"
                                        font.pixelSize: 11
                                        visible: !patchNotesInput.text && !patchNotesInput.activeFocus
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                }
                            }

                            Rectangle {
                                width: 110
                                height: 32
                                radius: 4
                                color: exportMouse.pressed ? "#0082CC" : (exportMouse.containsMouse ? "#1AB0FF" : "#00A3FF")

                                RowLayout {
                                    anchors.centerIn: parent
                                    spacing: 4
                                    Text { text: "📦"; font.pixelSize: 11 }
                                    Text {
                                        text: "Exportar..."
                                        color: "#FFFFFF"
                                        font.pixelSize: 11
                                        font.bold: true
                                    }
                                }

                                MouseArea {
                                    id: exportMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: exportFileDialog.open()
                                }
                            }
                        }
                    }
                }

                // SUB-SECCIÓN: IMPORTAR
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: importCol.implicitHeight + 20
                    color: "#16181D"
                    radius: 6
                    border.color: "#282C35"
                    border.width: 1

                    ColumnLayout {
                        id: importCol
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 12
                        spacing: 8

                        Text {
                            text: "Importar parche de colaborador (.novapatch)"
                            color: "#FFFFFF"
                            font.pixelSize: 12
                            font.bold: true
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Rectangle {
                                Layout.fillWidth: true
                                height: 32
                                radius: 4
                                color: "#101215"
                                border.color: "#2D313B"

                                Text {
                                    anchors.verticalCenter: parent.verticalCenter
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.margins: 8
                                    elide: Text.ElideMiddle
                                    text: root.selectedPatchPath ? root.selectedPatchPath : "Ningún archivo .novapatch seleccionado..."
                                    color: root.selectedPatchPath ? "#00FF88" : "#6B707E"
                                    font.pixelSize: 11
                                }
                            }

                            Rectangle {
                                width: 110
                                height: 32
                                radius: 4
                                color: pickMouse.pressed ? "#353A46" : (pickMouse.containsMouse ? "#2D313B" : "#242730")
                                border.color: "#3A3F4D"
                                border.width: 1

                                Text {
                                    anchors.centerIn: parent
                                    text: "Examinar..."
                                    color: "#FFFFFF"
                                    font.pixelSize: 11
                                    font.bold: true
                                }

                                MouseArea {
                                    id: pickMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: importFileDialog.open()
                                }
                            }
                        }

                        // TARJETA DE PREVIEW DEL PARCHE INSPECCIONADO
                        Rectangle {
                            visible: root.inspectedPatch && root.inspectedPatch.valid ? true : false
                            Layout.fillWidth: true
                            height: 58
                            radius: 4
                            color: "#1C2420"
                            border.color: "#2E523E"
                            border.width: 1

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.margins: 8
                                    spacing: 8

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 2
                                        Text {
                                            text: root.inspectedPatch ? ("Autor: " + (root.inspectedPatch.author || "Anónimo") + "  |  BPM: " + (root.inspectedPatch.bpm || 120) + "  |  Pistas: " + (root.inspectedPatch.trackCount || 0) + "  |  Clips: " + (root.inspectedPatch.clipCount || 0)) : ""
                                            color: "#00FF88"
                                            font.pixelSize: 11
                                            font.bold: true
                                        }
                                        Text {
                                            text: root.inspectedPatch ? ("Notas: " + (root.inspectedPatch.notes ? root.inspectedPatch.notes : "Sin notas adicionales")) : ""
                                            color: "#C0C6D4"
                                            font.pixelSize: 10
                                            elide: Text.ElideRight
                                        }
                                    }

                                    Rectangle {
                                        width: 120
                                        height: 30
                                        radius: 4
                                        color: applyMouse.pressed ? "#00AA55" : (applyMouse.containsMouse ? "#00CC66" : "#00FF88")

                                        Text {
                                            anchors.centerIn: parent
                                            text: "⚡ Fusionar"
                                            color: "#0D2014"
                                            font.pixelSize: 11
                                            font.bold: true
                                        }

                                        MouseArea {
                                            id: applyMouse
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                AudioEngine.applyPatch(root.selectedPatchPath, true);
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

            // ══════════════════════════════════════════════════════════════
            // VISTA 2: ENLACE WEB (EXISTENTE)
            // ══════════════════════════════════════════════════════════════
            ColumnLayout {
                visible: root.currentTab === 1
                Layout.fillWidth: true
                spacing: 12

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Rectangle {
                        Layout.fillWidth: true
                        height: 38
                        radius: 4
                        color: "#14161A"
                        border.color: userInput.activeFocus ? "#00A3FF" : "#2D313B"
                        border.width: 1

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.IBeamCursor
                            onClicked: userInput.forceActiveFocus()
                        }

                        TextInput {
                            id: userInput
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            verticalAlignment: TextInput.AlignVCenter
                            color: "#FFFFFF"
                            font.pixelSize: 13
                            selectByMouse: true
                            activeFocusOnPress: true

                            Text {
                                text: "Nombre de usuario o email"
                                color: "#6B707E"
                                font.pixelSize: 13
                                visible: !userInput.text && !userInput.activeFocus
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                    }

                    Rectangle {
                        id: sendBtn
                        width: 76
                        height: 38
                        radius: 4
                        enabled: userInput.text.trim().length > 0
                        color: !enabled ? "#2A2D36" : (sendMouse.pressed ? "#0082CC" : (sendMouse.containsMouse ? "#1AB0FF" : "#00A3FF"))

                        Text {
                            anchors.centerIn: parent
                            text: "Enviar"
                            color: sendBtn.enabled ? "#FFFFFF" : "#555A66"
                            font.pixelSize: 13
                            font.bold: true
                        }

                        MouseArea {
                            id: sendMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: sendBtn.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                            onClicked: {
                                if (sendBtn.enabled) {
                                    root.inviteSent(userInput.text, messageInput.text);
                                    userInput.text = "";
                                    messageInput.text = "";
                                    root.close();
                                }
                            }
                        }
                    }
                }

                // MENSAJE
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    Text {
                        text: "Mensaje"
                        color: "#9A9EAB"
                        font.pixelSize: 12
                        font.bold: true
                    }

                    Rectangle {
                        id: messageBox
                        Layout.fillWidth: true
                        height: 70
                        radius: 4
                        color: "#14161A"
                        border.color: messageInput.activeFocus ? "#00A3FF" : "#2D313B"
                        border.width: 1

                        TextEdit {
                            id: messageInput
                            anchors.fill: parent
                            anchors.margins: 8
                            color: "#FFFFFF"
                            font.pixelSize: 12
                            wrapMode: TextEdit.Wrap
                            selectByMouse: true

                            Text {
                                text: "Mensaje opcional para el colaborador..."
                                color: "#6B707E"
                                font.pixelSize: 12
                                visible: !messageInput.text && !messageInput.activeFocus
                            }
                        }
                    }
                }

                // LINK DE INVITACIÓN
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Text {
                            text: "Enlace de colaboración"
                            color: "#FFFFFF"
                            font.pixelSize: 12
                            font.bold: true
                        }
                        Text {
                            text: root.projectLink
                            color: "#9A9EAB"
                            font.pixelSize: 11
                            elide: Text.ElideMiddle
                        }
                    }

                    Rectangle {
                        id: copyBtn
                        width: 104
                        height: 32
                        radius: 4
                        color: copyMouse.pressed ? "#353A46" : (copyMouse.containsMouse ? "#2D313B" : "#242730")
                        border.color: "#3A3F4D"
                        border.width: 1

                        property bool copied: false

                        Text {
                            anchors.centerIn: parent
                            text: copyBtn.copied ? "¡Copiado!" : "Copiar enlace"
                            color: copyBtn.copied ? "#00FF88" : "#FFFFFF"
                            font.pixelSize: 12
                            font.bold: true
                        }

                        MouseArea {
                            id: copyMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                copyBtn.copied = true;
                                resetCopyTimer.restart();
                                root.linkCopied(root.projectLink);
                            }
                        }

                        Timer {
                            id: resetCopyTimer
                            interval: 2000
                            onTriggered: copyBtn.copied = false
                        }
                    }
                }
            }

            // MENSAJE DE ESTADO (FEEDBACK)
            Rectangle {
                visible: root.statusMessage.length > 0
                Layout.fillWidth: true
                height: 28
                radius: 4
                color: root.statusSuccess ? "#163322" : "#3B181A"
                border.color: root.statusSuccess ? "#00FF88" : "#FF4444"
                border.width: 1

                Text {
                    anchors.centerIn: parent
                    text: root.statusMessage
                    color: root.statusSuccess ? "#00FF88" : "#FF6666"
                    font.pixelSize: 11
                    font.bold: true
                }
            }
        }
    }

    // ── DIÁLOGOS DE ARCHIVO (.novapatch) ──────────────────────────────────
    FileDialog {
        id: exportFileDialog
        title: "Guardar paquete de cambios de colaboración"
        fileMode: FileDialog.SaveFile
        nameFilters: ["Parches NOVA (*.novapatch)"]
        onAccepted: {
            var raw = exportFileDialog.selectedFile.toString().replace(/^file:\/\//, "");
            if (!raw.endsWith(".novapatch")) raw += ".novapatch";
            AudioEngine.exportPatch(raw, patchNotesInput.text, authorInput.text);
        }
    }

    FileDialog {
        id: importFileDialog
        title: "Seleccionar paquete de cambios de colaborador"
        fileMode: FileDialog.OpenFile
        nameFilters: ["Parches NOVA (*.novapatch)"]
        onAccepted: {
            var raw = importFileDialog.selectedFile.toString().replace(/^file:\/\//, "");
            root.selectedPatchPath = raw;
            root.inspectedPatch = AudioEngine.inspectPatch(raw);
        }
    }
}
