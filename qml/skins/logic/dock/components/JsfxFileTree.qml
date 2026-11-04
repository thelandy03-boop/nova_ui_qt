import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

/**
 * Árbol de proyectos y archivos JSFX para el explorador lateral.
 * Muestra las carpetas y archivos con insignias JS vectoriales.
 */
Rectangle {
    id: root

    property string selectedFile: "simple_delay.jsfx"

    signal fileSelected(string fileName)

    color: "#181A20"
    border.color: "#141518"
    border.width: 1

    ScrollView {
        anchors.fill: parent
        clip: true
        contentWidth: availableWidth

        ColumnLayout {
            width: parent.width
            spacing: 2
            anchors.topMargin: 6
            anchors.bottomMargin: 6

            // Cabecera de sección: PROJECTS
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 22
                color: "transparent"

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    spacing: 6

                    Canvas {
                        width: 8; height: 8
                        Layout.alignment: Qt.AlignVCenter
                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.clearRect(0, 0, 8, 8)
                            ctx.strokeStyle = "#8F94A0"
                            ctx.lineWidth = 1.3
                            ctx.beginPath()
                            ctx.moveTo(1.5, 2.5)
                            ctx.lineTo(4, 5.5)
                            ctx.lineTo(6.5, 2.5)
                            ctx.stroke()
                        }
                    }
                    Text {
                        text: "PROJECTS"
                        color: "#8F94A0"
                        font.pixelSize: 11
                        font.bold: true
                        font.family: "Segoe UI, Inter, Sans-serif"
                    }
                }
            }

            // Carpeta raíz: jsfx_effects
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 22
                color: "transparent"

                Row {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    spacing: 6

                    Canvas {
                        width: 8; height: 8
                        anchors.verticalCenter: parent.verticalCenter
                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.clearRect(0, 0, 8, 8)
                            ctx.strokeStyle = "#8F94A0"
                            ctx.lineWidth = 1.3
                            ctx.beginPath()
                            ctx.moveTo(1.5, 2.5)
                            ctx.lineTo(4, 5.5)
                            ctx.lineTo(6.5, 2.5)
                            ctx.stroke()
                        }
                    }

                    // Icono vectorial de carpeta dorada
                    Item {
                        width: 14; height: 11
                        anchors.verticalCenter: parent.verticalCenter
                        Rectangle { x: 0; y: 0; width: 6; height: 3; color: "#D4B16A"; radius: 1 }
                        Rectangle { x: 0; y: 2; width: 14; height: 9; color: "#E5C07B"; radius: 1.5 }
                    }

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: "jsfx_effects"
                        color: "#CCCCCC"
                        font.pixelSize: 11
                        font.family: "Segoe UI, Inter, Sans-serif"
                    }
                }
            }

            // Lista de archivos JSFX
            Repeater {
                model: [
                    { name: "chorus.jsfx",        cat: "Mod" },
                    { name: "distortion.jsfx",    cat: "Distortion" },
                    { name: "lfo_filter.jsfx",     cat: "Filter" },
                    { name: "simple_delay.jsfx",  cat: "Delay" },
                    { name: "multi_tap.jsfx",     cat: "Delay" },
                    { name: "reverb.jsfx",        cat: "Space" },
                    { name: "phaser_stereo.jsfx", cat: "Mod" },
                    { name: "utils.jsh",          cat: "Utility" }
                ]

                delegate: Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 22
                    color: root.selectedFile === modelData.name ? "#2A2D37" : (fileMouse.containsMouse ? "#20232B" : "transparent")

                    Row {
                        anchors.fill: parent
                        anchors.leftMargin: 26
                        anchors.rightMargin: 8
                        spacing: 6

                        // Insignia vectorial JS
                        Rectangle {
                            width: 12; height: 13
                            anchors.verticalCenter: parent.verticalCenter
                            color: "#1E2026"
                            border.color: root.selectedFile === modelData.name ? "#D4B16A" : "#8F94A0"
                            border.width: 1
                            radius: 1

                            Text {
                                anchors.centerIn: parent
                                text: "JS"
                                color: root.selectedFile === modelData.name ? "#D4B16A" : "#8F94A0"
                                font.pixelSize: 6
                                font.bold: true
                            }
                        }

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width - 26 - 8 - 12 - 6
                            text: modelData.name
                            color: root.selectedFile === modelData.name ? "#FFFFFF" : "#CCCCCC"
                            font.pixelSize: 11
                            font.family: "Monospace, Consolas, Courier New"
                            elide: Text.ElideRight
                        }
                    }

                    MouseArea {
                        id: fileMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            root.fileSelected(modelData.name)
                        }
                    }
                }
            }
        }
    }
}
