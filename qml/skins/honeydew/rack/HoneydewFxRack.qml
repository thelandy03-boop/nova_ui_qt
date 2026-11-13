import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import "../"
import "../components"

Rectangle {
    id: root

    property int trackIndex: 0
    property string trackName: "Audio 1"
    property color trackColor: HoneydewTheme.getTrackColor(trackIndex)
    property var pluginsList: []

    signal closeRequested()
    signal addPluginRequested(int trkIdx)
    signal editJsfxRequested(int trkIdx, int plgIdx, string plgNm)
    signal reapackRequested()

    function refreshPlugins() {
        if (typeof AudioEngine !== "undefined" && AudioEngine.tracks && root.trackIndex >= 0) {
            root.pluginsList = AudioEngine.tracks.trackPlugins(root.trackIndex)
        } else {
            root.pluginsList = []
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

    width: 320
    color: HoneydewTheme.bgSurface
    border.color: HoneydewTheme.borderSubtle
    border.width: 1

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ══════════════════════════════════════════════
        // CABECERA DEL RACK
        // ══════════════════════════════════════════════
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 52
            color: HoneydewTheme.bgSurfaceLight
            border.color: HoneydewTheme.borderSubtle
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 12
                spacing: 8

                // Píldora de color
                Rectangle {
                    width: 6
                    height: 24
                    radius: 3
                    color: root.trackColor
                }

                Column {
                    Layout.fillWidth: true
                    spacing: 1

                    Text {
                        text: "TRACK CHANNEL & FX"
                        color: HoneydewTheme.flOrange
                        font.pixelSize: 9
                        font.bold: true
                    }

                    Text {
                        text: root.trackName
                        color: HoneydewTheme.textPrimary
                        font.pixelSize: 13
                        font.bold: true
                        elide: Text.ElideRight
                    }
                }

                // Botón Cerrar (✕)
                Rectangle {
                    width: 28
                    height: 28
                    radius: 14
                    color: closeMouse.containsMouse ? "#3A3E4E" : "#282B36"

                    Text {
                        anchors.centerIn: parent
                        text: "✕"
                        color: HoneydewTheme.textSecondary
                        font.pixelSize: 11
                        font.bold: true
                    }

                    MouseArea {
                        id: closeMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.closeRequested()
                    }
                }
            }
        }

        // ══════════════════════════════════════════════
        // SECCIÓN MASTER DEL CANAL (VOLUMEN & PAN)
        // ══════════════════════════════════════════════
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 90
            color: "#181920"
            border.color: HoneydewTheme.borderSubtle
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 14

                // Knob de Panorama
                HoneydewKnob {
                    label: "PAN"
                    from: -1.0
                    to: 1.0
                    value: 0.0
                    isBipolar: true
                    accentColor: HoneydewTheme.flCyan
                    onValueModified: (v) => {
                        if (typeof AudioEngine !== "undefined" && AudioEngine.tracks) {
                            AudioEngine.tracks.setPan(root.trackIndex, v)
                        }
                    }
                }

                // Knob de Ganancia
                HoneydewKnob {
                    label: "GAIN"
                    from: -24.0
                    to: 12.0
                    value: 0.0
                    isBipolar: true
                    suffix: "dB"
                    accentColor: HoneydewTheme.flOrange
                }

                Item { Layout.fillWidth: true }

                // Slider vertical de Volumen con Vúmetro
                Item {
                    Layout.preferredWidth: 64
                    Layout.fillHeight: true

                    Column {
                        anchors.centerIn: parent
                        spacing: 2

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "VOL"
                            color: HoneydewTheme.textSecondary
                            font.pixelSize: 9
                            font.bold: true
                        }

                        Slider {
                            id: volSlider
                            orientation: Qt.Vertical
                            from: 0.0
                            to: 1.2
                            value: 1.0
                            height: 52
                            anchors.horizontalCenter: parent.horizontalCenter

                            onValueChanged: {
                                if (typeof AudioEngine !== "undefined" && AudioEngine.tracks) {
                                    AudioEngine.tracks.setGain(root.trackIndex, value)
                                }
                            }
                        }
                    }
                }
            }
        }

        // ══════════════════════════════════════════════
        // LISTA DE MÓDULOS JSFX / EFECTOS
        // ══════════════════════════════════════════════
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: HoneydewTheme.bgMain

            ScrollView {
                anchors.fill: parent
                clip: true

                Column {
                    width: parent.width
                    spacing: 8
                    topPadding: 10
                    bottomPadding: 10

                    // Tarjetas de plugins JSFX activos
                    Repeater {
                        model: root.pluginsList

                        Rectangle {
                            width: parent.width - 16
                            anchors.horizontalCenter: parent.horizontalCenter
                            height: 84
                            radius: 8
                            color: HoneydewTheme.bgCard
                            border.color: HoneydewTheme.borderSubtle
                            border.width: 1

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 8
                                spacing: 4

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 6

                                    // Switch Bypass ON/OFF
                                    Rectangle {
                                        width: 20
                                        height: 20
                                        radius: 10
                                        property bool isEnabled: modelData.enabled !== undefined ? modelData.enabled : true
                                        color: isEnabled ? HoneydewTheme.flGreen : "#383C48"

                                        Text {
                                            anchors.centerIn: parent
                                            text: "✓"
                                            color: parent.isEnabled ? "#000000" : "#808696"
                                            font.pixelSize: 10
                                            font.bold: true
                                        }

                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                if (typeof AudioEngine !== "undefined" && AudioEngine.tracks) {
                                                    AudioEngine.tracks.setPluginBypassed(root.trackIndex, modelData.index, parent.isEnabled)
                                                }
                                            }
                                        }
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        text: modelData.name || ("JSFX " + (index + 1))
                                        color: HoneydewTheme.textPrimary
                                        font.pixelSize: 11
                                        font.bold: true
                                        elide: Text.ElideRight
                                    }

                                    // Botón Editar Código JSFX
                                    Rectangle {
                                        width: 24
                                        height: 20
                                        radius: 4
                                        color: "#2C303D"

                                        Text {
                                            anchors.centerIn: parent
                                            text: "</>"
                                            color: HoneydewTheme.flOrange
                                            font.pixelSize: 9
                                            font.bold: true
                                        }

                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                root.editJsfxRequested(root.trackIndex, modelData.index, modelData.name || "plugin.jsfx")
                                            }
                                        }
                                    }

                                    // Botón Eliminar Plugin
                                    Rectangle {
                                        width: 20
                                        height: 20
                                        radius: 4
                                        color: "#2C303D"

                                        Text {
                                            anchors.centerIn: parent
                                            text: "✕"
                                            color: "#FF5252"
                                            font.pixelSize: 9
                                            font.bold: true
                                        }

                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                if (typeof AudioEngine !== "undefined" && AudioEngine.tracks) {
                                                    AudioEngine.tracks.removePluginFromTrack(root.trackIndex, modelData.index)
                                                }
                                            }
                                        }
                                    }
                                }

                                // Barra de Slider interactivo rápido del primer parámetro JSFX
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 6

                                    Text {
                                        text: "PAR 1"
                                        color: HoneydewTheme.textSecondary
                                        font.pixelSize: 8
                                        font.bold: true
                                    }

                                    Slider {
                                        Layout.fillWidth: true
                                        from: 0.0
                                        to: 1.0
                                        value: 0.5
                                    }
                                }
                            }
                        }
                    }

                    // Botón Grande "+ ADD JSFX / REAPACK"
                    Rectangle {
                        width: parent.width - 16
                        anchors.horizontalCenter: parent.horizontalCenter
                        height: 48
                        radius: 8
                        color: addFxMouse.pressed ? "#2B2E3A" : (addFxMouse.containsMouse ? "#242732" : HoneydewTheme.bgCard)
                        border.color: addFxMouse.containsMouse ? HoneydewTheme.flOrange : HoneydewTheme.borderSubtle
                        border.width: 1

                        Row {
                            anchors.centerIn: parent
                            spacing: 8

                            Text {
                                text: "+"
                                color: HoneydewTheme.flOrange
                                font.pixelSize: 18
                                font.bold: true
                                anchors.verticalCenter: parent.verticalCenter
                            }

                            Text {
                                text: "ADD JSFX EFFECT"
                                color: HoneydewTheme.textPrimary
                                font.pixelSize: 11
                                font.bold: true
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }

                        MouseArea {
                            id: addFxMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.reapackRequested()
                        }
                    }
                }
            }
        }
    }
}
