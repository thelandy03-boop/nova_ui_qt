import QtQuick
import QtQuick.Layouts

/**
 * Activity Bar lateral izquierda estilo VS Code.
 * Proporciona acceso rápido al explorador de archivos, búsqueda, control de versiones,
 * ejecución/depuración y extensiones mediante iconos vectoriales puros.
 */
Rectangle {
    id: root

    property int activeTab: 0 // 0: Explorer, 1: Search, 2: Git, 3: Run, 4: Extensions
    property bool sidebarOpen: true

    signal tabClicked(int tabIndex)

    Layout.preferredWidth: (Qt.platform.os === "android") ? 42 : 38
    Layout.fillHeight: true
    color: "#1E1F24"
    border.color: "#141518"
    border.width: 1

    ColumnLayout {
        anchors.fill: parent
        spacing: 4
        anchors.topMargin: 6
        anchors.bottomMargin: 6

        // 1. Explorador de archivos (Páginas superpuestas)
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: (Qt.platform.os === "android") ? 44 : 38
            color: root.activeTab === 0 && root.sidebarOpen ? "#252832" : "transparent"

            Rectangle {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: 2
                color: "#FFFFFF"
                visible: root.activeTab === 0 && root.sidebarOpen
            }

            Item {
                width: 18; height: 18
                anchors.centerIn: parent
                opacity: (root.activeTab === 0 && root.sidebarOpen) ? 1.0 : 0.6

                Rectangle {
                    x: 1; y: 3; width: 12; height: 14
                    color: "transparent"
                    border.color: "#FFFFFF"
                    border.width: 1.4
                    radius: 1
                }
                Rectangle {
                    x: 4; y: 0; width: 12; height: 14
                    color: "#1E1F24"
                    border.color: "#FFFFFF"
                    border.width: 1.4
                    radius: 1
                }
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: root.tabClicked(0)
            }
        }

        // 2. Búsqueda (Lupa vectorial)
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: (Qt.platform.os === "android") ? 44 : 38
            color: root.activeTab === 1 && root.sidebarOpen ? "#252832" : "transparent"

            Rectangle {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: 2
                color: "#FFFFFF"
                visible: root.activeTab === 1 && root.sidebarOpen
            }

            Item {
                width: 16; height: 16
                anchors.centerIn: parent
                opacity: (root.activeTab === 1 && root.sidebarOpen) ? 1.0 : 0.6

                Rectangle {
                    x: 1; y: 1; width: 9; height: 9
                    radius: 4.5
                    color: "transparent"
                    border.color: "#FFFFFF"
                    border.width: 1.4
                }
                Rectangle {
                    x: 8; y: 8; width: 6; height: 1.8
                    color: "#FFFFFF"
                    rotation: 45
                    transformOrigin: Item.TopLeft
                }
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: root.tabClicked(1)
            }
        }

        // 3. Source Control / Git (Canvas vectorial)
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: (Qt.platform.os === "android") ? 44 : 38
            color: root.activeTab === 2 && root.sidebarOpen ? "#252832" : "transparent"

            Rectangle {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: 2
                color: "#FFFFFF"
                visible: root.activeTab === 2 && root.sidebarOpen
            }

            Canvas {
                width: 16; height: 16
                anchors.centerIn: parent
                opacity: (root.activeTab === 2 && root.sidebarOpen) ? 1.0 : 0.6
                onPaint: {
                    var ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    ctx.strokeStyle = "#FFFFFF"
                    ctx.fillStyle = "#FFFFFF"
                    ctx.lineWidth = 1.4

                    ctx.beginPath()
                    ctx.moveTo(4, 5)
                    ctx.lineTo(4, 12)
                    ctx.stroke()

                    ctx.beginPath()
                    ctx.moveTo(4, 10)
                    ctx.bezierCurveTo(4, 7, 12, 7, 12, 5)
                    ctx.stroke()

                    ctx.beginPath()
                    ctx.arc(4, 12, 1.8, 0, Math.PI * 2)
                    ctx.fill()

                    ctx.beginPath()
                    ctx.arc(4, 4, 1.8, 0, Math.PI * 2)
                    ctx.fill()

                    ctx.beginPath()
                    ctx.arc(12, 4, 1.8, 0, Math.PI * 2)
                    ctx.fill()
                }
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: root.tabClicked(2)
            }
        }

        // 4. Run / Debug (Play contorno vectorial)
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: (Qt.platform.os === "android") ? 44 : 38
            color: root.activeTab === 3 && root.sidebarOpen ? "#252832" : "transparent"

            Rectangle {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: 2
                color: "#FFFFFF"
                visible: root.activeTab === 3 && root.sidebarOpen
            }

            Canvas {
                width: 15; height: 15
                anchors.centerIn: parent
                opacity: (root.activeTab === 3 && root.sidebarOpen) ? 1.0 : 0.6
                onPaint: {
                    var ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    ctx.strokeStyle = "#FFFFFF"
                    ctx.lineWidth = 1.4
                    ctx.beginPath()
                    ctx.moveTo(3, 2)
                    ctx.lineTo(12, 7.5)
                    ctx.lineTo(3, 13)
                    ctx.closePath()
                    ctx.stroke()
                }
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: root.tabClicked(3)
            }
        }

        // 5. Extensiones (4 cubos modulares de VS Code)
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: (Qt.platform.os === "android") ? 44 : 38
            color: root.activeTab === 4 && root.sidebarOpen ? "#252832" : "transparent"

            Rectangle {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: 2
                color: "#FFFFFF"
                visible: root.activeTab === 4 && root.sidebarOpen
            }

            Item {
                width: 15; height: 15
                anchors.centerIn: parent
                opacity: (root.activeTab === 4 && root.sidebarOpen) ? 1.0 : 0.6

                Rectangle { x: 1; y: 1; width: 5.5; height: 5.5; color: "transparent"; border.color: "#FFFFFF"; border.width: 1.3; radius: 1 }
                Rectangle { x: 1; y: 8.5; width: 5.5; height: 5.5; color: "transparent"; border.color: "#FFFFFF"; border.width: 1.3; radius: 1 }
                Rectangle { x: 8.5; y: 8.5; width: 5.5; height: 5.5; color: "transparent"; border.color: "#FFFFFF"; border.width: 1.3; radius: 1 }
                Rectangle { x: 8.5; y: 0; width: 5.5; height: 5.5; color: "transparent"; border.color: "#FFFFFF"; border.width: 1.3; radius: 1 }
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: root.tabClicked(4)
            }
        }

        Item { Layout.fillHeight: true }
    }
}
