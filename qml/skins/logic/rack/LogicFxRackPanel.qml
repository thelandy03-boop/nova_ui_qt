import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Rectangle {
    id: root

    property int trackIndex: -1
    property string trackName: "Track"
    property bool masterBypass: false
    property var pluginsList: []
    property bool showAddDialog: false

    signal closeRequested()
    signal editJsfxRequested(int trackIndex, int pluginIndex, string pluginName)

    color: "#121319"
    border.color: "#252834"
    border.width: 1

    function refreshPlugins() {
        if (typeof AudioEngine !== "undefined" && AudioEngine.tracks && root.trackIndex >= 0) {
            root.pluginsList = AudioEngine.tracks.trackPlugins(root.trackIndex)
            root.masterBypass = AudioEngine.tracks.isTrackPluginsBypassed(root.trackIndex)
        } else {
            root.pluginsList = []
            root.masterBypass = false
        }
    }

    onTrackIndexChanged: refreshPlugins()
    Component.onCompleted: refreshPlugins()

    Connections {
        target: (typeof AudioEngine !== "undefined" && AudioEngine.tracks) ? AudioEngine.tracks : null
        function onTrackPluginsChanged(row) {
            if (row === root.trackIndex) {
                root.refreshPlugins()
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ==========================================
        // 1. BARRA SUPERIOR DEL RACK (INSPECTOR)
        // ==========================================
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 38
            color: "#181A22"
            border.color: "#232634"
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 10

                // Icono e indicador
                Text {
                    text: "🎛️"
                    font.pixelSize: 14
                }

                Text {
                    text: "AUDIO FX RACK"
                    color: "#94A3B8"
                    font.pixelSize: 11
                    font.bold: true
                    font.letterSpacing: 0.5
                }

                // Chip de la pista actual
                Rectangle {
                    Layout.preferredHeight: 22
                    Layout.preferredWidth: trackChipText.implicitWidth + 16
                    radius: 4
                    color: "#1E293B"
                    border.color: "#3B82F6"
                    border.width: 1

                    Text {
                        id: trackChipText
                        anchors.centerIn: parent
                        text: (root.trackIndex >= 0 ? "Pista " + (root.trackIndex + 1) + " · " : "") + root.trackName
                        color: "#60A5FA"
                        font.pixelSize: 11
                        font.bold: true
                    }
                }

                // Contador de plugins
                Rectangle {
                    Layout.preferredHeight: 20
                    Layout.preferredWidth: countText.implicitWidth + 12
                    radius: 10
                    color: "#20232C"

                    Text {
                        id: countText
                        anchors.centerIn: parent
                        text: root.pluginsList.length + (root.pluginsList.length === 1 ? " plug-in" : " plug-ins")
                        color: root.pluginsList.length > 0 ? "#A5B4FC" : "#64748B"
                        font.pixelSize: 9
                        font.bold: true
                    }
                }

                Item { Layout.fillWidth: true } // spacer

                // Botón rápido para insertar plug-in
                Rectangle {
                    Layout.preferredWidth: addBtnText.implicitWidth + 16
                    Layout.preferredHeight: 24
                    radius: 4
                    color: addBtnArea.containsMouse ? "#3B82F6" : "#2563EB"

                    Row {
                        anchors.centerIn: parent
                        spacing: 4
                        Text { text: "+"; color: "#FFFFFF"; font.pixelSize: 12; font.bold: true }
                        Text { id: addBtnText; text: "Añadir Plug-in"; color: "#FFFFFF"; font.pixelSize: 10; font.bold: true }
                    }

                    MouseArea {
                        id: addBtnArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (typeof AudioEngine !== "undefined" && AudioEngine.hapticFeedback) {
                                AudioEngine.hapticFeedback()
                            }
                            root.showAddDialog = true
                        }
                    }
                }

                // Botón Master Bypass
                Rectangle {
                    Layout.preferredWidth: bypassText.implicitWidth + 14
                    Layout.preferredHeight: 24
                    radius: 4
                    color: root.masterBypass ? "#DC2626" : "#242733"
                    border.color: root.masterBypass ? "#EF4444" : "#363A4B"
                    border.width: 1

                    Text {
                        id: bypassText
                        anchors.centerIn: parent
                        text: root.masterBypass ? "⚠️ BYPASS ACTIVO" : "BYPASS TODOS"
                        color: root.masterBypass ? "#FFFFFF" : "#94A3B8"
                        font.pixelSize: 10
                        font.bold: true
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (typeof AudioEngine !== "undefined" && AudioEngine.hapticFeedback) {
                                AudioEngine.hapticFeedback()
                            }
                            if (typeof AudioEngine !== "undefined" && AudioEngine.tracks && root.trackIndex >= 0) {
                                AudioEngine.tracks.setTrackPluginsBypassed(root.trackIndex, !root.masterBypass)
                                root.masterBypass = !root.masterBypass
                            }
                        }
                    }
                }

                // Botón cerrar rack
                Rectangle {
                    Layout.preferredWidth: 24
                    Layout.preferredHeight: 24
                    radius: 4
                    color: closeArea.containsMouse ? "#EF4444" : "#242733"
                    border.color: closeArea.containsMouse ? "#DC2626" : "#363A4B"
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: "✕"
                        color: "#E2E8F0"
                        font.pixelSize: 12
                        font.bold: true
                    }

                    MouseArea {
                        id: closeArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (typeof AudioEngine !== "undefined" && AudioEngine.hapticFeedback) {
                                AudioEngine.hapticFeedback()
                            }
                            root.closeRequested()
                        }
                    }
                }
            }
        }

        // ==========================================
        // 2. CONTENIDO: ESTADO VACÍO O LISTA REAL
        // ==========================================
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            // --- ESTADO VACÍO: NO HAY PLUGINS EN LA PISTA ---
            Item {
                anchors.fill: parent
                visible: root.pluginsList.length === 0

                Column {
                    anchors.centerIn: parent
                    spacing: 8

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "🔌"
                        font.pixelSize: 28
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "Esta pista no tiene plug-ins insertados todavía"
                        color: "#E2E8F0"
                        font.pixelSize: 13
                        font.bold: true
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "Añade plug-ins nativos de Ardour (ACE EQ, Compresor, Delay, Reverb) para procesar el audio en tiempo real."
                        color: "#64748B"
                        font.pixelSize: 11
                    }

                    Item { width: 1; height: 4 }

                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: emptyBtnRow.implicitWidth + 24
                        height: 32
                        radius: 6
                        color: emptyBtnArea.containsMouse ? "#2563EB" : "#1D4ED8"

                        Row {
                            id: emptyBtnRow
                            anchors.centerIn: parent
                            spacing: 6
                            Text { text: "➕"; color: "#FFFFFF"; font.pixelSize: 11 }
                            Text { text: "Insertar Plug-in de Ardour"; color: "#FFFFFF"; font.pixelSize: 11; font.bold: true }
                        }

                        MouseArea {
                            id: emptyBtnArea
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (typeof AudioEngine !== "undefined" && AudioEngine.hapticFeedback) {
                                    AudioEngine.hapticFeedback()
                                }
                                root.showAddDialog = true
                            }
                        }
                    }
                }
            }

            // --- LISTA DE PLUGINS REALES INSERTADOS ---
            Flickable {
                anchors.fill: parent
                visible: root.pluginsList.length > 0
                contentWidth: effectsRow.implicitWidth + 24
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                flickableDirection: Flickable.HorizontalFlick

                Row {
                    id: effectsRow
                    x: 12
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 12

                    Repeater {
                        model: root.pluginsList

                        delegate: LogicFxCard {
                            id: cardDelegate
                            title: modelData.name || "Plug-in"
                            category: modelData.category || "EFECTO"
                            accentColor: {
                                if (category === "EQ") return "#3B82F6"
                                if (category === "DINÁMICA") return "#F59E0B"
                                if (category === "ESPACIO") return "#A855F7"
                                if (category === "DELAY") return "#10B981"
                                return "#38BDF8"
                            }
                            enabledFx: !modelData.bypassed && !root.masterBypass
                            isJsfx: (modelData.category === "JSFX") || (modelData.uniqueId && modelData.uniqueId.indexOf("jsfx:") === 0)

                            onEditCodeRequested: {
                                root.editJsfxRequested(root.trackIndex, modelData.index, modelData.name)
                            }

                            onToggled: (isEnabled) => {
                                if (typeof AudioEngine !== "undefined" && AudioEngine.tracks && root.trackIndex >= 0) {
                                    AudioEngine.tracks.setPluginBypassed(root.trackIndex, modelData.index, !isEnabled)
                                }
                            }

                            onRemoveRequested: {
                                if (typeof AudioEngine !== "undefined" && AudioEngine.tracks && root.trackIndex >= 0) {
                                    AudioEngine.tracks.removePluginFromTrack(root.trackIndex, modelData.index)
                                }
                            }

                            // Sliders dinámicos según los parámetros reales del plug-in
                            Row {
                                anchors.centerIn: parent
                                spacing: 10
                                visible: modelData.parameters && modelData.parameters.length > 0

                                Repeater {
                                    model: modelData.parameters

                                    delegate: LogicFxSlider {
                                        property var p: modelData
                                        label: p.name || ("P" + (index + 1))
                                        value: (p.max > p.min) ? Math.max(0.0, Math.min(1.0, (p.value - p.min) / (p.max - p.min))) : 0.5
                                        displayValue: p.value.toFixed(1) + (p.unit ? " " + p.unit : "")
                                        activeColor: cardDelegate.accentColor

                                        onValueChanged: {
                                            if (typeof AudioEngine !== "undefined" && AudioEngine.tracks && root.trackIndex >= 0) {
                                                var realVal = p.min + value * (p.max - p.min)
                                                AudioEngine.tracks.setPluginParameter(root.trackIndex, cardDelegate.modelData.index, p.index, realVal)
                                            }
                                        }
                                    }
                                }
                            }

                            // Mensaje si el plugin no expone sliders
                            Text {
                                anchors.centerIn: parent
                                visible: !modelData.parameters || modelData.parameters.length === 0
                                text: "Plug-in activo y procesando señal"
                                color: "#64748B"
                                font.pixelSize: 10
                                font.italic: true
                            }
                        }
                    }

                    // Ranura final para añadir más plug-ins
                    Rectangle {
                        width: 120
                        height: 165
                        radius: 10
                        color: addMoreArea.containsMouse ? "#1E222D" : "#161820"
                        border.color: addMoreArea.containsMouse ? "#3B82F6" : "#262A37"
                        border.width: 1

                        Column {
                            anchors.centerIn: parent
                            spacing: 8

                            Rectangle {
                                anchors.horizontalCenter: parent.horizontalCenter
                                width: 36
                                height: 36
                                radius: 18
                                color: addMoreArea.containsMouse ? "#2563EB" : "#1E293B"

                                Text {
                                    anchors.centerIn: parent
                                    text: "+"
                                    color: "#FFFFFF"
                                    font.pixelSize: 18
                                    font.bold: true
                                }
                            }

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: "Añadir\nPlug-in"
                                color: addMoreArea.containsMouse ? "#F1F5F9" : "#94A3B8"
                                font.pixelSize: 10
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                            }
                        }

                        MouseArea {
                            id: addMoreArea
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (typeof AudioEngine !== "undefined" && AudioEngine.hapticFeedback) {
                                    AudioEngine.hapticFeedback()
                                }
                                root.showAddDialog = true
                            }
                        }
                    }
                }
            }
        }
    }

    // ==========================================
    // 3. DIÁLOGO MODAL: SELECTOR DE PLUG-IN REAL
    // ==========================================
    Rectangle {
        id: addDialogOverlay
        anchors.fill: parent
        color: "#B3000000"
        visible: root.showAddDialog
        z: 999

        MouseArea {
            anchors.fill: parent
            onClicked: root.showAddDialog = false
        }

        Rectangle {
            width: Math.min(parent.width - 40, 520)
            height: Math.min(parent.height - 20, 210)
            anchors.centerIn: parent
            radius: 10
            color: "#181A22"
            border.color: "#333846"
            border.width: 1

            MouseArea {
                // Previene cerrar al hacer click dentro del diálogo
                anchors.fill: parent
                onClicked: {}
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 10

                // Encabezado del diálogo
                RowLayout {
                    Layout.fillWidth: true

                    Text {
                        text: "🔌 Insertar Plug-in de Ardour"
                        color: "#FFFFFF"
                        font.pixelSize: 13
                        font.bold: true
                    }

                    Item { Layout.fillWidth: true }

                    Rectangle {
                        width: 22
                        height: 22
                        radius: 4
                        color: closeDlgArea.containsMouse ? "#EF4444" : "#242733"

                        Text {
                            anchors.centerIn: parent
                            text: "✕"
                            color: "#FFFFFF"
                            font.pixelSize: 11
                            font.bold: true
                        }

                        MouseArea {
                            id: closeDlgArea
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.showAddDialog = false
                        }
                    }
                }

                Text {
                    text: "Selecciona un efecto nativo para conectarlo a la pista en tiempo real:"
                    color: "#94A3B8"
                    font.pixelSize: 11
                }


                // Lista de plug-ins disponibles
                ListView {
                    id: availableView
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 6
                    model: (typeof AudioEngine !== "undefined" && AudioEngine.tracks) ? AudioEngine.tracks.availablePlugins() : []

                    delegate: Rectangle {
                        width: availableView.width
                        height: 38
                        radius: 6
                        color: plugItemArea.containsMouse ? "#232734" : "#1B1E28"
                        border.color: plugItemArea.containsMouse ? "#3B82F6" : "#282C3A"
                        border.width: 1

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            spacing: 8

                            // Badge de categoría
                            Rectangle {
                                Layout.preferredHeight: 18
                                Layout.preferredWidth: catPlugText.implicitWidth + 8
                                radius: 4
                                color: {
                                    if (modelData.category === "EQ") return "#1E3A8A"
                                    if (modelData.category === "DINÁMICA") return "#78350F"
                                    if (modelData.category === "ESPACIO") return "#581C87"
                                    if (modelData.category === "DELAY") return "#064E3B"
                                    return "#1E293B"
                                }

                                Text {
                                    id: catPlugText
                                    anchors.centerIn: parent
                                    text: modelData.category || "LV2"
                                    color: "#FFFFFF"
                                    font.pixelSize: 8
                                    font.bold: true
                                }
                            }

                            // Nombre y descripción
                            Column {
                                Layout.fillWidth: true
                                spacing: 2

                                Text {
                                    text: modelData.name
                                    color: "#F8FAFC"
                                    font.pixelSize: 11
                                    font.bold: true
                                }

                                Text {
                                    text: modelData.description || ""
                                    color: "#64748B"
                                    font.pixelSize: 9
                                    elide: Text.ElideRight
                                    width: 320
                                }
                            }

                            // Botón Insertar
                            Rectangle {
                                Layout.preferredWidth: 62
                                Layout.preferredHeight: 22
                                radius: 4
                                color: "#2563EB"

                                Text {
                                    anchors.centerIn: parent
                                    text: "+ Insertar"
                                    color: "#FFFFFF"
                                    font.pixelSize: 9
                                    font.bold: true
                                }
                            }
                        }

                        MouseArea {
                            id: plugItemArea
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (typeof AudioEngine !== "undefined" && AudioEngine.hapticFeedback) {
                                    AudioEngine.hapticFeedback()
                                }
                                if (typeof AudioEngine !== "undefined" && AudioEngine.tracks && root.trackIndex >= 0) {
                                    AudioEngine.tracks.addPluginToTrack(root.trackIndex, modelData.id)
                                }
                                root.showAddDialog = false
                            }
                        }
                    }
                }
            }
        }
    }
}

