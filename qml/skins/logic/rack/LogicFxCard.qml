import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root

    property string title: "EFFECT"
    property string category: "EQ"
    property bool enabledFx: true
    property color accentColor: "#3B82F6"

    signal toggled(bool isEnabled)

    default property alias content: contentSlot.data

    implicitWidth: 190
    implicitHeight: 165
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

            // Interruptor ON / BYPASS
            Rectangle {
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
