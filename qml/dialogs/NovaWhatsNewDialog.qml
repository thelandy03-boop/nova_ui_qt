import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

/**
 * Diálogo de Novedades ("What's New") de NOVA Studio.
 * Diseñado con alineación milimétrica, jerarquía visual limpia y estética profesional:
 * - Cabecera simétrica con píldora de versión, contador de mejoras y botón de cierre alineados
 * - Tarjetas de novedades auto-ajustables sin solapamientos ni saltos de texto
 * - Scrollbar flotante sin colisión con los bordes de las tarjetas
 * - Botón de acción principal táctil y atajos de teclado (Esc / Back / Enter)
 */
Rectangle {
    id: whatsNewRoot
    anchors.fill: parent
    color: "#D007090E" // Fondo oscuro con atenuación profunda
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
            duration: 200
            easing.type: Easing.OutCubic
        }
        NumberAnimation {
            target: cardScale
            property: "xScale"
            from: 0.95
            to: 1.0
            duration: 220
            easing.type: Easing.OutBack
        }
        NumberAnimation {
            target: cardScale
            property: "yScale"
            from: 0.95
            to: 1.0
            duration: 220
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
        width: Math.min(parent.width - 32, 540)
        height: Math.min(parent.height - 40, 590)
        color: "#13161F"
        border.color: "#262C3C"
        border.width: 1
        radius: 16
        clip: true

        transform: Scale {
            id: cardScale
            origin.x: dialogCard.width / 2
            origin.y: dialogCard.height / 2
        }

        // Iluminación ambiental superior suave
        Rectangle {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 120
            radius: 16
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#203B82F6" }
                GradientStop { position: 1.0; color: "#0013161F" }
            }
        }

        // Borde interno sutil (bisel luminoso)
        Rectangle {
            anchors.fill: parent
            anchors.margins: 1
            color: "transparent"
            border.color: "#15FFFFFF"
            border.width: 1
            radius: 15
            z: 5
        }

        // Contenedor principal estructurado
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 12

            // =================================================================
            // 1. FILA SUPERIOR: VERSIÓN, CONTADOR Y BOTÓN DE CIERRE (ALINEADOS)
            // =================================================================
            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 28
                spacing: 8

                // Píldora de versión con selector de historial
                Rectangle {
                    id: versionPill
                    Layout.preferredHeight: 26
                    implicitWidth: versionRow.implicitWidth + (hasMultipleVersions ? 24 : 16)
                    radius: 13
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
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        Rectangle {
                            width: 3
                            height: 3
                            radius: 1.5
                            color: "#60A5FA"
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        Text {
                            text: "v" + (whatsNewRoot.currentData.version || (typeof UpdateChecker !== "undefined" && UpdateChecker ? UpdateChecker.currentVersion : "1.0.17"))
                            color: "#DBEAFE"
                            font.pixelSize: 10
                            font.bold: true
                            anchors.verticalCenter: parent.verticalCenter
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
                    Layout.preferredHeight: 26
                    implicitWidth: countLabel.implicitWidth + 16
                    radius: 13
                    color: "#1A202E"
                    border.color: "#2E3648"
                    border.width: 1

                    Text {
                        id: countLabel
                        anchors.centerIn: parent
                        text: (whatsNewRoot.featuresList ? whatsNewRoot.featuresList.length : 0) + " mejoras"
                        color: "#94A3B8"
                        font.pixelSize: 10
                        font.bold: true
                    }
                }

                Item { Layout.fillWidth: true }

                // Botón cerrar (✕) perfectamente alineado en la fila superior
                Rectangle {
                    id: closeBtn
                    Layout.preferredWidth: 28
                    Layout.preferredHeight: 28
                    radius: 8
                    color: closeMouse.pressed ? "#2D3548" : (closeMouse.containsMouse ? "#222736" : "#181C26")
                    border.color: closeMouse.containsMouse ? "#475569" : "#2A3142"
                    border.width: 1

                    Behavior on color { ColorAnimation { duration: 120 } }
                    Behavior on border.color { ColorAnimation { duration: 120 } }

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
                        onClicked: whatsNewRoot.closeDialog()
                    }
                }
            }

            // =================================================================
            // 2. TÍTULO Y DESCRIPCIÓN PRINCIPAL
            // =================================================================
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4

                Text {
                    text: whatsNewRoot.currentData.title || "¿Qué hay de nuevo en Nova?"
                    color: "#F8FAFC"
                    font.pixelSize: 19
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
                    visible: text.length > 0
                }
            }

            // Separador sutil
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: "#1E2433"
            }

            // =================================================================
            // 3. CONTENEDOR DE TARJETAS CON SCROLL Y ALINEACIÓN PERFECTA
            // =================================================================
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                Flickable {
                    id: flickArea
                    anchors.fill: parent
                    contentHeight: contentCol.implicitHeight + 4
                    contentWidth: width
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
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        width: 4
                        policy: (flickArea.contentHeight > flickArea.height) ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
                        contentItem: Rectangle {
                            implicitWidth: 4
                            radius: 2
                            color: vScroll.pressed ? "#60A5FA" : "#3B4458"
                            opacity: 0.85
                        }
                    }

                    Column {
                        id: contentCol
                        width: flickArea.width - (vScroll.visible ? 10 : 0)
                        spacing: 8

                        Repeater {
                            model: whatsNewRoot.featuresList

                            delegate: Rectangle {
                                id: featureCard
                                width: contentCol.width
                                implicitHeight: cardInnerRow.implicitHeight + 20
                                radius: 10
                                color: cardHover.hovered ? "#1C212E" : "#161922"
                                border.color: cardHover.hovered ? (modelData.accentBorder || "#3B82F6") : "#222736"
                                border.width: 1

                                Behavior on color { ColorAnimation { duration: 120 } }
                                Behavior on border.color { ColorAnimation { duration: 120 } }

                                HoverHandler {
                                    id: cardHover
                                }

                                RowLayout {
                                    id: cardInnerRow
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.top: parent.top
                                    anchors.margins: 10
                                    spacing: 12

                                    // Icono estilizado con fondo temático
                                    Rectangle {
                                        Layout.preferredWidth: 36
                                        Layout.preferredHeight: 36
                                        Layout.alignment: Qt.AlignTop
                                        radius: 8
                                        color: modelData.accentBg || "#1E3A5F"
                                        border.color: modelData.accentBorder || "#3B82F6"
                                        border.width: 1

                                        Text {
                                            anchors.centerIn: parent
                                            text: modelData.icon || "✨"
                                            color: modelData.accentText || "#60A5FA"
                                            font.pixelSize: 16
                                        }
                                    }

                                    // Contenido de la mejora
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 4

                                        RowLayout {
                                            Layout.fillWidth: true
                                            spacing: 6

                                            Text {
                                                text: modelData.title || ""
                                                color: "#F8FAFC"
                                                font.pixelSize: 13
                                                font.bold: true
                                                Layout.fillWidth: true
                                                wrapMode: Text.WordWrap
                                                elide: Text.ElideNone
                                            }

                                            // Badge de categoría (NUEVO, MEJORA, CORRECCIÓN)
                                            Rectangle {
                                                Layout.preferredHeight: 18
                                                implicitWidth: badgeLabel.implicitWidth + 12
                                                Layout.alignment: Qt.AlignVCenter
                                                radius: 4
                                                color: modelData.accentBg || "#1E3A5F"
                                                border.color: modelData.accentBorder || "#3B82F6"
                                                border.width: 1

                                                Text {
                                                    id: badgeLabel
                                                    anchors.centerIn: parent
                                                    text: modelData.badge || modelData.category || "MEJORA"
                                                    color: modelData.accentText || "#60A5FA"
                                                    font.pixelSize: 8
                                                    font.bold: true
                                                    font.letterSpacing: 0.4
                                                }
                                            }
                                        }

                                        Text {
                                            text: modelData.description || ""
                                            color: "#94A3B8"
                                            font.pixelSize: 11
                                            Layout.fillWidth: true
                                            wrapMode: Text.WordWrap
                                            lineHeight: 1.25
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
                    anchors.rightMargin: 8
                    height: 14
                    visible: flickArea.contentY > 2
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
                    anchors.rightMargin: 8
                    height: 18
                    visible: (flickArea.contentY + flickArea.height) < (flickArea.contentHeight - 2)
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: "#0013161F" }
                        GradientStop { position: 1.0; color: "#13161F" }
                    }
                }
            }

            // =================================================================
            // 4. PIE DEL MODAL CON BOTÓN DE ACCIÓN PRINCIPAL (CTA)
            // =================================================================
            Rectangle {
                id: ctaBtn
                Layout.fillWidth: true
                Layout.preferredHeight: 42
                radius: 10
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
                    font.pixelSize: 13
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