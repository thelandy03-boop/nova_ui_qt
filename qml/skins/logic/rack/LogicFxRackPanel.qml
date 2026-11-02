import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Rectangle {
    id: root

    property int trackIndex: -1
    property string trackName: "Track"
    property bool masterBypass: false

    signal closeRequested()

    color: "#121319"
    border.color: "#252834"
    border.width: 1

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

                Item { Layout.fillWidth: true } // spacer

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
                            root.masterBypass = !root.masterBypass
                        }
                    }
                }

                // Botón Cerrar (✕)
                Rectangle {
                    Layout.preferredWidth: 26
                    Layout.preferredHeight: 26
                    radius: 13
                    color: closeArea.containsPress ? "#334155" : (closeArea.containsMouse ? "#2A2F3D" : "#1E222D")
                    border.color: "#373C4E"
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
        // 2. CONTENEDOR DESLIZABLE DE EFECTOS
        // ==========================================
        Flickable {
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: effectsRow.implicitWidth + 24
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            flickableDirection: Flickable.HorizontalFlick

            Row {
                id: effectsRow
                x: 12
                anchors.verticalCenter: parent.verticalCenter
                spacing: 12

                // --- EFECTO 1: CHANNEL EQ ---
                LogicFxCard {
                    title: "Channel EQ"
                    category: "EQ"
                    accentColor: "#3B82F6"
                    enabledFx: !root.masterBypass

                    Row {
                        anchors.centerIn: parent
                        spacing: 12

                        LogicFxSlider {
                            label: "GRAVES"
                            value: 0.5
                            displayValue: ((value - 0.5) * 24.0 >= 0 ? "+" : "") + ((value - 0.5) * 24.0).toFixed(1) + " dB"
                            activeColor: "#3B82F6"
                        }
                        LogicFxSlider {
                            label: "MEDIOS"
                            value: 0.5
                            displayValue: ((value - 0.5) * 24.0 >= 0 ? "+" : "") + ((value - 0.5) * 24.0).toFixed(1) + " dB"
                            activeColor: "#60A5FA"
                        }
                        LogicFxSlider {
                            label: "AGUDOS"
                            value: 0.5
                            displayValue: ((value - 0.5) * 24.0 >= 0 ? "+" : "") + ((value - 0.5) * 24.0).toFixed(1) + " dB"
                            activeColor: "#93C5FD"
                        }
                    }
                }

                // --- EFECTO 2: SPACE REVERB ---
                LogicFxCard {
                    title: "Space Reverb"
                    category: "ESPACIO"
                    accentColor: "#8B5CF6"
                    enabledFx: !root.masterBypass

                    Row {
                        anchors.centerIn: parent
                        spacing: 10

                        LogicFxSlider {
                            label: "SALA"
                            value: 0.4
                            displayValue: Math.round(value * 100) + " %"
                            activeColor: "#8B5CF6"
                        }
                        LogicFxSlider {
                            label: "MEZCLA"
                            value: 0.25
                            displayValue: Math.round(value * 100) + " %"
                            activeColor: "#A78BFA"
                        }

                        // Presets rápidos
                        Column {
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 6

                            Repeater {
                                model: ["Room", "Hall", "Plate"]
                                Rectangle {
                                    width: 52
                                    height: 22
                                    radius: 4
                                    color: (index === 0) ? "#8B5CF6" : "#212431"
                                    border.color: (index === 0) ? "#A78BFA" : "#32374A"
                                    border.width: 1

                                    Text {
                                        anchors.centerIn: parent
                                        text: modelData
                                        color: (index === 0) ? "#FFFFFF" : "#94A3B8"
                                        font.pixelSize: 9
                                        font.bold: true
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            if (typeof AudioEngine !== "undefined" && AudioEngine.hapticFeedback) {
                                                AudioEngine.hapticFeedback()
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // --- EFECTO 3: TAPE DELAY ---
                LogicFxCard {
                    title: "Tape Delay"
                    category: "DELAY"
                    accentColor: "#10B981"
                    enabledFx: !root.masterBypass

                    Row {
                        anchors.centerIn: parent
                        spacing: 12

                        LogicFxSlider {
                            label: "TIEMPO"
                            value: 0.35
                            displayValue: Math.round(value * 800) + " ms"
                            activeColor: "#10B981"
                        }
                        LogicFxSlider {
                            label: "FEEDBACK"
                            value: 0.3
                            displayValue: Math.round(value * 100) + " %"
                            activeColor: "#34D399"
                        }
                        LogicFxSlider {
                            label: "MEZCLA"
                            value: 0.2
                            displayValue: Math.round(value * 100) + " %"
                            activeColor: "#6EE7B7"
                        }
                    }
                }

                // --- EFECTO 4: COMPRESSOR ---
                LogicFxCard {
                    title: "Compressor"
                    category: "DINÁMICA"
                    accentColor: "#F59E0B"
                    enabledFx: !root.masterBypass

                    Row {
                        anchors.centerIn: parent
                        spacing: 12

                        LogicFxSlider {
                            label: "UMBRAL"
                            value: 0.7
                            displayValue: ((value - 1.0) * 40.0).toFixed(0) + " dB"
                            activeColor: "#F59E0B"
                        }
                        LogicFxSlider {
                            label: "RATIO"
                            value: 0.4
                            displayValue: (1.0 + value * 7.0).toFixed(1) + ":1"
                            activeColor: "#FBBF24"
                        }
                        LogicFxSlider {
                            label: "GANANCIA"
                            value: 0.2
                            displayValue: "+" + (value * 12.0).toFixed(1) + " dB"
                            activeColor: "#FCD34D"
                        }
                    }
                }

                // --- EFECTO 5: SLOT AÑADIR EFECTO (+) ---
                Rectangle {
                    width: 130
                    height: 165
                    radius: 10
                    color: "#14151C"
                    border.color: addArea.containsMouse ? "#3B82F6" : "#242735"
                    border.width: 1

                    Column {
                        anchors.centerIn: parent
                        spacing: 8

                        Rectangle {
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: 38
                            height: 38
                            radius: 19
                            color: addArea.containsPress ? "#2563EB" : (addArea.containsMouse ? "#1E293B" : "#1B1D27")
                            border.color: "#3B82F6"
                            border.width: 1

                            Text {
                                anchors.centerIn: parent
                                text: "+"
                                color: "#60A5FA"
                                font.pixelSize: 22
                                font.bold: true
                            }
                        }

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "Añadir Plug-in"
                            color: addArea.containsMouse ? "#60A5FA" : "#8E94A5"
                            font.pixelSize: 11
                            font.bold: true
                        }

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "Chorus, Flanger..."
                            color: "#4B5563"
                            font.pixelSize: 9
                        }
                    }

                    MouseArea {
                        id: addArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (typeof AudioEngine !== "undefined" && AudioEngine.hapticFeedback) {
                                AudioEngine.hapticFeedback()
                            }
                        }
                    }
                }
            }
        }
    }
}
