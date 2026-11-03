// NovaWelcomeDialog.qml
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtCore

Rectangle {
    id: welcomeRoot
    anchors.fill: parent
    color: "#E807080C"
    z: 100005
    focus: visible

    property bool forceShow: false

    Settings {
        id: welcomeSettings
        category: "NovaOnboarding"
        property bool welcomeSeen: false
    }

    // Se muestra solo en primera apertura (o al invocar show()) si no hay actualización obligatoria en curso
    visible: (forceShow || !welcomeSettings.welcomeSeen) &&
             !(typeof UpdateChecker !== "undefined" && UpdateChecker.updateRequired)

    function triggerHaptic() {
        if (typeof AudioEngine !== "undefined" && AudioEngine.hapticFeedback) {
            AudioEngine.hapticFeedback()
        }
    }

    function dismiss() {
        triggerHaptic()
        welcomeSettings.welcomeSeen = true
        forceShow = false
        welcomeRoot.visible = false
    }

    function show() {
        forceShow = true
        welcomeRoot.visible = true
        entranceAnim.restart()
    }

    onVisibleChanged: {
        if (visible) {
            entranceAnim.restart()
        }
    }

    Keys.onEscapePressed: dismiss()
    Keys.onReturnPressed: dismiss()
    Keys.onEnterPressed: dismiss()

    // Animación de entrada suave del modal
    ParallelAnimation {
        id: entranceAnim
        NumberAnimation {
            target: dialogCard
            property: "opacity"
            from: 0.0
            to: 1.0
            duration: 220
            easing.type: Easing.OutCubic
        }
        NumberAnimation {
            target: dialogCard
            property: "scale"
            from: 0.94
            to: 1.0
            duration: 240
            easing.type: Easing.OutBack
        }
    }

    // Bloquear interacción de clics, hover y rueda del ratón con las capas inferiores del DAW
    MouseArea {
        anchors.fill: parent
        preventStealing: true
        hoverEnabled: true
         onWheel: (wheel) => { wheel.accepted = true }
    }

    // Tarjeta Modal Central
    Rectangle {
        id: dialogCard
        anchors.centerIn: parent
        width: Math.min(parent.width * 0.92, 550)
        height: Math.min(parent.height * 0.92, Math.max(420, cardLayout.implicitHeight + 36))
        radius: 20
        color: "#11131C"
        border.color: "#2B3148"
        border.width: 1.5
        clip: true

        // Gradiente de fondo sutil estilo estudio profesional
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#171A28" }
            GradientStop { position: 0.35; color: "#121520" }
            GradientStop { position: 1.0; color: "#0D0F17" }
        }

        // Halo ambiental superior detrás del icono
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            y: -65
            width: 260
            height: 130
            radius: 65
            color: "#3B82F6"
            opacity: 0.10
        }

        // Acento luminoso en el borde superior
        Rectangle {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 3
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: "#3B82F6" }
                GradientStop { position: 0.5; color: "#8B5CF6" }
                GradientStop { position: 1.0; color: "#06B6D4" }
            }
        }

        ColumnLayout {
            id: cardLayout
            anchors.fill: parent
            anchors.topMargin: 18
            anchors.bottomMargin: 16
            anchors.leftMargin: 22
            anchors.rightMargin: 22
            spacing: 12

            // ==========================================
            // 1. CABECERA (ICONO, BADGE Y TÍTULOS)
            // ==========================================
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 10

                // Icono con anillo exterior translúcido
                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    width: 58
                    height: 58
                    radius: 29
                    color: "#1A3B82F6"
                    border.color: "#3360A5FA"
                    border.width: 1

                    Rectangle {
                        anchors.centerIn: parent
                        width: 46
                        height: 46
                        radius: 23
                        gradient: Gradient {
                            orientation: Gradient.Vertical
                            GradientStop { position: 0.0; color: "#3B82F6" }
                            GradientStop { position: 1.0; color: "#1D4ED8" }
                        }
                        border.color: "#93C5FD"
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
                    Layout.alignment: Qt.AlignHCenter
                    height: 24
                    width: badgeRow.implicitWidth + 20
                    radius: 12
                    color: "#172236"
                    border.color: "#3B82F6"
                    border.width: 1

                    Row {
                        id: badgeRow
                        anchors.centerIn: parent
                        spacing: 6

                        Rectangle {
                            width: 6
                            height: 6
                            radius: 3
                            color: "#60A5FA"
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: "ACCESO ANTICIPADO · PROYECTO INDIE"
                            color: "#60A5FA"
                            font.pixelSize: 10
                            font.bold: true
                            font.letterSpacing: 0.6
                        }
                    }
                }

                // Títulos principales
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 3

                    Text {
                        Layout.fillWidth: true
                        text: "¡Bienvenido a NOVA Studio!"
                        color: "#F8FAFC"
                        font.pixelSize: 21
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                    }

                    Text {
                        Layout.fillWidth: true
                        text: "Estación de audio digital (DAW) móvil · Creada con pasión"
                        color: "#94A3B8"
                        font.pixelSize: 12
                        horizontalAlignment: Text.AlignHCenter
                    }
                }
            }

            // ==========================================
            // 2. CONTENIDO DESPLAZABLE (TARJETAS INFO)
            // ==========================================
            ScrollView {
                id: scrollArea
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredHeight: contentCol.implicitHeight
                clip: true
                ScrollBar.vertical.policy: ScrollBar.AsNeeded
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                Column {
                    id: contentCol
                    width: scrollArea.availableWidth
                    spacing: 12

                    // Tarjeta 1: Desarrollado por 1 sola persona
                    Rectangle {
                        width: parent.width
                        implicitHeight: devRow.implicitHeight + 24
                        radius: 12
                        color: "#171B28"
                        border.color: "#283048"
                        border.width: 1
                        clip: true

                        // Barra lateral de acento azul
                        Rectangle {
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            width: 4
                            color: "#3B82F6"
                        }

                        RowLayout {
                            id: devRow
                            anchors.fill: parent
                            anchors.leftMargin: 16
                            anchors.rightMargin: 14
                            anchors.topMargin: 12
                            anchors.bottomMargin: 12
                            spacing: 12

                            Rectangle {
                                Layout.alignment: Qt.AlignTop
                                width: 40
                                height: 40
                                radius: 10
                                color: "#1E293B"
                                border.color: "#334155"
                                border.width: 1

                                Text {
                                    anchors.centerIn: parent
                                    text: "👨‍💻"
                                    font.pixelSize: 20
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 4

                                Text {
                                    Layout.fillWidth: true
                                    text: "Desarrollado por 1 sola persona"
                                    color: "#F1F5F9"
                                    font.pixelSize: 13
                                    font.bold: true
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: "NOVA Studio es un proyecto 100% independiente. Cada línea de código, el motor de audio de baja latencia y la interfaz están siendo construidos día a día por un único desarrollador."
                                    color: "#94A3B8"
                                    font.pixelSize: 12
                                    wrapMode: Text.WordWrap
                                    lineHeight: 1.25
                                }
                            }
                        }
                    }

                    // Tarjeta 2: Fase de desarrollo activo / Advertencia
                    Rectangle {
                        width: parent.width
                        implicitHeight: warnRow.implicitHeight + 24
                        radius: 12
                        color: "#1F160C"
                        border.color: "#78350F"
                        border.width: 1
                        clip: true

                        // Barra lateral de acento ámbar
                        Rectangle {
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            width: 4
                            color: "#F59E0B"
                        }

                        RowLayout {
                            id: warnRow
                            anchors.fill: parent
                            anchors.leftMargin: 16
                            anchors.rightMargin: 14
                            anchors.topMargin: 12
                            anchors.bottomMargin: 12
                            spacing: 12

                            Rectangle {
                                Layout.alignment: Qt.AlignTop
                                width: 40
                                height: 40
                                radius: 10
                                color: "#2E1F0F"
                                border.color: "#92400E"
                                border.width: 1

                                Text {
                                    anchors.centerIn: parent
                                    text: "⚠️"
                                    font.pixelSize: 19
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 4

                                Text {
                                    Layout.fillWidth: true
                                    text: "Fase de desarrollo activo: pueden existir fallos"
                                    color: "#FBBF24"
                                    font.pixelSize: 13
                                    font.bold: true
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: "Al encontrarse en evolución continua, es normal que experimentes pequeños errores, funciones en construcción o detalles por pulir. Tu comprensión, paciencia y sugerencias son fundamentales para hacer crecer este proyecto."
                                    color: "#FDE68A"
                                    font.pixelSize: 12
                                    wrapMode: Text.WordWrap
                                    lineHeight: 1.25
                                }
                            }
                        }
                    }
                }
            }

            // ==========================================
            // 3. PIE FIJO (BOTÓN DE ACCIÓN Y NOTA)
            // ==========================================
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 8

                Rectangle {
                    id: btnStart
                    Layout.fillWidth: true
                    Layout.preferredHeight: 46
                    radius: 12
                    scale: btnMouse.containsPress ? 0.98 : 1.0
                    Behavior on scale { NumberAnimation { duration: 100; easing.type: Easing.OutQuad } }

                    gradient: Gradient {
                        orientation: Gradient.Vertical
                        GradientStop {
                            position: 0.0
                            color: btnMouse.containsPress ? "#1D4ED8" : (btnMouse.containsMouse ? "#3B82F6" : "#2563EB")
                        }
                        GradientStop {
                            position: 1.0
                            color: btnMouse.containsPress ? "#1E40AF" : (btnMouse.containsMouse ? "#2563EB" : "#1D4ED8")
                        }
                    }
                    border.color: btnMouse.containsMouse ? "#93C5FD" : "#60A5FA"
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
                        onClicked: welcomeRoot.dismiss()
                    }
                }

                Text {
                    Layout.fillWidth: true
                    text: "Este mensaje solo se mostrará durante tu primera apertura."
                    color: "#64748B"
                    font.pixelSize: 11
                    font.italic: true
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }
    }
}