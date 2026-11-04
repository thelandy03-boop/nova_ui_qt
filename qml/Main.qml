import QtQuick
import QtQuick.Window
import QtQuick.Controls
import "theme"
import "screens"
import "hub"
import "dialogs"

Window {
    id: mainWindow
    // En Android la UI se adapta a la pantalla y arranca en horizontal
    // (landscape): el manifest ya fuerza landscape; aquí hacemos que la
    // ventana ocupe toda la pantalla en vez de un tamaño fijo de escritorio.
    width: (Qt.platform.os === "android") ? Screen.width : 1280
    height: (Qt.platform.os === "android") ? Screen.height : 800
    visibility: (Qt.platform.os === "android") ? Window.FullScreen : Window.Windowed
    visible: true
    title: "NOVA Studio v9.8 (Qt 6) - " + AudioEngine.currentProjectName + (AudioEngine.isDirty ? " *" : "")
    color: "#121212"

    // ── INTERCEPTAR EVENTO DE CIERRE DE LA VENTANA ──
    onClosing: (close) => {
        if (AudioEngine.isDirty) {
            close.accepted = false // Detener el cierre inmediato
            unsavedDialog.closeActionType = "exitApp"
            unsavedDialog.visible = true // Mostrar el diálogo modal
        } else {
            AudioEngine.closeProject()
        }
    }

    // 🔙 Gesto / Tecla "Atrás" de Android y Escape de escritorio
    Shortcut {
        sequence: "Back"
        context: Qt.ApplicationShortcut
        onActivated: handleBackNavigation()
    }

    Item {
        anchors.fill: parent
        focus: true
        Keys.onBackPressed: (event) => {
            if (handleBackNavigation()) {
                event.accepted = true;
            }
        }
        Keys.onReleased: (event) => {
            if (event.key === Qt.Key_Back) {
                if (handleBackNavigation()) {
                    event.accepted = true;
                }
            }
        }
    }

    function handleBackNavigation() {
        // 1. Cerrar diálogo de novedades si está abierto
        if (whatsNewDialog && whatsNewDialog.visible) {
            whatsNewDialog.close();
            return true;
        }
        // 2. Cerrar diálogo modal de cambios sin guardar si está abierto
        if (unsavedDialog && unsavedDialog.visible) {
            unsavedDialog.visible = false;
            return true;
        }
        // 3. Si estamos en el workspace con un proyecto abierto, volver al Hub
        if (rootStack.depth > 1) {
            if (AudioEngine.isDirty) {
                unsavedDialog.closeActionType = "showHub";
                unsavedDialog.visible = true;
            } else {
                AudioEngine.closeProject();
                rootStack.pop();
            }
            return true;
        }
        return false;
    }

    // Navegación principal del DAW (StackView de Alto Rendimiento)
    StackView {
        id: rootStack
        anchors.fill: parent
        anchors.leftMargin: (Qt.platform.os === "android") ? 12 : 0
        anchors.rightMargin: (Qt.platform.os === "android") ? 12 : 0
        
        initialItem: (AudioEngine.recentProjects && AudioEngine.recentProjects.length > 0) ? hubComponent : workspaceComponent

        pushEnter: Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 150 } }
        pushExit:  Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 150 } }
        popEnter:  Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 150 } }
        popExit:   Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 150 } }
    }

    // ── COMPONENTE: ÁREA DE TRABAJO (WORKSPACE DAW) ──
    Component {
        id: workspaceComponent
        WorkspaceScreen {
            onGoBack: {
                if (AudioEngine.isDirty) {
                    unsavedDialog.closeActionType = "showHub"
                    unsavedDialog.visible = true
                } else {
                    AudioEngine.closeProject()
                    if (rootStack.depth > 1) {
                        rootStack.pop()
                    } else {
                        rootStack.push(hubComponent)
                    }
                }
            }
        }
    }

    // ── COMPONENTE: PANTALLA DE INICIO / PROYECTOS RECIENTES (PROJECT HUB) ──
    Component {
        id: hubComponent
        NovaProjectHub {
            onProjectOpened: {
                if (rootStack.depth > 1) {
                    rootStack.pop()
                } else {
                    rootStack.push(workspaceComponent)
                }
            }
        }
    }

    // Escuchar cuando la app recupera el foco para refrescar el permiso de instalación
    Connections {
        target: Qt.application
        function onStateChanged() {
            if (Qt.application.state === Qt.ApplicationActive && typeof UpdateChecker !== "undefined") {
                UpdateChecker.checkInstallPermission()
                // Volver a mirar si hay una version nueva al recuperar el
                // foco. El comprobador limita la frecuencia (30 min), asi que
                // esto no genera una peticion de red en cada cambio de estado.
                UpdateChecker.check()
            }
        }
    }

    // ── OVERLAY OBLIGATORIO: ACTUALIZACIÓN INTEGRADA (IN-APP DOWNLOADER & INSTALLER) ──
    Rectangle {
        id: updateOverlay
        anchors.fill: parent
        z: 9999
        color: "#E60B0C11"
        visible: (typeof UpdateChecker !== "undefined") && UpdateChecker.updateRequired

        // Bloquear toques al fondo
        MouseArea {
            anchors.fill: parent
            preventStealing: true
        }

        Rectangle {
            anchors.centerIn: parent
            width: Math.min(parent.width * 0.9, 440)
            height: updateCol.implicitHeight + 48
            radius: 16
            color: "#181920"
            border.color: "#2C2D3B"
            border.width: 1.5

            Column {
                id: updateCol
                anchors.centerIn: parent
                width: parent.width - 48
                spacing: 14

                // Badge cabecera
                Row {
                    spacing: 8
                    anchors.horizontalCenter: parent.horizontalCenter
                    Rectangle {
                        height: 24
                        width: badgeText.implicitWidth + 16
                        radius: 12
                        color: "#1E293B"
                        border.color: "#3B82F6"
                        border.width: 1
                        Text {
                            id: badgeText
                            anchors.centerIn: parent
                            text: "✨ ACTUALIZACIÓN DISPONIBLE"
                            color: "#60A5FA"
                            font.pixelSize: 10
                            font.bold: true
                            font.letterSpacing: 0.5
                        }
                    }
                }

                // Títulos
                Column {
                    width: parent.width
                    spacing: 4
                    Text {
                        text: "NOVA Studio v" + (typeof UpdateChecker !== "undefined" ? UpdateChecker.latestVersion : "")
                        color: "#FFFFFF"
                        font.pixelSize: 20
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        width: parent.width
                    }
                    Text {
                        text: "Versión actual instalada: v" + (typeof UpdateChecker !== "undefined" ? UpdateChecker.currentVersion : "")
                        color: "#94A3B8"
                        font.pixelSize: 12
                        horizontalAlignment: Text.AlignHCenter
                        width: parent.width
                    }
                }

                // Explicación
                Text {
                    text: "Se descargará e instalará directamente sin salir de la app. Tus proyectos, pistas de audio y configuraciones permanecerán completamente seguros."
                    color: "#CBD5E1"
                    font.pixelSize: 13
                    lineHeight: 1.2
                    wrapMode: Text.WordWrap
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                }

                // Zona dinámica: 1. Descargando
                Column {
                    width: parent.width
                    spacing: 8
                    visible: (typeof UpdateChecker !== "undefined") && UpdateChecker.downloading

                    Item {
                        width: parent.width
                        height: 20
                        Text {
                            text: "Descargando actualización..."
                            color: "#93C5FD"
                            font.pixelSize: 12
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Text {
                            text: (typeof UpdateChecker !== "undefined" ? UpdateChecker.downloadProgress : 0) + "%"
                            color: "#FFFFFF"
                            font.bold: true
                            font.pixelSize: 13
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }

                    Rectangle {
                        width: parent.width
                        height: 8
                        radius: 4
                        color: "#252836"

                        Rectangle {
                            height: parent.height
                            radius: 4
                            color: "#3B82F6"
                            width: Math.max(4, parent.width * ((typeof UpdateChecker !== "undefined" ? Math.min(100, Math.max(0, UpdateChecker.downloadProgress)) : 0) / 100))
                            Behavior on width {
                                NumberAnimation { duration: 120 }
                            }
                        }
                    }

                    Text {
                        text: "Descarga en segundo plano. No cierres la app."
                        color: "#64748B"
                        font.pixelSize: 11
                        font.italic: true
                        horizontalAlignment: Text.AlignHCenter
                        width: parent.width
                    }
                }

                // Zona dinámica: 2. Error en descarga o instalación
                Rectangle {
                    width: parent.width
                    height: errorText.implicitHeight + 16
                    radius: 8
                    color: "#2B1113"
                    border.color: "#EF4444"
                    border.width: 1
                    visible: (typeof UpdateChecker !== "undefined") && !UpdateChecker.downloading && UpdateChecker.downloadError !== ""

                    Text {
                        id: errorText
                        anchors.centerIn: parent
                        width: parent.width - 20
                        text: (typeof UpdateChecker !== "undefined" ? UpdateChecker.downloadError : "")
                        color: "#FCA5A5"
                        font.pixelSize: 12
                        wrapMode: Text.WordWrap
                        horizontalAlignment: Text.AlignHCenter
                    }
                }

                // Zona dinámica: 3. Botones de acción
                Column {
                    width: parent.width
                    spacing: 10
                    visible: (typeof UpdateChecker !== "undefined") && !UpdateChecker.downloading

                    // Si el APK ya está descargado y falta permiso
                    Column {
                        width: parent.width
                        spacing: 8
                        visible: (typeof UpdateChecker !== "undefined") && UpdateChecker.apkReady && !UpdateChecker.installPermissionGranted

                        Rectangle {
                            width: parent.width
                            height: 44
                            radius: 10
                            color: "#D97706"
                            Text {
                                anchors.centerIn: parent
                                text: "⚙️ Activar Permiso de Instalación"
                                color: "#FFFFFF"
                                font.pixelSize: 14
                                font.bold: true
                            }
                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    if (typeof AudioEngine !== "undefined") AudioEngine.hapticFeedback()
                                    UpdateChecker.requestInstallPermission()
                                }
                            }
                        }

                        Rectangle {
                            width: parent.width
                            height: 44
                            radius: 10
                            color: "#1E293B"
                            border.color: "#3B82F6"
                            border.width: 1
                            Text {
                                anchors.centerIn: parent
                                text: "🚀 Instalar una vez concedido"
                                color: "#93C5FD"
                                font.pixelSize: 14
                                font.bold: true
                            }
                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    if (typeof AudioEngine !== "undefined") AudioEngine.hapticFeedback()
                                    UpdateChecker.installApk()
                                }
                            }
                        }
                    }

                    // Si el APK ya está listo y tiene permiso
                    Rectangle {
                        width: parent.width
                        height: 46
                        radius: 10
                        color: "#10B981"
                        visible: (typeof UpdateChecker !== "undefined") && UpdateChecker.apkReady && UpdateChecker.installPermissionGranted

                        Text {
                            anchors.centerIn: parent
                            text: "🚀 Instalar Actualización (1 toque)"
                            color: "#FFFFFF"
                            font.pixelSize: 15
                            font.bold: true
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                if (typeof AudioEngine !== "undefined") AudioEngine.hapticFeedback()
                                UpdateChecker.installApk()
                            }
                        }
                    }

                    // Si aún no está descargado
                    Rectangle {
                        width: parent.width
                        height: 46
                        radius: 10
                        color: "#2563EB"
                        visible: (typeof UpdateChecker !== "undefined") && !UpdateChecker.apkReady

                        Text {
                            anchors.centerIn: parent
                            text: "⚡ Descargar e Instalar Ahora"
                            color: "#FFFFFF"
                            font.pixelSize: 15
                            font.bold: true
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                if (typeof AudioEngine !== "undefined") AudioEngine.hapticFeedback()
                                UpdateChecker.downloadAndInstall()
                            }
                        }
                    }

                    // Enlace secundario: navegador
                    Text {
                        text: "¿Problemas? Descargar archivo desde el navegador"
                        color: "#64748B"
                        font.pixelSize: 11
                        font.underline: true
                        horizontalAlignment: Text.AlignHCenter
                        anchors.horizontalCenter: parent.horizontalCenter
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (typeof UpdateChecker !== "undefined") UpdateChecker.openDownloadPage()
                            }
                        }
                    }
                }
            }
        }
    }

    // ── OVERLAY MODAL: DIÁLOGO DE CAMBIOS SIN GUARDAR ──
    NovaUnsavedDialog {
        id: unsavedDialog
        anchors.fill: parent
        visible: false
        
        property string closeActionType: "exitApp"

        onSaveAndExit: {
            AudioEngine.saveProject()
            AudioEngine.closeProject()
            executeClosingAction()
        }

        onDiscardAndExit: {
            // 🗑️ Descartar cambios, eliminar la carpeta física si nunca fue guardado, y cerrar la app completamente
            AudioEngine.discardProject()
            AudioEngine.quitApplication()
        }

        onCancelClose: {
            unsavedDialog.visible = false
            closeActionType = "exitApp"
        }

        function executeClosingAction() {
            unsavedDialog.visible = false
            
            if (closeActionType === "exitApp") {
                Qt.quit() // Salida segura
            } else if (closeActionType === "showHub") {
                if (rootStack.depth > 1) {
                    rootStack.pop()
                } else {
                    rootStack.push(hubComponent)
                }
            }
        }
    }

    // ── OVERLAY MODAL: NOVEDADES DE LA ACTUALIZACIÓN ──
    NovaWhatsNewDialog {
        id: whatsNewDialog
        anchors.fill: parent
    }

    // ── OVERLAY MODAL: BIENVENIDA Y AVISO DE DESARROLLO (PRIMERA INSTALACIÓN) ──
    NovaWelcomeDialog {
        id: welcomeDialog
        anchors.fill: parent
    }

    // 📂 Alternar Hub de Proyectos Recientes: Ctrl + H
    Shortcut {
        sequence: "Ctrl+H"
        context: Qt.ApplicationShortcut
        onActivated: {
            if (rootStack.currentItem !== hubComponent) {
                if (AudioEngine.isDirty) {
                    unsavedDialog.closeActionType = "showHub"
                    unsavedDialog.visible = true
                } else {
                    AudioEngine.closeProject()
                    if (rootStack.depth > 1 && rootStack.currentItem === workspaceComponent) {
                        rootStack.push(hubComponent)
                    } else {
                        rootStack.push(hubComponent)
                    }
                }
            }
        }
    }

    // 💾 Guardado Rápido de Proyecto: Ctrl + S
    Shortcut {
        sequence: "StandardKey.Save"
        context: Qt.ApplicationShortcut
        onActivated: {
            if (AudioEngine.saveProject()) {
                console.log("💾 [Shortcut] Proyecto guardado exitosamente.")
            }
        }
    }

    // ✂️ Cortar Clip en la Posición del Cabezal (Playhead): Tecla S
    Shortcut {
        sequence: "S"
        context: Qt.ApplicationShortcut
        onActivated: {
            if (AudioEngine.splitAtPlayhead()) {
                console.log("✂️ [Shortcut] Clip dividido en el Playhead.")
            }
        }
    }

    // 🗑️ Borrado de Clip Seleccionado con Teclado: Delete o Backspace
    Shortcut {
        sequences: ["Delete", "Backspace"]
        context: Qt.ApplicationShortcut
        onActivated: {
            var selectedIdx = AudioEngine.regions.selectedRegionIndex;
            if (selectedIdx >= 0) {
                console.log("🗑️ [Shortcut] Eliminando clip seleccionado en índice: " + selectedIdx);
                AudioEngine.regions.removeRegion(selectedIdx);
            }
        }
    }
}