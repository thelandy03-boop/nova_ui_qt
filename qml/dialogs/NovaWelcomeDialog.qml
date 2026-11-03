// qml/dialogs/NovaWelcomeDialog.qml
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtCore

Rectangle {
    id: welcomeRoot
    anchors.fill: parent
    color: "#D9080A0F"
    z: 100005
    focus: visible

    property bool forceShow: false

    Settings {
        id: welcomeSettings
        category: "NovaOnboarding"
        property bool welcomeSeen: false
    }

    // Se muestra solo en primera apertura si no hay una actualización obligatoria en curso
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
        openAnim.restart()
    }

    onVisibleChanged: {
        if (visible) {
            openAnim.restart()
        }
    }

    Keys.onEscapePressed: dismiss()
    Keys.onReturnPressed: dismiss()
    Keys.onEnterPressed: dismiss()

    // Animación de apertura sobria y rápida
    ParallelAnimation {
        id: openAnim
        NumberAnimation {
            target: dialogCard
            property: "opacity"
            from: 0.0
            to: 1.0
            duration: 180
            easing.type: Easing.OutCubic
        }
        NumberAnimation {
            target: dialogCard
            property: "scale"
            from: 0.97
            to: 1.0
            duration: 200
            easing.type: Easing.OutCubic
        }
    }

    // Bloquear interacción táctil, clics y rueda de ratón con las capas inferiores
    MouseArea {
        anchors.fill: parent
        preventStealing: true
        hoverEnabled: true
        onWheel: (wheel) => { wheel.accepted = true }
    }

    // =========================================================================
    // CONTENEDOR MODAL PRINCIPAL (STUDIO DARK)
    // =========================================================================
    Rectangle {
        id: dialogCard
        anchors.centerIn: parent
        width: Math.min(parent.width - 32, 440)
        height: Math.min(parent.height - 24, mainLayout.implicitHeight + 44)
        radius: 14
        color: "#13161F"
        border.color: "#2A3142"
        border.width: 1
        clip: true

        // Gradiente ambiental superior muy suave en azul estudio
        Rectangle {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 110
            gradient: Gradient {
                orientation: Gradient.Vertical
                GradientStop { position: 0.0; color: "#243B82F6" }
                GradientStop { position: 1.0; color: "transparent" }
            }
        }

        // Bisel interno cristalino (1px)
        Rectangle {
            anchors.fill: parent
            anchors.margins: 1
            radius: 13
            color: "transparent"
            border.color: "#18FFFFFF"
            border.width: 1
            z: 5
        }

        // Botón Cerrar (✕) en la esquina superior derecha
        Rectangle {
            id: closeBtn
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.topMargin: 14
            anchors.rightMargin: 14
            width: 28
            height: 28
            radius: 7
            z: 10
            color: closeMouse.containsPress ? "#252B3B" : (closeMouse.containsMouse ? "#1F2433" : "#1A1E29")
            border.color: closeMouse.containsMouse ? "#384157" : "#2A3142"
            border.width: 1
            Behavior on color { ColorAnimation { duration: 120 } }

            Text {
                anchors.centerIn: parent
                text: "✕"
                color: closeMouse.containsMouse ? "#F8FAFC" : "#94A3B8"
                font.pixelSize: 12
                font.bold: true
            }

            MouseArea {
                id: closeMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: welcomeRoot.dismiss()
            }
        }

        // Contenido adaptable (permite scroll únicamente en pantallas móviles ultra bajas)
        Flickable {
            id: bodyFlickable
            anchors.fill: parent
            anchors.topMargin: 22
            anchors.bottomMargin: 22
            anchors.leftMargin: 24
            anchors.rightMargin: 24
            contentWidth: width
            contentHeight: mainLayout.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            ColumnLayout {
                id: mainLayout
                width: bodyFlickable.width
                spacing: 18

                // =============================================================
                // 1. CABECERA: BADGE Y TÍTULO
                // =============================================================
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    // Badge discreto de fase experimental
                    Rectangle {
                        Layout.preferredHeight: 22
                        Layout.preferredWidth: badgeRow.implicitWidth + 16
                        radius: 6
                        color: "#221A0F"
                        border.color: "#5B3A12"
                        border.width: 1

                        Row {
                            id: badgeRow
                            anchors.centerIn: parent
                            spacing: 6

                            Rectangle {
                                width: 5
                                height: 5
                                radius: 3
                                color: "#F59E0B"
                                anchors.verticalCenter: parent.verticalCenter
                            }

                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: "FASE EXPERIMENTAL · EN CONSTRUCCIÓN"
                                color: "#FCD34D"
                                font.pixelSize: 10
                                font.bold: true
                                font.letterSpacing: 0.6
                            }
                        }
                    }

                    // Título principal y subtítulo humilde
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Text {
                            Layout.fillWidth: true
                            text: "NOVA STUDIO"
                            color: "#F8FAFC"
                            font.pixelSize: 20
                            font.bold: true
                            font.letterSpacing: 1.2
                        }

                        Text {
                            Layout.fillWidth: true
                            text: "Proyecto indie en fase temprana · No es una versión final"
                            color: "#8E96AA"
                            font.pixelSize: 12
                        }
                    }
                }

                // =============================================================
                // 2. BLOQUE COMPACTO DE PUNTOS ESENCIALES (TRANSPARENCIA)
                // =============================================================
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: infoListLayout.implicitHeight + 28
                    radius: 10
                    color: "#0D0F17"
                    border.color: "#2A3142"
                    border.width: 1

                    ColumnLayout {
                        id: infoListLayout
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 12

                        // Punto 1: Puede contener errores (Prioridad máxima)
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12

                            Rectangle {
                                Layout.alignment: Qt.AlignTop
                                Layout.topMargin: 5
                                width: 6
                                height: 6
                                radius: 3
                                color: "#F59E0B"
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                Text {
                                    Layout.fillWidth: true
                                    text: "Puede contener errores y funciones incompletas"
                                    color: "#FBBF24"
                                    font.pixelSize: 12
                                    font.bold: true
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: "NOVA Studio aún no es un DAW terminado ni estable. Al estar en construcción activa, es normal encontrar fallos, cierres o funciones a medias."
                                    color: "#94A3B8"
                                    font.pixelSize: 12
                                    wrapMode: Text.WordWrap
                                    lineHeight: 1.25
                                }
                            }
                        }

                        // Divisor horizontal sutil
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 1
                            color: "#1A1E29"
                        }

                        // Punto 2: Desarrollado por 1 persona
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12

                            Rectangle {
                                Layout.alignment: Qt.AlignTop
                                Layout.topMargin: 5
                                width: 6
                                height: 6
                                radius: 3
                                color: "#3B82F6"
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                Text {
                                    Layout.fillWidth: true
                                    text: "Desarrollado por 1 sola persona"
                                    color: "#F1F5F9"
                                    font.pixelSize: 12
                                    font.bold: true
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: "No hay un equipo grande detrás: es un proyecto personal hecho con esfuerzo y aprendizaje continuo. Tu paciencia y comprensión son clave."
                                    color: "#94A3B8"
                                    font.pixelSize: 12
                                    wrapMode: Text.WordWrap
                                    lineHeight: 1.25
                                }
                            }
                        }
                    }
                }

                // =============================================================
                // 3. BOTÓN PRINCIPAL Y PIE DE NOTA
                // =============================================================
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Rectangle {
                        id: btnContinue
                        Layout.fillWidth: true
                        Layout.preferredHeight: 40
                        radius: 8
                        scale: btnMouse.containsPress ? 0.985 : 1.0
                        Behavior on scale { NumberAnimation { duration: 90; easing.type: Easing.OutQuad } }

                        gradient: Gradient {
                            orientation: Gradient.Vertical
                            GradientStop {
                                position: 0.0
                                color: btnMouse.containsPress ? "#1D4ED8" : (btnMouse.containsMouse ? "#3B82F6" : "#2563EB")
                            }
                            GradientStop {
                                position: 1.0
                                color: btnMouse.containsPress ? "#1E3A8A" : (btnMouse.containsMouse ? "#2563EB" : "#1D4ED8")
                            }
                        }

                        border.color: btnMouse.containsMouse ? "#60A5FA" : "#3B82F6"
                        border.width: 1

                        // Bisel superior interno del botón
                        Rectangle {
                            anchors.fill: parent
                            anchors.margins: 1
                            radius: 7
                            color: "transparent"
                            border.color: "#22FFFFFF"
                            border.width: 1
                        }

                        Text {
                            anchors.centerIn: parent
                            text: "Entendido, explorar la app"
                            color: "#FFFFFF"
                            font.pixelSize: 13
                            font.bold: true
                            font.letterSpacing: 0.3
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
                        text: "Este aviso solo se mostrará durante tu primera apertura"
                        color: "#64748B"
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                    }
                }
            }
        }
    }
}