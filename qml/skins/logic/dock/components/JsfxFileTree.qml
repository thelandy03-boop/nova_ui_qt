import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

/**
 * Árbol de proyectos y archivos JSFX para el explorador lateral.
 * Permite crear, renombrar y eliminar archivos y carpetas,
 * así como compactar/colapsar carpetas con un solo clic.
 */
Rectangle {
    id: root

    property string selectedFile: "simple_delay.jsfx"
    property int renamingIndex: -1
    property int renamingFolderIndex: -1
    property string activeFolderId: "effects"

    signal fileSelected(string fileName)
    signal fileRenamed(string oldName, string newName)
    signal fileCreated(string fileName)
    signal fileDeleted(string fileName)

    color: "#181A20"
    border.color: "#141518"
    border.width: 1

    // Modelo de carpetas
    ListModel {
        id: foldersModel
        ListElement { folderId: "effects"; name: "jsfx_effects"; collapsed: false }
        ListElement { folderId: "mod";     name: "modulation";   collapsed: false }
        ListElement { folderId: "utils";   name: "utilities";    collapsed: false }
    }

    // Modelo de archivos
    ListModel {
        id: filesModel
        ListElement { name: "simple_delay.jsfx";  baseName: "simple_delay";  ext: ".jsfx"; folderId: "effects" }
        ListElement { name: "multi_tap.jsfx";     baseName: "multi_tap";     ext: ".jsfx"; folderId: "effects" }
        ListElement { name: "reverb.jsfx";        baseName: "reverb";        ext: ".jsfx"; folderId: "effects" }
        ListElement { name: "distortion.jsfx";    baseName: "distortion";    ext: ".jsfx"; folderId: "effects" }
        ListElement { name: "chorus.jsfx";        baseName: "chorus";        ext: ".jsfx"; folderId: "mod" }
        ListElement { name: "phaser_stereo.jsfx"; baseName: "phaser_stereo"; ext: ".jsfx"; folderId: "mod" }
        ListElement { name: "lfo_filter.jsfx";     baseName: "lfo_filter";     ext: ".jsfx"; folderId: "mod" }
        ListElement { name: "utils.jsh";          baseName: "utils";          ext: ".jsh";  folderId: "utils" }
    }

    // Cuenta cuántos archivos tiene una carpeta
    function countFilesInFolder(fId) {
        var count = 0
        for (var i = 0; i < filesModel.count; ++i) {
            if (filesModel.get(i).folderId === fId) count++
        }
        return count
    }

    // Compactar / Expandir todas las carpetas (Collapse All)
    function toggleCompactAllFolders() {
        cancelRename()
        var anyOpen = false
        for (var i = 0; i < foldersModel.count; ++i) {
            if (!foldersModel.get(i).collapsed) {
                anyOpen = true
                break
            }
        }
        for (var j = 0; j < foldersModel.count; ++j) {
            foldersModel.setProperty(j, "collapsed", anyOpen)
        }
    }

    // Crear un nuevo archivo en la carpeta activa
    function createNewFile(targetFolderId) {
        cancelRename()
        var fId = targetFolderId || root.activeFolderId || "effects"

        // Asegurarse de que la carpeta esté expandida
        for (var f = 0; f < foldersModel.count; ++f) {
            if (foldersModel.get(f).folderId === fId) {
                foldersModel.setProperty(f, "collapsed", false)
                break
            }
        }

        // Generar nombre secuencial único
        var base = "nuevo_efecto"
        var candidate = base + ".jsfx"
        var counter = 1
        var exists = true
        while (exists) {
            exists = false
            for (var i = 0; i < filesModel.count; ++i) {
                if (filesModel.get(i).name.toLowerCase() === candidate.toLowerCase()) {
                    counter++
                    candidate = base + "_" + counter + ".jsfx"
                    exists = true
                    break
                }
            }
        }

        var finalBase = candidate.replace(".jsfx", "")
        filesModel.append({
            "name": candidate,
            "baseName": finalBase,
            "ext": ".jsfx",
            "folderId": fId
        })

        root.selectedFile = candidate
        root.fileCreated(candidate)
        root.fileSelected(candidate)

        // Iniciar renombrado del nuevo archivo inmediatamente
        startRename(filesModel.count - 1)
    }

    // Crear una nueva carpeta
    function createNewFolder() {
        cancelRename()
        var base = "carpeta"
        var candidate = base
        var counter = 1
        var exists = true
        while (exists) {
            exists = false
            for (var i = 0; i < foldersModel.count; ++i) {
                if (foldersModel.get(i).name.toLowerCase() === candidate.toLowerCase()) {
                    counter++
                    candidate = base + "_" + counter
                    exists = true
                    break
                }
            }
        }

        var newId = "f_" + Date.now()
        foldersModel.append({
            "folderId": newId,
            "name": candidate,
            "collapsed": false
        })
        root.activeFolderId = newId
        root.renamingFolderIndex = foldersModel.count - 1
    }

    // Eliminar archivo
    function deleteFile(idx) {
        if (idx < 0 || idx >= filesModel.count) return
        if (filesModel.count <= 1) return // Mantener al menos un archivo

        var deletedName = filesModel.get(idx).name
        cancelRename()
        filesModel.remove(idx)

        // Si eliminamos el seleccionado, seleccionar el primero disponible
        if (root.selectedFile === deletedName) {
            var nextName = filesModel.get(Math.min(idx, filesModel.count - 1)).name
            root.selectedFile = nextName
            root.fileSelected(nextName)
        }

        root.fileDeleted(deletedName)
    }

    // Eliminar carpeta
    function deleteFolder(fIdx) {
        if (fIdx < 0 || fIdx >= foldersModel.count) return
        if (foldersModel.count <= 1) return // Mantener al menos una carpeta

        var fId = foldersModel.get(fIdx).folderId
        cancelRename()

        // Mover los archivos de esta carpeta a la primera carpeta disponible
        var targetFId = foldersModel.get(fIdx === 0 ? 1 : 0).folderId
        for (var i = 0; i < filesModel.count; ++i) {
            if (filesModel.get(i).folderId === fId) {
                filesModel.setProperty(i, "folderId", targetFId)
            }
        }

        foldersModel.remove(fIdx)
        if (root.activeFolderId === fId) {
            root.activeFolderId = targetFId
        }
    }

    // Gestión de renombrado de archivos
    function startRename(idx) {
        cancelRename()
        if (idx >= 0 && idx < filesModel.count) {
            renamingIndex = idx
        }
    }

    function cancelRename() {
        renamingIndex = -1
        renamingFolderIndex = -1
    }

    function commitRename(idx, rawBase) {
        if (idx < 0 || idx >= filesModel.count) return
        var item = filesModel.get(idx)
        var oldName = item.name
        var ext = item.ext

        var cleanBase = (rawBase || "").trim().replace(/[\\\/:\*\?"<>\|\s]/g, "_")
        if (cleanBase.length === 0) {
            cleanBase = item.baseName
        }

        var newName = cleanBase + ext
        if (newName !== oldName) {
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

    // Renombrado de carpeta
    function commitFolderRename(fIdx, newName) {
        if (fIdx < 0 || fIdx >= foldersModel.count) return
        var clean = (newName || "").trim().replace(/[\\\/:\*\?"<>\|\s]/g, "_")
        if (clean.length > 0) {
            foldersModel.setProperty(fIdx, "name", clean)
        }
        renamingFolderIndex = -1
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
        MenuSeparator {}
        MenuItem {
            text: "Eliminar archivo"
            onTriggered: {
                if (fileContextMenu.targetIndex >= 0) {
                    root.deleteFile(fileContextMenu.targetIndex)
                }
            }
        }
    }

    // Menú contextual para carpetas
    Menu {
        id: folderContextMenu
        property int folderIndex: -1

        MenuItem {
            text: "Nuevo archivo aquí"
            onTriggered: {
                if (folderContextMenu.folderIndex >= 0) {
                    root.createNewFile(foldersModel.get(folderContextMenu.folderIndex).folderId)
                }
            }
        }
        MenuItem {
            text: "Renombrar carpeta"
            onTriggered: {
                if (folderContextMenu.folderIndex >= 0) {
                    root.cancelRename()
                    root.renamingFolderIndex = folderContextMenu.folderIndex
                }
            }
        }
        MenuSeparator {}
        MenuItem {
            text: "Eliminar carpeta"
            enabled: foldersModel.count > 1
            onTriggered: {
                if (folderContextMenu.folderIndex >= 0) {
                    root.deleteFolder(folderContextMenu.folderIndex)
                }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // 1. Barra de herramientas superior del explorador
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 28
            color: "#14161B"
            border.color: "#1C1F28"
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 6
                spacing: 4

                Text {
                    text: "EXPLORADOR"
                    color: "#8B949E"
                    font.pixelSize: 10
                    font.bold: true
                    font.family: "Segoe UI, Inter, Sans-serif"
                    Layout.alignment: Qt.AlignVCenter
                }

                Item { Layout.fillWidth: true }

                // Botón + Archivo
                Rectangle {
                    width: 22; height: 20
                    radius: 3
                    color: newFileMa.containsMouse ? "#2A2E39" : "transparent"
                    Layout.alignment: Qt.AlignVCenter
                    ToolTip.visible: newFileMa.containsMouse
                    ToolTip.text: "Nuevo archivo JSFX"
                    ToolTip.delay: 300

                    Canvas {
                        width: 12; height: 12
                        anchors.centerIn: parent
                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.clearRect(0, 0, 12, 12)
                            ctx.strokeStyle = newFileMa.containsMouse ? "#E6EDF3" : "#8B949E"
                            ctx.lineWidth = 1.2
                            // Hoja de archivo
                            ctx.strokeRect(1, 1, 7, 10)
                            // Signo +
                            ctx.beginPath()
                            ctx.moveTo(9.5, 4.5); ctx.lineTo(9.5, 8.5)
                            ctx.moveTo(7.5, 6.5); ctx.lineTo(11.5, 6.5)
                            ctx.stroke()
                        }
                    }

                    MouseArea {
                        id: newFileMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.createNewFile(root.activeFolderId)
                    }
                }

                // Botón + Carpeta
                Rectangle {
                    width: 22; height: 20
                    radius: 3
                    color: newFolderMa.containsMouse ? "#2A2E39" : "transparent"
                    Layout.alignment: Qt.AlignVCenter
                    ToolTip.visible: newFolderMa.containsMouse
                    ToolTip.text: "Nueva carpeta"
                    ToolTip.delay: 300

                    Canvas {
                        width: 12; height: 12
                        anchors.centerIn: parent
                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.clearRect(0, 0, 12, 12)
                            ctx.strokeStyle = newFolderMa.containsMouse ? "#E6EDF3" : "#8B949E"
                            ctx.lineWidth = 1.2
                            // Carpeta
                            ctx.beginPath()
                            ctx.moveTo(1, 3); ctx.lineTo(4, 3); ctx.lineTo(5.5, 4.5); ctx.lineTo(10.5, 4.5); ctx.lineTo(10.5, 10.5); ctx.lineTo(1, 10.5); ctx.closePath()
                            ctx.stroke()
                            // Signo +
                            ctx.beginPath()
                            ctx.moveTo(5.5, 6.5); ctx.lineTo(5.5, 9.5)
                            ctx.moveTo(4, 8); ctx.lineTo(7, 8)
                            ctx.stroke()
                        }
                    }

                    MouseArea {
                        id: newFolderMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.createNewFolder()
                    }
                }

                // Botón Compactar Folders (Collapse All)
                Rectangle {
                    width: 22; height: 20
                    radius: 3
                    color: compactMa.containsMouse ? "#2A2E39" : "transparent"
                    Layout.alignment: Qt.AlignVCenter
                    ToolTip.visible: compactMa.containsMouse
                    ToolTip.text: "Compactar / Colapsar carpetas"
                    ToolTip.delay: 300

                    Canvas {
                        width: 12; height: 12
                        anchors.centerIn: parent
                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.clearRect(0, 0, 12, 12)
                            ctx.strokeStyle = compactMa.containsMouse ? "#E6EDF3" : "#8B949E"
                            ctx.lineWidth = 1.2
                            // Flecha colapsar superior hacia abajo
                            ctx.beginPath()
                            ctx.moveTo(2, 2); ctx.lineTo(6, 5); ctx.lineTo(10, 2)
                            ctx.stroke()
                            // Flecha colapsar inferior hacia arriba
                            ctx.beginPath()
                            ctx.moveTo(2, 10); ctx.lineTo(6, 7); ctx.lineTo(10, 10)
                            ctx.stroke()
                        }
                    }

                    MouseArea {
                        id: compactMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.toggleCompactAllFolders()
                    }
                }
            }
        }

        // 2. Lista scrolleable de carpetas y archivos
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth

            MouseArea {
                anchors.fill: parent
                z: -1
                onClicked: root.cancelRename()
            }

            Column {
                width: parent.width
                spacing: 2
                topPadding: 4
                bottomPadding: 6

                // Iteración por cada carpeta
                Repeater {
                    model: foldersModel

                    delegate: Column {
                        id: folderColumn
                        width: parent.width
                        property string curFolderId: model.folderId
                        property bool isCollapsed: model.collapsed
                        property int folderIdx: index

                        // Fila de la Carpeta
                        Rectangle {
                            width: parent.width
                            height: 24
                            color: folderMa.containsMouse ? "#20232B" : "transparent"

                            Row {
                                anchors.fill: parent
                                anchors.leftMargin: 8
                                anchors.rightMargin: 6
                                spacing: 6

                                // Chevron interactivo
                                Canvas {
                                    width: 8; height: 8
                                    anchors.verticalCenter: parent.verticalCenter
                                    onPaint: {
                                        var ctx = getContext("2d")
                                        ctx.clearRect(0, 0, 8, 8)
                                        ctx.strokeStyle = "#8F94A0"
                                        ctx.lineWidth = 1.3
                                        ctx.beginPath()
                                        if (folderColumn.isCollapsed) {
                                            // Chevron hacia la derecha (colapsado)
                                            ctx.moveTo(2.5, 1.5)
                                            ctx.lineTo(5.5, 4)
                                            ctx.lineTo(2.5, 6.5)
                                        } else {
                                            // Chevron hacia abajo (expandido)
                                            ctx.moveTo(1.5, 2.5)
                                            ctx.lineTo(4, 5.5)
                                            ctx.lineTo(6.5, 2.5)
                                        }
                                        ctx.stroke()
                                    }

                                    Connections {
                                        target: model
                                        function onCollapsedChanged() { parent.requestPaint() }
                                    }
                                }

                                // Icono vectorial de carpeta dorada
                                Item {
                                    width: 14; height: 11
                                    anchors.verticalCenter: parent.verticalCenter
                                    Rectangle { x: 0; y: 0; width: 6; height: 3; color: "#D4B16A"; radius: 1 }
                                    Rectangle { x: 0; y: 2; width: 14; height: 9; color: "#E5C07B"; radius: 1.5 }
                                }

                                // Nombre de la carpeta o TextInput si se está renombrando
                                Loader {
                                    anchors.verticalCenter: parent.verticalCenter
                                    sourceComponent: (root.renamingFolderIndex === folderColumn.folderIdx) ? folderRenameComp : folderTextComp
                                }

                                Component {
                                    id: folderTextComp
                                    Row {
                                        spacing: 4
                                        Text {
                                            text: model.name
                                            color: "#CCCCCC"
                                            font.pixelSize: 11
                                            font.bold: true
                                            font.family: "Segoe UI, Inter, Sans-serif"
                                            anchors.verticalCenter: parent.verticalCenter
                                        }
                                        Text {
                                            text: "(" + root.countFilesInFolder(folderColumn.curFolderId) + ")"
                                            color: "#6E7681"
                                            font.pixelSize: 10
                                            font.family: "Segoe UI, Inter, Sans-serif"
                                            anchors.verticalCenter: parent.verticalCenter
                                        }
                                    }
                                }

                                Component {
                                    id: folderRenameComp
                                    TextInput {
                                        width: 90
                                        color: "#FFFFFF"
                                        font.pixelSize: 11
                                        font.family: "Segoe UI, Inter, Sans-serif"
                                        text: model.name
                                        selectByMouse: true
                                        Component.onCompleted: {
                                            forceActiveFocus()
                                            selectAll()
                                        }
                                        Keys.onReturnPressed: root.commitFolderRename(folderColumn.folderIdx, text)
                                        Keys.onEnterPressed: root.commitFolderRename(folderColumn.folderIdx, text)
                                        Keys.onEscapePressed: root.cancelRename()
                                    }
                                }
                            }

                            MouseArea {
                                id: folderMa
                                anchors.fill: parent
                                hoverEnabled: true
                                acceptedButtons: Qt.LeftButton | Qt.RightButton
                                cursorShape: Qt.PointingHandCursor
                                onClicked: (mouse) => {
                                    root.activeFolderId = folderColumn.curFolderId
                                    if (mouse.button === Qt.RightButton) {
                                        folderContextMenu.folderIndex = folderColumn.folderIdx
                                        folderContextMenu.popup()
                                    } else {
                                        foldersModel.setProperty(folderColumn.folderIdx, "collapsed", !folderColumn.isCollapsed)
                                    }
                                }
                            }
                        }

                        // Lista de archivos hijos dentro de la carpeta (visible sólo si NO está colapsada)
                        Column {
                            width: parent.width
                            visible: !folderColumn.isCollapsed

                            Repeater {
                                model: filesModel

                                delegate: Rectangle {
                                    id: fileDelegate
                                    width: parent.width
                                    height: (model.folderId === folderColumn.curFolderId) ? 24 : 0
                                    visible: model.folderId === folderColumn.curFolderId
                                    color: root.selectedFile === model.name ? "#2A2D37" : (fileMouse.containsMouse ? "#20232B" : "transparent")

                                    // 1. Modo Normal
                                    Row {
                                        anchors.fill: parent
                                        anchors.leftMargin: 24
                                        anchors.rightMargin: 6
                                        spacing: 6
                                        visible: root.renamingIndex !== index

                                        // Insignia JS
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

                                        // Nombre del archivo
                                        Text {
                                            width: parent.width - 24 - 44
                                            anchors.verticalCenter: parent.verticalCenter
                                            text: model.name
                                            color: root.selectedFile === model.name ? "#FFFFFF" : "#CCCCCC"
                                            font.pixelSize: 11
                                            font.family: "Segoe UI, Inter, Sans-serif"
                                            elide: Text.ElideRight
                                        }

                                        // Botón lápiz renombrar (Android / Hover)
                                        Rectangle {
                                            width: 16; height: 16
                                            anchors.verticalCenter: parent.verticalCenter
                                            visible: (Qt.platform.os === "android") || fileMouse.containsMouse
                                            color: renameBtnMouse.containsMouse ? "#30363D" : "transparent"
                                            radius: 2

                                            Canvas {
                                                width: 10; height: 10
                                                anchors.centerIn: parent
                                                onPaint: {
                                                    var ctx = getContext("2d")
                                                    ctx.clearRect(0, 0, 10, 10)
                                                    ctx.strokeStyle = renameBtnMouse.containsMouse ? "#58A6FF" : "#8B949E"
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

                                        // Botón papelera eliminar (Android / Hover)
                                        Rectangle {
                                            width: 16; height: 16
                                            anchors.verticalCenter: parent.verticalCenter
                                            visible: ((Qt.platform.os === "android") || fileMouse.containsMouse) && filesModel.count > 1
                                            color: delBtnMouse.containsMouse ? "#3B1818" : "transparent"
                                            radius: 2

                                            Canvas {
                                                width: 10; height: 10
                                                anchors.centerIn: parent
                                                onPaint: {
                                                    var ctx = getContext("2d")
                                                    ctx.clearRect(0, 0, 10, 10)
                                                    ctx.strokeStyle = delBtnMouse.containsMouse ? "#F85149" : "#8B949E"
                                                    ctx.lineWidth = 1.1
                                                    ctx.beginPath()
                                                    ctx.moveTo(2, 2.5); ctx.lineTo(8, 2.5)
                                                    ctx.moveTo(3.5, 2.5); ctx.lineTo(3.5, 1.5); ctx.lineTo(6.5, 1.5); ctx.lineTo(6.5, 2.5)
                                                    ctx.moveTo(3, 3.5); ctx.lineTo(3.5, 8.5); ctx.lineTo(6.5, 8.5); ctx.lineTo(7, 3.5)
                                                    ctx.stroke()
                                                }
                                            }

                                            MouseArea {
                                                id: delBtnMouse
                                                anchors.fill: parent
                                                hoverEnabled: true
                                                cursorShape: Qt.PointingHandCursor
                                                ToolTip.visible: containsMouse
                                                ToolTip.text: "Eliminar archivo"
                                                ToolTip.delay: 300
                                                onClicked: root.deleteFile(index)
                                            }
                                        }
                                    }

                                    // 2. Modo Edición Inline (Renombrar con extensión intocable)
                                    Row {
                                        anchors.fill: parent
                                        anchors.leftMargin: 22
                                        anchors.rightMargin: 4
                                        spacing: 4
                                        visible: root.renamingIndex === index

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

                                        Rectangle {
                                            anchors.verticalCenter: parent.verticalCenter
                                            width: (Qt.platform.os === "android") ? Math.max(70, fileDelegate.width - 22 - 12 - 4 - 44) : Math.max(70, fileDelegate.width - 22 - 12 - 4 - 8)
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

                                        // Botones Android Confirmar / Cancelar
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

                                    // MouseArea del archivo
                                    MouseArea {
                                        id: fileMouse
                                        anchors.fill: parent
                                        enabled: root.renamingIndex !== index
                                        hoverEnabled: true
                                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                                        cursorShape: Qt.PointingHandCursor

                                        onClicked: (mouse) => {
                                            root.cancelRename()
                                            root.activeFolderId = model.folderId
                                            if (mouse.button === Qt.RightButton) {
                                                fileContextMenu.targetIndex = index
                                                fileContextMenu.popup()
                                            } else {
                                                root.fileSelected(model.name)
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
