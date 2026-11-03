import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtCore

Rectangle {
    id: welcomeRoot
    anchors.fill: parent
    color: "#E608090D"
    z: 100005

    Settings {
        id: welcomeSettings
        category: "NovaOnboarding"
        property bool welcomeSeen: false
    }

    // Se muestra solo en primera apertura si no hay una actualización obligatoria en curso
    visible: !welcomeSettings.welcomeSeen && !(typeof UpdateChecker !== "undefined" && UpdateChecker.updateRequired)

    function dismiss() {
        welcomeSettings.welcomeSeen = true
        welcomeRoot.visible = false
    }

    function show() {
        welcomeRoot.visible = true
    }

    // Bloquear interacción con las capas inferiores
    MouseArea {
        anchors.fill: parent
        preventStealing: true
    }

    // Tarjeta Modal Central
    Rectangle {
        id: dialogCard
        anchors.centerIn: parent
        width: Math.min(parent.width * 0.92, 540)
        height: Math.min(parent.height * 0.92, 530)
        radius: 20
        color: "#13151F"
        border.color: "#2C3147"
        border.width: 1.5
        clip: true

        // Acento sutil en el borde superior
        Rectangle {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 3
            radius: 2
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: "#3B82F6" }
                GradientStop { position: 0.5; color: "#8B5CF6" }
                GradientStop { position: 1.0; color: "#06B6D4" }
            }
        }

        ScrollView {
            id: scrollArea
            anchors.fill: parent
            anchors.topMargin: 12
            anchors.bottomMargin: 12
            anchors.leftMargin: 20
            anchors.rightMargin: 20
            clip: true
            ScrollBar.vertical.policy: ScrollBar.AsNeeded

            Column {
                id: contentCol
                width: scrollArea.width - (scrollArea.ScrollBar.vertical.visible ? 12 : 0)
                spacing: 16

                // 1. Icono y Badge superior
                Row {
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 10

                    Rectangle {
                        width: 48
                        height: 48
                        radius: 24
                        gradient: Gradient {
                            orientation: Gradient.Vertical
                            GradientStop { position: 0.0; color: "#3B82F6" }
                            GradientStop { position: 1.0; color: "#1D4ED8" }
                        }
                        border.color: "#60A5FA"
                        border.width: 1

                        Text {
                            anchors.centerIn: parent
                            text: "🎧"
                            font.pixelSize: 22
                        }
                    }
                }

                // Badge de versión temprana
                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    height: 24
                    width: badgeText.implicitWidth + 20
                    radius: 12
                    color: "#1E293B"
                    border.color: "#3B82F6"
                    border.width: 1

                    Text {
                        id: badgeText
                        anchors.centerIn: parent
                        text: "🛠️ ACCESO ANTICIPADO · PROYECTO INDIE"
                        color: "#60A5FA"
                        font.pixelSize: 10
                        font.bold: true
                        font.letterSpacing: 0.5
                    }
                }

                // 2. Títulos
                Column {
                    width: parent.width
                    spacing: 4

                    Text {
                        text: "¡Bienvenido a NOVA Studio!"
                        color: "#FFFFFF"
                        font.pixelSize: 21
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        width: parent.width
                    }

                    Text {
                        text: "Estación de audio digital (DAW) móvil · Creada con pasión"
                        color: "#94A3B8"
                        font.pixelSize: 12
                        horizontalAlignment: Text.AlignHCenter
                        width: parent.width
                    }
                }

                // 3. Tarjeta: Desarrollado por 1 sola persona
                Rectangle {
                    width: parent.width
                    implicitHeight: devCol.implicitHeight + 20
                    radius: 12
                    color: "#181B26"
                    border.color: "#282E42"
                    border.width: 1

                    RowLayout {
                        id: devCol
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 12

                        Text {
                            text: "👨‍💻"
                            font.pixelSize: 26
                            Layout.alignment: Qt.AlignTop
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4

                            Text {
                                text: "Desarrollado por 1 sola persona"
                                color: "#F1F5F9"
                                font.pixelSize: 13
                                font.bold: true
                            }

                            Text {
                                text: "NOVA Studio es un proyecto 100% independiente. Cada línea de código, el motor de audio de baja latencia y la interfaz están siendo construidos día a día por un único desarrollador."
                                color: "#94A3B8"
                                font.pixelSize: 12
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                                lineHeight: 1.25
                            }
                        }
                    }
                }

                // 4. Tarjeta de Advertencia / Proyecto en Desarrollo
                Rectangle {
                    width: parent.width
                    implicitHeight: warnCol.implicitHeight + 20
                    radius: 12
                    color: "#22170A"
                    border.color: "#92400E"
                    border.width: 1

                    RowLayout {
                        id: warnCol
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 12

                        Text {
                            text: "⚠️"
                            font.pixelSize: 24
                            Layout.alignment: Qt.AlignTop
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4

                            Text {
                                text: "Fase de desarrollo activo: pueden existir fallos"
                                color: "#FBBF24"
                                font.pixelSize: 13
                                font.bold: true
                            }

                            Text {
                                text: "Al encontrarse en evolución continua, es normal que experimentes pequeños errores, funciones en construcción o detalles por pulir. Tu comprensión, paciencia y sugerencias son fundamentales para hacer crecer este proyecto."
                                color: "#FDE68A"
                                font.pixelSize: 12
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                                lineHeight: 1.25
                            }
                        }
                    }
                }

                // 5. Botón de Acción Principal
                Rectangle {
                    id: btnStart
                    width: parent.width
                    height: 48
                    radius: 12
                    gradient: Gradient {
                        orientation: Gradient.Vertical
                        GradientStop { position: 0.0; color: btnMouse.containsMouse ? "#3B82F6" : "#2563EB" }
                        GradientStop { position: 1.0; color: btnMouse.containsMouse ? "#2563EB" : "#1D4ED8" }
                    }
                    border.color: "#60A5FA"
                    border.width: 1

                    Row {
                        anchors.centerIn: parent
                        spacing: 8

                        Text {
                            text: "✨"
                            font.pixelSize: 16
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        Text {
                            text: "¡Entendido, comenzar a crear!"
                            color: "#FFFFFF"
                            font.pixelSize: 14
                            font.bold: true
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }

                    MouseArea {
                        id: btnMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (typeof AudioEngine !== "undefined") {
                                AudioEngine.hapticFeedback()
                            }
                            welcomeRoot.dismiss()
                        }
                    }
                }

                // 6. Pie de nota sutil
                Text {
                    text: "Este mensaje solo se mostrará durante tu primera apertura."
                    color: "#64748B"
                    font.pixelSize: 11
                    font.italic: true
                    horizontalAlignment: Text.AlignHCenter
                    anchors.horizontalCenter: parent.horizontalCenter
                }
            }
        }
    }
}
