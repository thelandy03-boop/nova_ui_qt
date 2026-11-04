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
    property int renamingIndex: -1

    signal fileSelected(string fileName)
    signal fileRenamed(string oldName, string newName)

    ListModel {
        id: filesModel
        ListElement { name: "chorus.jsfx";        baseName: "chorus";        ext: ".jsfx"; cat: "Mod" }
        ListElement { name: "distortion.jsfx";    baseName: "distortion";    ext: ".jsfx"; cat: "Distortion" }
        ListElement { name: "lfo_filter.jsfx";     baseName: "lfo_filter";     ext: ".jsfx"; cat: "Filter" }
        ListElement { name: "simple_delay.jsfx";  baseName: "simple_delay";  ext: ".jsfx"; cat: "Delay" }
        ListElement { name: "multi_tap.jsfx";     baseName: "multi_tap";     ext: ".jsfx"; cat: "Delay" }
        ListElement { name: "reverb.jsfx";        baseName: "reverb";        ext: ".jsfx"; cat: "Space" }
        ListElement { name: "phaser_stereo.jsfx"; baseName: "phaser_stereo"; ext: ".jsfx"; cat: "Mod" }
        ListElement { name: "utils.jsh";          baseName: "utils";          ext: ".jsh";  cat: "Utility" }
    }

    onSelectedFileChanged: {
        if (renamingIndex !== -1 && filesModel.count > renamingIndex) {
            if (filesModel.get(renamingIndex).name !== selectedFile) {
                cancelRename()
            }
        }
    }

    function startRename(idx) {
        if (renamingIndex !== -1 && renamingIndex !== idx) {
            cancelRename()
        }
        if (idx >= 0 && idx < filesModel.count) {
            renamingIndex = idx
        }
    }

    function startRenameSelected() {
        for (var i = 0; i < filesModel.count; ++i) {
            if (filesModel.get(i).name === root.selectedFile) {
                startRename(i)
                return
            }
        }
    }

    function cancelRename() {
        renamingIndex = -1
    }

    function commitRename(idx, rawBase) {
        if (idx < 0 || idx >= filesModel.count) return
        var item = filesModel.get(idx)
        var oldName = item.name
        var ext = item.ext

        // Sanitizar el nombre base: quitar espacios y caracteres no válidos
        var cleanBase = (rawBase || "").trim().replace(/[\\\/:\*\?"<>\|\s]/g, "_")
        if (cleanBase.length === 0) {
            cleanBase = item.baseName
        }

        var newName = cleanBase + ext
        if (newName !== oldName) {
            // Evitar colisión de nombres duplicados
            for (var i = 0; i < filesModel.count; ++i) {
                if (i !== idx && filesModel.get(i).name.toLowerCase() === newName.toLowerCase()) {
                    renamingIndex = -1
                    return
                }
            }

            filesModel.setProperty(idx, "baseName", cleanBase)
            filesModel.setProperty(idx, "name", newName)
            if (root.selectedFile === oldName) {
                root.selectedFile = newName
            }
            root.fileRenamed(oldName, newName)
        }
        renamingIndex = -1
    }

    // Menú contextual para clic derecho en archivos
    Menu {
        id: fileContextMenu
        property int targetIndex: -1

        MenuItem {
            text: "Abrir archivo"
            onTriggered: {
                if (fileContextMenu.targetIndex >= 0) {
                    root.fileSelected(filesModel.get(fileContextMenu.targetIndex).name)
                }
            }
        }
        MenuItem {
            text: "Renombrar (F2)"
            onTriggered: {
                if (fileContextMenu.targetIndex >= 0) {
                    root.startRename(fileContextMenu.targetIndex)
                }
            }
        }
    }

    color: "#181A20"
    border.color: "#141518"
    border.width: 1

    ScrollView {
        anchors.fill: parent
        clip: true
        contentWidth: availableWidth

        MouseArea {
            anchors.fill: parent
            z: -1
            onClicked: {
                if (root.renamingIndex !== -1) root.cancelRename()
            }
        }

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

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        if (root.renamingIndex !== -1) root.cancelRename()
                    }
                }

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

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        if (root.renamingIndex !== -1) root.cancelRename()
                    }
                }

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

            // Lista dinámica de archivos JSFX con soporte de renombrado seguro
            Repeater {
                model: filesModel

                delegate: Rectangle {
                    id: fileDelegate
                    Layout.fillWidth: true
                    Layout.preferredHeight: 24
                    color: root.selectedFile === model.name ? "#2A2D37" : (fileMouse.containsMouse ? "#20232B" : "transparent")

                    // 1. MODO NORMAL (LECTURA / SELECCIÓN)
                    Row {
                        anchors.fill: parent
                        anchors.leftMargin: 26
                        anchors.rightMargin: 6
                        spacing: 6
                        visible: root.renamingIndex !== index

                        // Insignia vectorial JS
                        Rectangle {
                            width: 12; height: 13
                            anchors.verticalCenter: parent.verticalCenter
                            color: "#1E2026"
                            border.color: root.selectedFile === model.name ? "#D4B16A" : "#8F94A0"
                            border.width: 1
                            radius: 1

                            Text {
                                anchors.centerIn: parent
                                text: "JS"
                                color: root.selectedFile === model.name ? "#D4B16A" : "#8F94A0"
                                font.pixelSize: 6
                                font.bold: true
                            }
                        }

                        // Nombre completo natural con extensión ("simple_delay.jsfx")
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            width: (Qt.platform.os === "android") ? (parent.width - 26 - 6 - 12 - 6 - 28) : (parent.width - 26 - 6 - 12 - 6 - 8)
                            text: model.name
                            color: root.selectedFile === model.name ? "#FFFFFF" : "#CCCCCC"
                            font.pixelSize: 11
                            font.family: "Monospace, Consolas, Courier New"
                            elide: Text.ElideRight
                        }

                        // Botón de lápiz para renombrar (exclusivo para Android con interfaz táctil)
                        Rectangle {
                            width: (Qt.platform.os === "android") ? 20 : 0
                            height: (Qt.platform.os === "android") ? 20 : 0
                            anchors.verticalCenter: parent.verticalCenter
                            visible: (Qt.platform.os === "android")
                            color: renameBtnMouse.containsMouse ? "#3A3F4D" : "transparent"
                            radius: 2

                            Canvas {
                                anchors.centerIn: parent
                                width: 10; height: 10
                                onPaint: {
                                    var ctx = getContext("2d")
                                    ctx.clearRect(0, 0, 10, 10)
                                    ctx.strokeStyle = renameBtnMouse.containsMouse ? "#FFFFFF" : "#8F94A0"
                                    ctx.lineWidth = 1.1
                                    ctx.beginPath()
                                    ctx.moveTo(1.5, 8.5)
                                    ctx.lineTo(3.5, 8.5)
                                    ctx.lineTo(8.5, 3.5)
                                    ctx.lineTo(6.5, 1.5)
                                    ctx.lineTo(1.5, 6.5)
                                    ctx.closePath()
                                    ctx.stroke()
                                }
                            }

                            MouseArea {
                                id: renameBtnMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                ToolTip.visible: containsMouse
                                ToolTip.text: "Renombrar archivo (F2)"
                                ToolTip.delay: 300
                                onClicked: root.startRename(index)
                            }
                        }
                    }

                    // 2. MODO EDICIÓN INLINE (RENOMBRAR CON EXTENSIÓN .jsfx DESHABILITADA)
                    Row {
                        anchors.fill: parent
                        anchors.leftMargin: 20
                        anchors.rightMargin: 4
                        spacing: 4
                        visible: root.renamingIndex === index

                        // Insignia vectorial JS
                        Rectangle {
                            width: 12; height: 13
                            anchors.verticalCenter: parent.verticalCenter
                            color: "#1E2026"
                            border.color: "#D4B16A"
                            border.width: 1
                            radius: 1
                            Text {
                                anchors.centerIn: parent
                                text: "JS"
                                color: "#D4B16A"
                                font.pixelSize: 6
                                font.bold: true
                            }
                        }

                        // Caja de entrada con extensión bloqueada
                        Rectangle {
                            anchors.verticalCenter: parent.verticalCenter
                            width: (Qt.platform.os === "android") ? Math.max(80, fileDelegate.width - 20 - 12 - 4 - 44) : Math.max(80, fileDelegate.width - 20 - 12 - 4 - 8)
                            height: 19
                            color: "#121418"
                            border.color: "#007ACC"
                            border.width: 1
                            radius: 2

                            Row {
                                anchors.fill: parent
                                anchors.leftMargin: 4
                                anchors.rightMargin: 4
                                spacing: 1

                                TextInput {
                                    id: renameInput
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: parent.width - extBadge.width - 4
                                    color: "#FFFFFF"
                                    font.pixelSize: 11
                                    font.family: "Monospace, Consolas, Courier New"
                                    selectByMouse: true
                                    text: model.baseName

                                    Component.onCompleted: {
                                        if (root.renamingIndex === index) {
                                            forceActiveFocus()
                                            selectAll()
                                        }
                                    }

                                    Connections {
                                        target: root
                                        function onRenamingIndexChanged() {
                                            if (root.renamingIndex === index) {
                                                renameInput.text = model.baseName
                                                renameInput.forceActiveFocus()
                                                renameInput.selectAll()
                                            }
                                        }
                                    }

                                    Keys.onReturnPressed: root.commitRename(index, renameInput.text)
                                    Keys.onEnterPressed: root.commitRename(index, renameInput.text)
                                    Keys.onEscapePressed: root.cancelRename()
                                }

                                // Extensión .jsfx protegida e intocable
                                Rectangle {
                                    id: extBadge
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: extText.implicitWidth + 4
                                    height: 15
                                    color: "#1E2026"
                                    radius: 2

                                    Text {
                                        id: extText
                                        anchors.centerIn: parent
                                        text: model.ext
                                        color: "#7A8294"
                                        font.pixelSize: 10
                                        font.family: "Monospace, Consolas, Courier New"
                                        font.bold: true
                                    }
                                }
                            }
                        }

                        // Botón confirmar renombrado (✓) - exclusivo para Android
                        Rectangle {
                            width: (Qt.platform.os === "android") ? 18 : 0
                            height: (Qt.platform.os === "android") ? 18 : 0
                            anchors.verticalCenter: parent.verticalCenter
                            visible: (Qt.platform.os === "android")
                            color: okMouse.containsMouse ? "#2A5B36" : "#1B3B23"
                            radius: 2
                            Text {
                                anchors.centerIn: parent
                                text: "✓"
                                color: "#89D185"
                                font.pixelSize: 11
                                font.bold: true
                            }
                            MouseArea {
                                id: okMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.commitRename(index, renameInput.text)
                            }
                        }

                        // Botón cancelar renombrado (✕) - exclusivo para Android
                        Rectangle {
                            width: (Qt.platform.os === "android") ? 18 : 0
                            height: (Qt.platform.os === "android") ? 18 : 0
                            anchors.verticalCenter: parent.verticalCenter
                            visible: (Qt.platform.os === "android")
                            color: cancelMouse.containsMouse ? "#5A1D1D" : "#3B1818"
                            radius: 2
                            Text {
                                anchors.centerIn: parent
                                text: "✕"
                                color: "#F14C4C"
                                font.pixelSize: 10
                            }
                            MouseArea {
                                id: cancelMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.cancelRename()
                            }
                        }
                    }

                    // MouseArea general para el archivo
                    MouseArea {
                        id: fileMouse
                        anchors.fill: parent
                        enabled: root.renamingIndex !== index
                        hoverEnabled: true
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        cursorShape: Qt.PointingHandCursor

                        onClicked: (mouse) => {
                            if (root.renamingIndex !== -1) {
                                root.cancelRename()
                            }
                            if (mouse.button === Qt.RightButton) {
                                fileContextMenu.targetIndex = index
                                fileContextMenu.popup()
                            } else {
                                root.fileSelected(model.name)
                            }
                        }

                        onDoubleClicked: (mouse) => {
                            if (mouse.button === Qt.LeftButton) {
                                root.fileSelected(model.name)
                            }
                        }
                    }
                }
            }
        }
    }
}
