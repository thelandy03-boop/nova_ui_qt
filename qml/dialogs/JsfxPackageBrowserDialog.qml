import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

/**
 * Explorador y Gestor de Paquetes JSFX estilo ReaPack para NOVA Studio.
 * Permite explorar, buscar e instalar plugins JSFX de la comunidad (ReaTeam)
 * con 1 solo clic y configurar repositorios personalizados vía URL.
 */
Rectangle {
    id: root
    anchors.fill: parent
    color: "#D007090E"
    z: 100000
    visible: false
    focus: visible

    property string selectedCategory: "Todas"
    property string searchQuery: ""
    property bool onlyInstalled: false
    property var packageList: []
    property bool showRepoConfig: false
    property string lastInstalledNotice: ""

    Timer {
        id: noticeTimer
        interval: 6000
        repeat: false
        onTriggered: root.lastInstalledNotice = ""
    }

    signal openCodeRequested(string code, string name)

    function open() {
        root.visible = true
        root.forceActiveFocus()
        refreshPackages()
        // Solo sincronizar si hay repositorios configurados y la lista está vacía
        if (typeof AudioEngine !== "undefined" && AudioEngine.packages) {
            if (AudioEngine.packages.repositories.length > 0 && AudioEngine.packages.totalPackagesCount === 0) {
                AudioEngine.packages.syncRepositories()
            }
        }
    }

    function close() {
        root.visible = false
    }

    function refreshPackages() {
        if (typeof AudioEngine !== "undefined" && AudioEngine.packages) {
            root.packageList = AudioEngine.packages.searchPackages(root.searchQuery, root.selectedCategory, root.onlyInstalled)
        }
    }

    Keys.onBackPressed: (event) => {
        close()
        event.accepted = true
    }
    Keys.onEscapePressed: close()

    Connections {
        target: (typeof AudioEngine !== "undefined" && AudioEngine.packages) ? AudioEngine.packages : null

        function onPackageListChanged() {
            root.refreshPackages()
        }
        function onSyncingChanged(isSyncing) {
            if (!isSyncing) root.refreshPackages()
        }
        function onPackageInstallFinished(packageId, success, error) {
            if (success) {
                root.lastInstalledNotice = "✅ ¡Plugin instalado! Ya puedes encontrarlo e insertarlo en el rack de efectos de cualquier pista (+ Añadir Plug-in)."
                noticeTimer.restart()
            } else if (error && error.length > 0) {
                root.lastInstalledNotice = "❌ Error al instalar plugin: " + error
                noticeTimer.restart()
            }
        }
    }

    // Tarjeta principal del diálogo
    Rectangle {
        id: dialogCard
        width: Math.min(parent.width - (Qt.platform.os === "android" ? 16 : 48), 860)
        height: Math.min(parent.height - (Qt.platform.os === "android" ? 16 : 48), 640)
        anchors.centerIn: parent
        color: "#16181F"
        border.color: "#282C3A"
        border.width: 1
        radius: 8
        clip: true

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 12

            // 1. CABECERA: Título, estado y acciones
            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                // Icono
                Rectangle {
                    width: 34; height: 34; radius: 6
                    color: "#1E293B"
                    border.color: "#3B82F6"; border.width: 1
                    Text { anchors.centerIn: parent; text: "📦"; font.pixelSize: 18 }
                }

                Column {
                    Layout.fillWidth: true
                    spacing: 2
                    Text {
                        text: "Explorador de Paquetes JSFX (ReaPack)"
                        color: "#FFFFFF"
                        font.pixelSize: 15
                        font.bold: true
                        font.family: "Segoe UI, Inter, Sans-serif"
                    }
                    Text {
                        text: {
                            if (typeof AudioEngine === "undefined" || !AudioEngine.packages) return ""
                            if (AudioEngine.packages.isSyncing) return AudioEngine.packages.statusMessage
                            if (AudioEngine.packages.repositories.length === 0) {
                                return "Sin repositorios configurados"
                            }
                            if (AudioEngine.packages.statusMessage && AudioEngine.packages.statusMessage.indexOf("Error") !== -1) {
                                return AudioEngine.packages.statusMessage
                            }
                            return AudioEngine.packages.totalPackagesCount + " plugins disponibles (" + AudioEngine.packages.repositories.length + (AudioEngine.packages.repositories.length === 1 ? " repositorio)" : " repositorios)")
                        }
                        color: (typeof AudioEngine !== "undefined" && AudioEngine.packages && AudioEngine.packages.statusMessage && AudioEngine.packages.statusMessage.indexOf("Error") !== -1) ? "#F87171" : "#94A3B8"
                        font.pixelSize: 11
                        font.family: "Segoe UI, Inter, Sans-serif"
                        elide: Text.ElideRight
                    }
                }

                // Botón Configurar Repositorios
                Rectangle {
                    Layout.preferredHeight: 30
                    Layout.preferredWidth: repoBtnRow.implicitWidth + 20
                    radius: 4
                    color: repoMouse.containsMouse ? "#2A2E3D" : "#1F232F"
                    border.color: "#383E4F"
                    border.width: 1

                    Row {
                        id: repoBtnRow
                        anchors.centerIn: parent
                        spacing: 6
                        Text { text: "🌐"; font.pixelSize: 12 }
                        Text { text: "Repositorios"; color: "#E2E8F0"; font.pixelSize: 11; font.bold: true }
                    }

                    MouseArea {
                        id: repoMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.showRepoConfig = !root.showRepoConfig
                    }
                }

                // Botón Sincronizar
                Rectangle {
                    Layout.preferredHeight: 30
                    Layout.preferredWidth: syncBtnRow.implicitWidth + 20
                    radius: 4
                    color: (typeof AudioEngine !== "undefined" && AudioEngine.packages && AudioEngine.packages.isSyncing)
                           ? "#1E3A8A"
                           : (syncMouse.containsMouse ? "#2563EB" : "#1D4ED8")

                    Row {
                        id: syncBtnRow
                        anchors.centerIn: parent
                        spacing: 6
                        Text {
                            text: (typeof AudioEngine !== "undefined" && AudioEngine.packages && AudioEngine.packages.isSyncing) ? "⏳" : "🔄"
                            font.pixelSize: 12
                        }
                        Text {
                            text: (typeof AudioEngine !== "undefined" && AudioEngine.packages && AudioEngine.packages.isSyncing) ? "Sincronizando..." : "Sincronizar"
                            color: "#FFFFFF"
                            font.pixelSize: 11
                            font.bold: true
                        }
                    }

                    MouseArea {
                        id: syncMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        enabled: !(typeof AudioEngine !== "undefined" && AudioEngine.packages && AudioEngine.packages.isSyncing)
                        onClicked: {
                            if (typeof AudioEngine !== "undefined" && AudioEngine.packages) {
                                AudioEngine.packages.syncRepositories()
                            }
                        }
                    }
                }

                // Botón Cerrar [✕]
                Rectangle {
                    Layout.preferredWidth: 28
                    Layout.preferredHeight: 28
                    radius: 4
                    color: closeMouse.containsMouse ? "#DC2626" : "#202430"

                    Text {
                        anchors.centerIn: parent
                        text: "✕"
                        color: "#E2E8F0"
                        font.pixelSize: 13
                        font.bold: true
                    }

                    MouseArea {
                        id: closeMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.close()
                    }
                }
            }

            // Banner informativo tras instalación
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: (root.lastInstalledNotice.length > 0) ? 32 : 0
                visible: root.lastInstalledNotice.length > 0
                radius: 4
                color: root.lastInstalledNotice.startsWith("❌") ? "#7F1D1D" : "#064E3B"
                border.color: root.lastInstalledNotice.startsWith("❌") ? "#DC2626" : "#059669"
                clip: true

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    spacing: 8

                    Text {
                        text: root.lastInstalledNotice
                        color: root.lastInstalledNotice.startsWith("❌") ? "#FCA5A5" : "#A7F3D0"
                        font.pixelSize: 11
                        font.bold: true
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }

                    Text {
                        text: "✕"
                        color: "#94A3B8"
                        font.pixelSize: 11
                        font.bold: true
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.lastInstalledNotice = ""
                        }
                    }
                }
            }

            // PANEL DESPLEGABLE DE REPOSITORIOS
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: root.showRepoConfig ? (typeof AudioEngine !== "undefined" && AudioEngine.packages && AudioEngine.packages.repositories.length > 0 ? 180 : 140) : 0
                visible: root.showRepoConfig
                clip: true
                color: "#1E222D"
                radius: 6
                border.color: "#343A4C"
                border.width: 1

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 6

                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: "Gestión de Repositorios ReaPack (URL index.xml):"
                            color: "#93C5FD"
                            font.pixelSize: 11
                            font.bold: true
                            Layout.fillWidth: true
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 32
                            color: "#13151C"
                            radius: 4
                            border.color: "#383E4F"
                            border.width: 1

                            TextInput {
                                id: newRepoInput
                                anchors.fill: parent
                                anchors.leftMargin: 8
                                anchors.rightMargin: 8
                                verticalAlignment: TextInput.AlignVCenter
                                color: "#F8FAFC"
                                font.pixelSize: 12
                                selectByMouse: true
                                clip: true
                                Text {
                                    anchors.fill: parent
                                    verticalAlignment: Text.AlignVCenter
                                    text: "https://ejemplo.com/repo/index.xml"
                                    color: "#64748B"
                                    font.pixelSize: 12
                                    visible: !newRepoInput.text && !newRepoInput.activeFocus
                                }
                            }
                        }

                        // Botón Añadir Repositorio
                        Rectangle {
                            Layout.preferredWidth: 110
                            Layout.preferredHeight: 32
                            radius: 4
                            color: addRepoMouse.containsMouse ? "#2563EB" : "#1D4ED8"

                            Text {
                                anchors.centerIn: parent
                                text: "+ Importar"
                                color: "#FFFFFF"
                                font.pixelSize: 11
                                font.bold: true
                            }

                            MouseArea {
                                id: addRepoMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (newRepoInput.text.trim().length > 0 && typeof AudioEngine !== "undefined" && AudioEngine.packages) {
                                        AudioEngine.packages.addRepository(newRepoInput.text.trim())
                                        newRepoInput.text = ""
                                        root.showRepoConfig = false
                                    }
                                }
                            }
                        }

                        // Botón Borrar todos los repositorios
                        Rectangle {
                            visible: typeof AudioEngine !== "undefined" && AudioEngine.packages && AudioEngine.packages.repositories.length > 0
                            Layout.preferredWidth: 90
                            Layout.preferredHeight: 32
                            radius: 4
                            color: clearRepoMouse.containsMouse ? "#7F1D1D" : "#451A1A"
                            border.color: "#991B1B"
                            border.width: 1

                            Text {
                                anchors.centerIn: parent
                                text: "Borrar todos"
                                color: "#FECACA"
                                font.pixelSize: 11
                            }

                            MouseArea {
                                id: clearRepoMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (typeof AudioEngine !== "undefined" && AudioEngine.packages) {
                                        AudioEngine.packages.clearRepositories()
                                    }
                                }
                            }
                        }
                    }

                    // Acciones de integración rápida
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Rectangle {
                            Layout.preferredHeight: 26
                            Layout.preferredWidth: impReaperBtnRow.implicitWidth + 14
                            radius: 3
                            color: impReaperMouse.containsMouse ? "#0D9488" : "#0F766E"

                            Row {
                                id: impReaperBtnRow
                                anchors.centerIn: parent
                                spacing: 5
                                Text { text: "📥"; font.pixelSize: 11 }
                                Text {
                                    text: "Importar de REAPER instalado"
                                    color: "#FFFFFF"
                                    font.pixelSize: 10
                                    font.bold: true
                                }
                            }

                            MouseArea {
                                id: impReaperMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (typeof AudioEngine !== "undefined" && AudioEngine.packages) {
                                        var count = AudioEngine.packages.importFromReaperInstallation("")
                                        if (count > 0) {
                                            root.lastInstalledNotice = "✅ ¡Se importaron " + count + " scripts de REAPER con éxito!"
                                        } else {
                                            root.lastInstalledNotice = "⚠️ No se encontró la carpeta de efectos de REAPER."
                                        }
                                        noticeTimer.restart()
                                        root.refreshPackages()
                                    }
                                }
                            }
                        }

                        Item { Layout.fillWidth: true }
                    }

                    // Lista de repositorios añadidos
                    Flickable {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        contentHeight: repoListCol.implicitHeight
                        clip: true
                        boundsBehavior: Flickable.StopAtBounds

                        Column {
                            id: repoListCol
                            width: parent.width
                            spacing: 4

                            Repeater {
                                model: (typeof AudioEngine !== "undefined" && AudioEngine.packages) ? AudioEngine.packages.repositories : []
                                delegate: Rectangle {
                                    width: repoListCol.width
                                    height: 24
                                    color: "#161922"
                                    radius: 3
                                    border.color: "#282C3A"
                                    border.width: 1

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 8
                                        anchors.rightMargin: 8
                                        spacing: 6

                                        Text {
                                            text: "🔗 " + modelData
                                            color: "#CBD5E1"
                                            font.pixelSize: 10
                                            elide: Text.ElideMiddle
                                            Layout.fillWidth: true
                                        }

                                        Text {
                                            text: "Eliminar"
                                            color: "#EF4444"
                                            font.pixelSize: 10
                                            font.bold: true
                                            MouseArea {
                                                anchors.fill: parent
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: {
                                                    if (typeof AudioEngine !== "undefined" && AudioEngine.packages) {
                                                        AudioEngine.packages.removeRepository(modelData)
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }

                            Text {
                                visible: typeof AudioEngine !== "undefined" && AudioEngine.packages && AudioEngine.packages.repositories.length === 0
                                text: "No hay ningún repositorio configurado. Introduce una URL arriba para importar paquetes."
                                color: "#64748B"
                                font.pixelSize: 10
                            }
                        }
                    }
                }
            }

            // 2. BARRA DE BÚSQUEDA Y FILTROS
            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                // Campo de búsqueda en vivo
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 34
                    color: "#1A1D26"
                    radius: 4
                    border.color: searchInput.activeFocus ? "#3B82F6" : "#2A2E3B"
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        spacing: 6

                        Text { text: "🔍"; font.pixelSize: 12; opacity: 0.7 }

                        TextInput {
                            id: searchInput
                            Layout.fillWidth: true
                            verticalAlignment: TextInput.AlignVCenter
                            color: "#FFFFFF"
                            font.pixelSize: 12
                            selectByMouse: true
                            clip: true

                            Text {
                                anchors.fill: parent
                                verticalAlignment: Text.AlignVCenter
                                text: "Buscar plugin por nombre, autor o descripción (ej: ReEQ, Loudener)..."
                                color: "#64748B"
                                font.pixelSize: 12
                                visible: !searchInput.text && !searchInput.activeFocus
                            }

                            onTextChanged: {
                                root.searchQuery = text
                                root.refreshPackages()
                            }
                        }

                        // Botón limpiar búsqueda
                        Text {
                            text: "✕"
                            color: "#94A3B8"
                            font.pixelSize: 12
                            visible: searchInput.text.length > 0
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: searchInput.text = ""
                            }
                        }
                    }
                }

                // Checkbox "Solo instalados"
                Rectangle {
                    Layout.preferredHeight: 34
                    Layout.preferredWidth: onlyInstRow.implicitWidth + 16
                    radius: 4
                    color: onlyInstMouse.containsMouse ? "#252936" : "#1B1E28"
                    border.color: root.onlyInstalled ? "#3B82F6" : "#2D3240"
                    border.width: 1

                    Row {
                        id: onlyInstRow
                        anchors.centerIn: parent
                        spacing: 6

                        Rectangle {
                            width: 14; height: 14; radius: 3
                            anchors.verticalCenter: parent.verticalCenter
                            color: root.onlyInstalled ? "#3B82F6" : "transparent"
                            border.color: root.onlyInstalled ? "#3B82F6" : "#64748B"
                            border.width: 1.2
                            Text {
                                anchors.centerIn: parent
                                text: "✓"
                                color: "#FFFFFF"
                                font.pixelSize: 10
                                font.bold: true
                                visible: root.onlyInstalled
                            }
                        }

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: "Solo instalados"
                            color: root.onlyInstalled ? "#FFFFFF" : "#94A3B8"
                            font.pixelSize: 11
                        }
                    }

                    MouseArea {
                        id: onlyInstMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            root.onlyInstalled = !root.onlyInstalled
                            root.refreshPackages()
                        }
                    }
                }

                // Botón Instalar todos
                Rectangle {
                    visible: root.packageList.length > 0 && !root.onlyInstalled
                    Layout.preferredHeight: 34
                    Layout.preferredWidth: instAllRow.implicitWidth + 16
                    radius: 4
                    color: instAllMouse.containsMouse ? "#2563EB" : "#1D4ED8"

                    Row {
                        id: instAllRow
                        anchors.centerIn: parent
                        spacing: 5
                        Text { text: "⚡"; font.pixelSize: 11 }
                        Text {
                            text: "Instalar todos (" + root.packageList.length + ")"
                            color: "#FFFFFF"
                            font.pixelSize: 11
                            font.bold: true
                        }
                    }

                    MouseArea {
                        id: instAllMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (typeof AudioEngine !== "undefined" && AudioEngine.packages) {
                                AudioEngine.packages.installAllPackages()
                                root.lastInstalledNotice = "⚡ Descargando e instalando paquetes..."
                                noticeTimer.restart()
                            }
                        }
                    }
                }
            }

            // 3. BARRA HORIZONTAL DE CATEGORÍAS
            Flickable {
                Layout.fillWidth: true
                Layout.preferredHeight: 30
                contentWidth: catRow.implicitWidth + 10
                contentHeight: 30
                flickableDirection: Flickable.HorizontalFlick
                boundsBehavior: Flickable.StopAtBounds
                clip: true

                Row {
                    id: catRow
                    spacing: 6
                    anchors.verticalCenter: parent.verticalCenter

                    Repeater {
                        model: (typeof AudioEngine !== "undefined" && AudioEngine.packages)
                               ? AudioEngine.packages.categories
                               : ["Todas"]

                        delegate: Rectangle {
                            height: 24
                            width: catText.implicitWidth + 16
                            radius: 12
                            color: (root.selectedCategory === modelData) ? "#3B82F6" : (catMouse.containsMouse ? "#2A2E3B" : "#1C1F2A")
                            border.color: (root.selectedCategory === modelData) ? "#60A5FA" : "#2C3140"
                            border.width: 1

                            Text {
                                id: catText
                                anchors.centerIn: parent
                                text: modelData
                                color: (root.selectedCategory === modelData) ? "#FFFFFF" : "#CBD5E1"
                                font.pixelSize: 11
                                font.bold: (root.selectedCategory === modelData)
                            }

                            MouseArea {
                                id: catMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    root.selectedCategory = modelData
                                    root.refreshPackages()
                                }
                            }
                        }
                    }
                }
            }

            // 4. LISTA DE PAQUETES JSFX
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "#12141A"
                radius: 6
                border.color: "#232632"
                border.width: 1
                clip: true

                ListView {
                    id: packageListView
                    anchors.fill: parent
                    anchors.margins: 6
                    spacing: 6
                    model: root.packageList
                    boundsBehavior: Flickable.StopAtBounds

                    ScrollBar.vertical: ScrollBar {
                        policy: ScrollBar.AsNeeded
                        width: 6
                    }

                    delegate: Rectangle {
                        width: packageListView.width - 12
                        height: 68
                        radius: 6
                        color: itemMouse.containsMouse ? "#1E222D" : "#171A22"
                        border.color: modelData.isInstalled ? "#22C55E" : (itemMouse.containsMouse ? "#383E4F" : "#222632")
                        border.width: modelData.isInstalled ? 1.2 : 1

                        MouseArea {
                            id: itemMouse
                            anchors.fill: parent
                            hoverEnabled: true
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            spacing: 10

                            // Icono representativo
                            Rectangle {
                                width: 34; height: 34; radius: 6
                                color: modelData.isInstalled ? "#14532D" : "#1E2433"
                                border.color: modelData.isInstalled ? "#22C55E" : "#3B82F6"
                                border.width: 1
                                Text {
                                    anchors.centerIn: parent
                                    text: modelData.isInstalled ? "✓" : "JS"
                                    color: modelData.isInstalled ? "#4ADE80" : "#60A5FA"
                                    font.bold: true
                                    font.pixelSize: 12
                                }
                            }

                            // Datos principales del plugin
                            Column {
                                Layout.fillWidth: true
                                spacing: 3

                                Row {
                                    spacing: 8
                                    Text {
                                        text: modelData.name
                                        color: "#FFFFFF"
                                        font.pixelSize: 13
                                        font.bold: true
                                        font.family: "Monospace, Consolas"
                                    }
                                    Rectangle {
                                        height: 16
                                        width: catBadge.implicitWidth + 8
                                        radius: 3
                                        color: "#1E293B"
                                        border.color: "#334155"
                                        border.width: 1
                                        Text {
                                            id: catBadge
                                            anchors.centerIn: parent
                                            text: modelData.category
                                            color: "#94A3B8"
                                            font.pixelSize: 9
                                            font.bold: true
                                        }
                                    }
                                    Text {
                                        text: "v" + modelData.version + " • " + modelData.author
                                        color: "#64748B"
                                        font.pixelSize: 11
                                    }
                                }

                                Text {
                                    text: modelData.desc ? modelData.desc : "Script JSFX para procesamiento de señal de audio"
                                    color: "#94A3B8"
                                    font.pixelSize: 11
                                    elide: Text.ElideRight
                                    width: packageListView.width - 250
                                }
                            }

                            // Botones de acción según estado
                            Row {
                                spacing: 6

                                // Botón Ver / Editar Código (si está instalado)
                                Rectangle {
                                    visible: modelData.isInstalled
                                    width: 70
                                    height: 28
                                    radius: 4
                                    color: codeBtnMouse.containsMouse ? "#334155" : "#1E293B"
                                    border.color: "#475569"
                                    border.width: 1

                                    Text {
                                        anchors.centerIn: parent
                                        text: "Ver código"
                                        color: "#E2E8F0"
                                        font.pixelSize: 10
                                    }

                                    MouseArea {
                                        id: codeBtnMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            if (typeof AudioEngine !== "undefined" && AudioEngine.packages) {
                                                var code = AudioEngine.packages.getPackageCode(modelData.id)
                                                root.openCodeRequested(code, modelData.name)
                                                root.close()
                                            }
                                        }
                                    }
                                }

                                // Botón Desinstalar (si está instalado)
                                Rectangle {
                                    visible: modelData.isInstalled
                                    width: 75
                                    height: 28
                                    radius: 4
                                    color: uninstMouse.containsMouse ? "#7F1D1D" : "#301A1E"
                                    border.color: "#991B1B"
                                    border.width: 1

                                    Text {
                                        anchors.centerIn: parent
                                        text: "Desinstalar"
                                        color: "#FCA5A5"
                                        font.pixelSize: 10
                                    }

                                    MouseArea {
                                        id: uninstMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            if (typeof AudioEngine !== "undefined" && AudioEngine.packages) {
                                                AudioEngine.packages.uninstallPackage(modelData.id)
                                            }
                                        }
                                    }
                                }

                                // Botón Instalar (si NO está instalado)
                                Rectangle {
                                    visible: !modelData.isInstalled
                                    width: 85
                                    height: 28
                                    radius: 4
                                    color: instMouse.containsMouse ? "#2563EB" : "#1D4ED8"

                                    Row {
                                        anchors.centerIn: parent
                                        spacing: 4
                                        Text { text: "⬇"; color: "#FFFFFF"; font.pixelSize: 10 }
                                        Text { text: "Instalar"; color: "#FFFFFF"; font.pixelSize: 11; font.bold: true }
                                    }

                                    MouseArea {
                                        id: instMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            if (typeof AudioEngine !== "undefined" && AudioEngine.packages) {
                                                AudioEngine.packages.installPackage(modelData.id)
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // Estado vacío
                    Item {
                        anchors.fill: parent
                        visible: packageListView.count === 0

                        Column {
                            anchors.centerIn: parent
                            spacing: 10
                            width: Math.min(parent.width - 40, 500)

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: (typeof AudioEngine !== "undefined" && AudioEngine.packages && AudioEngine.packages.isSyncing)
                                      ? "⏳"
                                      : ((typeof AudioEngine !== "undefined" && AudioEngine.packages && AudioEngine.packages.repositories.length === 0)
                                         ? "🌐"
                                         : ((typeof AudioEngine !== "undefined" && AudioEngine.packages && AudioEngine.packages.statusMessage && AudioEngine.packages.statusMessage.indexOf("Error") !== -1) ? "⚠️" : "📦"))
                                font.pixelSize: 36
                            }

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: {
                                    if (typeof AudioEngine !== "undefined" && AudioEngine.packages) {
                                        if (AudioEngine.packages.isSyncing) return AudioEngine.packages.statusMessage
                                        if (AudioEngine.packages.repositories.length === 0) {
                                            return "No hay repositorios importados"
                                        }
                                        if (AudioEngine.packages.statusMessage && AudioEngine.packages.statusMessage.indexOf("Error") !== -1) {
                                            return "Error al descargar catálogo"
                                        }
                                        if (AudioEngine.packages.totalPackagesCount === 0) {
                                            return "Catálogo vacío o no sincronizado"
                                        }
                                    }
                                    return "No se encontraron plugins con ese criterio"
                                }
                                color: "#FFFFFF"
                                font.pixelSize: 14
                                font.bold: true
                            }

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                horizontalAlignment: Text.AlignHCenter
                                text: {
                                    if (typeof AudioEngine !== "undefined" && AudioEngine.packages) {
                                        if (AudioEngine.packages.isSyncing) return "Por favor, espera un momento mientras se descarga el catálogo."
                                        if (AudioEngine.packages.repositories.length === 0) {
                                            return "NOVA Studio no precarga repositorios por defecto.\nPulsa en 'Repositorios' arriba y pega la URL de un repositorio ReaPack para importar sus plugins."
                                        }
                                        if (AudioEngine.packages.statusMessage && AudioEngine.packages.statusMessage.indexOf("Error") !== -1) {
                                            return AudioEngine.packages.statusMessage + "\nVerifica tu conexión y pulsa 'Sincronizar'."
                                        }
                                        if (AudioEngine.packages.totalPackagesCount === 0) {
                                            return "Pulsa en 'Sincronizar' para descargar el listado de plugins de los repositorios configurados."
                                        }
                                    }
                                    return "Prueba con otra búsqueda o selecciona 'Todas' las categorías."
                                }
                                color: (typeof AudioEngine !== "undefined" && AudioEngine.packages && AudioEngine.packages.statusMessage && AudioEngine.packages.statusMessage.indexOf("Error") !== -1) ? "#FCA5A5" : "#94A3B8"
                                font.pixelSize: 12
                                wrapMode: Text.WordWrap
                                width: parent.width
                            }

                            // Botones de acción en estado vacío
                            RowLayout {
                                anchors.horizontalCenter: parent.horizontalCenter
                                spacing: 10

                                Rectangle {
                                    visible: typeof AudioEngine !== "undefined" && AudioEngine.packages && !AudioEngine.packages.isSyncing
                                    width: 170
                                    height: 32
                                    radius: 4
                                    color: emptyBtnMouse.containsMouse ? "#2563EB" : "#1D4ED8"

                                    Text {
                                        anchors.centerIn: parent
                                        text: (typeof AudioEngine !== "undefined" && AudioEngine.packages && AudioEngine.packages.repositories.length === 0)
                                              ? "🌐 Configurar Repositorios"
                                              : "🔄 Sincronizar Catálogo"
                                        color: "#FFFFFF"
                                        font.pixelSize: 11
                                        font.bold: true
                                    }

                                    MouseArea {
                                        id: emptyBtnMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            if (typeof AudioEngine !== "undefined" && AudioEngine.packages) {
                                                if (AudioEngine.packages.repositories.length === 0) {
                                                    root.showRepoConfig = true
                                                } else {
                                                    AudioEngine.packages.syncRepositories()
                                                }
                                            }
                                        }
                                    }
                                }

                                Rectangle {
                                    visible: typeof AudioEngine !== "undefined" && AudioEngine.packages && !AudioEngine.packages.isSyncing
                                    width: 190
                                    height: 32
                                    radius: 4
                                    color: emptyReaperBtnMouse.containsMouse ? "#0D9488" : "#0F766E"

                                    Row {
                                        anchors.centerIn: parent
                                        spacing: 6
                                        Text { text: "📥"; font.pixelSize: 11 }
                                        Text {
                                            text: "Importar de REAPER instalado"
                                            color: "#FFFFFF"
                                            font.pixelSize: 11
                                            font.bold: true
                                        }
                                    }

                                    MouseArea {
                                        id: emptyReaperBtnMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            if (typeof AudioEngine !== "undefined" && AudioEngine.packages) {
                                                var c = AudioEngine.packages.importFromReaperInstallation("")
                                                if (c > 0) {
                                                    root.lastInstalledNotice = "✅ ¡Se importaron " + c + " scripts de REAPER con éxito!"
                                                } else {
                                                    root.lastInstalledNotice = "⚠️ No se encontró la carpeta de efectos de REAPER."
                                                }
                                                noticeTimer.restart()
                                                root.refreshPackages()
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
