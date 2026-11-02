import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Rectangle {
    id: whatsNewRoot
    anchors.fill: parent
    color: "#BF07090E" // Fondo oscuro profundo con mayor contraste
    z: 100000
    visible: (typeof UpdateChecker !== "undefined" && UpdateChecker) ? UpdateChecker.showWhatsNew : false
    focus: visible

    // Función centralizada para cerrar el diálogo
    function closeDialog() {
        if (typeof UpdateChecker !== "undefined" && UpdateChecker) {
            UpdateChecker.dismissWhatsNew()
        } else {
            whatsNewRoot.visible = false
        }
    }

    function close() {
        closeDialog()
    }

    // Soporte de navegación y teclado (Atrás en Android / Esc / Enter para cerrar)
    Keys.onBackPressed: (event) => {
        closeDialog()
        event.accepted = true
    }
    Keys.onEscapePressed: closeDialog()
    Keys.onReturnPressed: closeDialog()

    // Animación de entrada al mostrarse el modal
    onVisibleChanged: {
        if (visible) {
            whatsNewRoot.forceActiveFocus()
            introAnim.restart()
        }
    }

    ParallelAnimation {
        id: introAnim
        NumberAnimation {
            target: dialogCard
            property: "opacity"
            from: 0.0
            to: 1.0
            duration: 220
            easing.type: Easing.OutCubic
        }
        NumberAnimation {
            target: cardScale
            property: "xScale"
            from: 0.94
            to: 1.0
            duration: 240
            easing.type: Easing.OutBack
        }
        NumberAnimation {
            target: cardScale
            property: "yScale"
            from: 0.94
            to: 1.0
            duration: 240
            easing.type: Easing.OutBack
        }
    }

    // Datos cargados nativamente por C++ desde whats_new.json
    readonly property var currentData: (typeof UpdateChecker !== "undefined" && UpdateChecker && UpdateChecker.currentWhatsNew) ? UpdateChecker.currentWhatsNew : ({})
    readonly property var featuresList: currentData.features || []

    // Bloqueador de scroll hacia el timeline de fondo
    WheelHandler {
        id: rootWheelBlocker
        target: null
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
        onWheel: function(event) {
            event.accepted = true
        }
    }

    // Bloqueador de clics fuera de la tarjeta
    MouseArea {
        anchors.fill: parent
        propagateComposedEvents: false
        hoverEnabled: true
        preventStealing: true
        onClicked: {}
        onWheel: function(wheel) { wheel.accepted = true }
    }

    // ── TARJETA MODAL PRINCIPAL ──
    Rectangle {
        id: dialogCard
        anchors.centerIn: parent
        width: Math.min(parent.width - 28, 560)
        height: Math.min(parent.height - 36, 600)
        color: "#13161F"
        border.color: "#2A3142"
        border.width: 1
        radius: 16
        clip: true

        transform: Scale {
            id: cardScale
            origin.x: dialogCard.width / 2
            origin.y: dialogCard.height / 2
        }

        // Iluminación ambiental superior (estilo DAW moderno)
        Rectangle {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 130
            radius: 16
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#243B82F6" }
                GradientStop { position: 1.0; color: "#0013161F" }
            }
        }

        // Borde interno de cristal (bisel luminoso)
        Rectangle {
            anchors.fill: parent
            anchors.margins: 1
            color: "transparent"
            border.color: "#18FFFFFF"
            border.width: 1
            radius: 15
            z: 5
        }

        // ── BOTÓN CERRAR SUPERIOR (✕) ──
        Rectangle {
            id: closeBtn
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.margins: 16
            width: 32
            height: 32
            radius: 10
            color: closeMouse.pressed
                   ? "#2D3548"
                   : (closeMouse.containsMouse ? "#252C3D" : "#1A1F2C")
            border.color: closeMouse.containsMouse ? "#475569" : "#2E3646"
            border.width: 1
            z: 10

            Behavior on color { ColorAnimation { duration: 140 } }
            Behavior on border.color { ColorAnimation { duration: 140 } }

            Text {
                anchors.centerIn: parent
                text: "✕"
                color: closeMouse.containsMouse ? "#F8FAFC" : "#94A3B8"
                font.pixelSize: 13
                font.bold: true
            }

            MouseArea {
                id: closeMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: whatsNewRoot.closeDialog()
            }
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 22
            spacing: 14

            // ── CABECERA ──
            ColumnLayout {
                Layout.fillWidth: true
                Layout.rightMargin: 38 // Evita solapamiento con el botón ✕
                spacing: 8

                RowLayout {
                    spacing: 8

                    // Píldora de versión principal con selector de historial
                    Rectangle {
                        id: versionPill
                        Layout.preferredWidth: versionRow.implicitWidth + (hasMultipleVersions ? 26 : 18)
                        Layout.preferredHeight: 24
                        radius: 12
                        color: hasMultipleVersions && pillMouse.containsMouse ? "#1E3A8A" : "#172554"
                        border.color: hasMultipleVersions && pillMouse.containsMouse ? "#60A5FA" : "#3B82F6"
                        border.width: 1

                        readonly property bool hasMultipleVersions: (typeof UpdateChecker !== "undefined" && UpdateChecker && UpdateChecker.availableVersions && UpdateChecker.availableVersions.length > 1)

                        Row {
                            id: versionRow
                            anchors.centerIn: parent
                            spacing: 6

                            Text {
                                text: whatsNewRoot.currentData.updateBadge || "✨ ACTUALIZACIÓN"
                                color: "#60A5FA"
                                font.pixelSize: 10
                                font.bold: true
                                font.letterSpacing: 0.5
                            }

                            Rectangle {
                                width: 3
                                height: 3
                                radius: 1.5
                                color: "#60A5FA"
                                anchors.verticalCenter: parent.verticalCenter
                            }

                            Text {
                                text: "v" + (whatsNewRoot.currentData.version || (typeof UpdateChecker !== "undefined" && UpdateChecker ? UpdateChecker.currentVersion : "1.0.11"))
                                color: "#DBEAFE"
                                font.pixelSize: 10
                                font.bold: true
                            }

                            Text {
                                visible: versionPill.hasMultipleVersions
                                text: "▾"
                                color: "#93C5FD"
                                font.pixelSize: 9
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }

                        MouseArea {
                            id: pillMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            enabled: versionPill.hasMultipleVersions
                            cursorShape: versionPill.hasMultipleVersions ? Qt.PointingHandCursor : Qt.ArrowCursor
                            onClicked: versionMenu.open()
                        }

                        Menu {
                            id: versionMenu
                            y: versionPill.height + 4
                            background: Rectangle {
                                implicitWidth: 150
                                color: "#181C26"
                                border.color: "#2E3648"
                                border.width: 1
                                radius: 8
                            }
                            Repeater {
                                model: (typeof UpdateChecker !== "undefined" && UpdateChecker) ? UpdateChecker.availableVersions : []
                                MenuItem {
                                    required property var modelData
                                    text: "v" + modelData + ((typeof UpdateChecker !== "undefined" && UpdateChecker && modelData === UpdateChecker.currentVersion) ? " (Actual)" : "")
                                    onTriggered: {
                                        if (typeof UpdateChecker !== "undefined" && UpdateChecker) {
                                            UpdateChecker.selectWhatsNewVersion(modelData)
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // Contador de mejoras
                    Rectangle {
                        Layout.preferredWidth: countLabel.implicitWidth + 14
                        Layout.preferredHeight: 24
                        radius: 12
                        color: "#1A202E"
                        border.color: "#2E3648"
                        border.width: 1

                        Text {
                            id: countLabel
                            anchors.centerIn: parent
                            text: (whatsNewRoot.featuresList ? whatsNewRoot.featuresList.length : 0) + " mejoras clave"
                            color: "#94A3B8"
                            font.pixelSize: 10
                            font.weight: Font.Medium
                        }
                    }
                }

                Text {
                    text: whatsNewRoot.currentData.title || "¿Qué hay de nuevo en Nova?"
                    color: "#F8FAFC"
                    font.pixelSize: 22
                    font.bold: true
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                }

                Text {
                    text: whatsNewRoot.currentData.description || ""
                    color: "#94A3B8"
                    font.pixelSize: 12
                    lineHeight: 1.25
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                }
            }

            // Separador horizontal con gradiente
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: "#103B82F6" }
                    GradientStop { position: 0.5; color: "#384152" }
                    GradientStop { position: 1.0; color: "#103B82F6" }
                }
            }

            // ── CONTENEDOR SCROLLABLE CON MÁSCARAS DE DESVANECIMIENTO ──
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                Flickable {
                    id: flickArea
                    anchors.fill: parent
                    anchors.rightMargin: 6
                    contentHeight: contentCol.implicitHeight + 4
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    flickDeceleration: 3500

                    WheelHandler {
                        id: flickWheelHandler
                        target: null
                        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                        onWheel: function(event) {
                            var step = -event.pixelDelta.y
                            if (step === 0) {
                                step = -event.angleDelta.y * 0.6
                            }
                            var maxY = Math.max(0, flickArea.contentHeight - flickArea.height)
                            flickArea.contentY = Math.max(0, Math.min(maxY, flickArea.contentY + step))
                            event.accepted = true
                        }
                    }

                    ScrollBar.vertical: ScrollBar {
                        id: vScroll
                        parent: flickArea.parent
                        anchors.top: flickArea.top
                        anchors.right: parent.right
                        anchors.bottom: flickArea.bottom
                        width: 5
                        policy: ScrollBar.AsNeeded
                        contentItem: Rectangle {
                            implicitWidth: 5
                            radius: 2.5
                            color: vScroll.pressed ? "#60A5FA" : "#3B4458"
                            opacity: flickArea.contentHeight > flickArea.height ? 0.85 : 0.0
                            Behavior on color { ColorAnimation { duration: 120 } }
                        }
                    }

                    ColumnLayout {
                        id: contentCol
                        width: flickArea.width
                        spacing: 10

                        Repeater {
                            model: whatsNewRoot.featuresList

                            delegate: Rectangle {
                                id: featureCard
                                required property var modelData
                                required property int index

                                Layout.fillWidth: true
                                Layout.preferredHeight: cardInnerCol.implicitHeight + 24
                                radius: 12
                                color: cardHover.hovered ? "#1E2331" : "#181C26"
                                border.color: cardHover.hovered ? modelData.accentBorder : "#262D3D"
                                border.width: 1
                                clip: true

                                Behavior on color { ColorAnimation { duration: 150 } }
                                Behavior on border.color { ColorAnimation { duration: 150 } }


                                // Halo de fondo suave al pasar el cursor
                                Rectangle {
                                    anchors.fill: parent
                                    color: featureCard.modelData.accentGlow
                                    opacity: cardHover.hovered ? 1.0 : 0.0
                                    Behavior on opacity { NumberAnimation { duration: 160 } }
                                }

                                HoverHandler {
                                    id: cardHover
                                }

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 14
                                    anchors.rightMargin: 12
                                    anchors.topMargin: 12
                                    anchors.bottomMargin: 12
                                    spacing: 12

                                    // Icono cuadrado redondeado con relieve
                                    Rectangle {
                                        Layout.preferredWidth: 34
                                        Layout.preferredHeight: 34
                                        Layout.alignment: Qt.AlignTop
                                        radius: 10
                                        color: featureCard.modelData.accentBg
                                        border.color: featureCard.modelData.accentBorder
                                        border.width: 1

                                        Text {
                                            anchors.centerIn: parent
                                            text: featureCard.modelData.icon
                                            color: featureCard.modelData.accentText
                                            font.pixelSize: 15
                                            font.bold: true
                                        }
                                    }

                                    // Textos y etiqueta de categoría
                                    ColumnLayout {
                                        id: cardInnerCol
                                        Layout.fillWidth: true
                                        spacing: 5

                                        RowLayout {
                                            Layout.fillWidth: true
                                            spacing: 8

                                            Text {
                                                text: featureCard.modelData.title
                                                color: "#F8FAFC"
                                                font.pixelSize: 13
                                                font.bold: true
                                                Layout.fillWidth: true
                                                wrapMode: Text.WordWrap
                                            }

                                            // Píldora de estado auto-ajustable (sin truncado)
                                            Rectangle {
                                                Layout.preferredWidth: badgeLabel.implicitWidth + 14
                                                Layout.preferredHeight: 20
                                                Layout.alignment: Qt.AlignTop
                                                radius: 10
                                                color: featureCard.modelData.accentBg
                                                border.color: featureCard.modelData.accentBorder
                                                border.width: 1

                                                Text {
                                                    id: badgeLabel
                                                    anchors.centerIn: parent
                                                    text: featureCard.modelData.badge
                                                    color: featureCard.modelData.accentText
                                                    font.pixelSize: 9
                                                    font.bold: true
                                                    font.letterSpacing: 0.4
                                                }
                                            }
                                        }

                                        Text {
                                            text: featureCard.modelData.description
                                            color: "#94A3B8"
                                            font.pixelSize: 12
                                            Layout.fillWidth: true
                                            wrapMode: Text.WordWrap
                                            lineHeight: 1.28
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // Sombra de desvanecimiento superior al hacer scroll
                Rectangle {
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: 18
                    visible: flickArea.contentY > 4
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: "#13161F" }
                        GradientStop { position: 1.0; color: "#0013161F" }
                    }
                }

                // Sombra de desvanecimiento inferior cuando hay más contenido abajo
                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: 22
                    visible: (flickArea.contentY + flickArea.height) < (flickArea.contentHeight - 4)
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: "#0013161F" }
                        GradientStop { position: 1.0; color: "#13161F" }
                    }
                }
            }

            // ── PIE DE MODAL Y BOTÓN DE ACCIÓN PRINCIPAL (CTA) ──
            Rectangle {
                id: ctaBtn
                Layout.fillWidth: true
                Layout.preferredHeight: 46
                radius: 11
                border.color: ctaMouse.containsMouse ? "#93C5FD" : "#60A5FA"
                border.width: 1

                scale: ctaMouse.pressed ? 0.985 : 1.0
                Behavior on scale { NumberAnimation { duration: 90 } }

                gradient: Gradient {
                    GradientStop {
                        position: 0.0
                        color: ctaMouse.pressed ? "#1D4ED8" : (ctaMouse.containsMouse ? "#3B82F6" : "#2563EB")
                    }
                    GradientStop {
                        position: 1.0
                        color: ctaMouse.pressed ? "#1E3A8A" : (ctaMouse.containsMouse ? "#2563EB" : "#1D4ED8")
                    }
                }

                Text {
                    anchors.centerIn: parent
                    text: whatsNewRoot.currentData.buttonText || "¡Empezar a Crear! ➜"
                    color: "#FFFFFF"
                    font.pixelSize: 14
                    font.bold: true
                }

                MouseArea {
                    id: ctaMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: whatsNewRoot.closeDialog()
                }
            }
        }
    }
}