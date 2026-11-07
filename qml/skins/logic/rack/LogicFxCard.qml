import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    property string title: "EFFECT"
    property string category: "EQ"
    property bool enabledFx: true
    property color accentColor: "#3B82F6"
    property bool isJsfx: false
    property bool showHeaderActions: true
    property bool hasGfx: false
    property bool showGfxView: hasGfx
    property int preferredGfxWidth: 360
    property int preferredGfxHeight: 220

    signal toggled(bool isEnabled)
    signal removeRequested()
    signal editCodeRequested()
    signal resetRequested()

    default property alias content: contentSlot.data

    implicitWidth: root.showGfxView ? Math.max(340, root.preferredGfxWidth + 20) : Math.max(190, contentSlot.childrenRect.width + 24)
    implicitHeight: root.showGfxView ? Math.max(220, root.preferredGfxHeight + 46) : 165
    radius: 10
    color: "#161820"
    border.color: root.enabledFx ? "#2A2E3D" : "#1F212A"
    border.width: 1

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 6

        // Encabezado del efecto
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 24
            spacing: 6

            // Badge de categoría
            Rectangle {
                Layout.preferredHeight: 18
                Layout.preferredWidth: catText.implicitWidth + 8
                radius: 4
                color: root.enabledFx ? Qt.rgba(root.accentColor.r, root.accentColor.g, root.accentColor.b, 0.2) : "#20232C"
                border.color: root.enabledFx ? root.accentColor : "#333846"
                border.width: 1

                Text {
                    id: catText
                    anchors.centerIn: parent
                    text: root.category
                    color: root.enabledFx ? root.accentColor : "#64748B"
                    font.pixelSize: 8
                    font.bold: true
                }
            }

            // Nombre del plugin
            Text {
                Layout.fillWidth: true
                text: root.title
                color: root.enabledFx ? "#F1F5F9" : "#64748B"
                font.pixelSize: 11
                font.bold: true
                elide: Text.ElideRight
            }

            // Botón conmutador Gráficos / Controles
            Rectangle {
                visible: root.hasGfx && root.showHeaderActions
                Layout.preferredHeight: 18
                Layout.preferredWidth: gfxToggleText.implicitWidth + 12
                radius: 4
                color: root.showGfxView ? Qt.rgba(root.accentColor.r, root.accentColor.g, root.accentColor.b, 0.25) : "#20232C"
                border.color: root.showGfxView ? root.accentColor : "#333846"
                border.width: 1

                Text {
                    id: gfxToggleText
                    anchors.centerIn: parent
                    text: root.showGfxView ? "Gráficos" : "Controles"
                    color: root.showGfxView ? root.accentColor : "#94A3B8"
                    font.pixelSize: 8
                    font.bold: true
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (typeof AudioEngine !== "undefined" && AudioEngine.hapticFeedback) {
                            AudioEngine.hapticFeedback()
                        }
                        root.showGfxView = !root.showGfxView
                    }
                }
            }

            // Botón de opciones (3 puntos ⋮) con submenú contextual
            Rectangle {
                id: moreBtn
                visible: root.showHeaderActions
                Layout.preferredWidth: 22
                Layout.preferredHeight: 18
                radius: 4
                color: moreArea.containsMouse ? "#2A2E3D" : "#20232C"
                border.color: moreArea.containsMouse ? "#3E4459" : "#2A2E3D"
                border.width: 1

                Text {
                    anchors.centerIn: parent
                    text: "⋮"
                    color: moreArea.containsMouse ? "#FFFFFF" : "#94A3B8"
                    font.pixelSize: 13
                    font.bold: true
                }

                MouseArea {
                    id: moreArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (typeof AudioEngine !== "undefined" && AudioEngine.hapticFeedback) {
                            AudioEngine.hapticFeedback()
                        }
                        optionsMenu.open()
                    }
                }

                Menu {
                    id: optionsMenu
                    y: moreBtn.height + 4
                    x: -80

                    MenuItem {
                        text: "Ver / Editar código JSFX"
                        visible: root.isJsfx
                        onTriggered: root.editCodeRequested()
                    }

                    MenuItem {
                        text: root.showGfxView ? "Mostrar controles deslizantes" : "Mostrar interfaz gráfica"
                        visible: root.hasGfx
                        onTriggered: root.showGfxView = !root.showGfxView
                    }

                    MenuItem {
                        text: "Restablecer parámetros"
                        onTriggered: root.resetRequested()
                    }

                    MenuSeparator {
                        visible: root.isJsfx
                    }

                    MenuItem {
                        text: "Eliminar plug-in"
                        onTriggered: root.removeRequested()
                    }
                }
            }

            // Interruptor ON / BYPASS
            Rectangle {
                visible: root.showHeaderActions
                Layout.preferredWidth: 32
                Layout.preferredHeight: 18
                radius: 9
                color: root.enabledFx ? "#10B981" : "#334155"

                // Indicador circular
                Rectangle {
                    x: root.enabledFx ? 16 : 2
                    y: 2
                    width: 14
                    height: 14
                    radius: 7
                    color: "#FFFFFF"

                    Behavior on x {
                        NumberAnimation { duration: 120 }
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    anchors.margins: -4
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (typeof AudioEngine !== "undefined" && AudioEngine.hapticFeedback) {
                            AudioEngine.hapticFeedback()
                        }
                        root.enabledFx = !root.enabledFx
                        root.toggled(root.enabledFx)
                    }
                }
            }
        }

        // Separador sutil
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: "#222530"
        }

        // Zona de controles del efecto
        Item {
            id: contentSlot
            Layout.fillWidth: true
            Layout.fillHeight: true
            opacity: root.enabledFx ? 1.0 : 0.4

            Behavior on opacity {
                NumberAnimation { duration: 120 }
            }
        }
    }
}
