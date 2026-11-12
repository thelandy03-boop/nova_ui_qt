import QtQuick
import QtQuick.Controls
import NovaStudio 1.0

/**
 * Área de edición principal de código EEL2 / JSFX.
 * Incluye Gutter de números de línea de alto rendimiento,
 * soporte de desplazamiento táctil fluido para Android (Flickable con pressDelay),
 * barra de símbolos de acceso rápido y botón para ocultar el teclado virtual.
 */
Item {
    id: root

    property alias text: codeTextEdit.text
    property alias cursorPosition: codeTextEdit.cursorPosition
    property alias lineCount: codeTextEdit.lineCount

    property int currentLine: 1
    property int currentCol: 1
    property string currentSection: "@init"
    property bool isInternalLoading: false
    property string cachedLineNumbers: "1"

    signal textModified()
    signal cursorMoved()

    function updateLineNumbers() {
        var count = Math.max(1, codeTextEdit.lineCount)
        var lines = []
        for (var i = 1; i <= count; ++i) {
            lines.push(i)
        }
        root.cachedLineNumbers = lines.join("\n")
    }

    function loadContent(newText) {
        root.isInternalLoading = true
        codeTextEdit.text = newText
        root.isInternalLoading = false
        updateCursorPosition()
        updateLineNumbers()
    }

    function searchInCode(term) {
        if (!term || term.length === 0) return
        var idx = codeTextEdit.text.indexOf(term, codeTextEdit.cursorPosition)
        if (idx === -1) idx = codeTextEdit.text.indexOf(term, 0)
        if (idx !== -1) {
            codeTextEdit.cursorPosition = idx
            codeTextEdit.select(idx, idx + term.length)
        }
    }

    function findNext(term) {
        if (!term || term.length === 0) return
        var start = codeTextEdit.cursorPosition + 1
        var idx = codeTextEdit.text.indexOf(term, start)
        if (idx === -1) idx = codeTextEdit.text.indexOf(term, 0)
        if (idx !== -1) {
            codeTextEdit.cursorPosition = idx
            codeTextEdit.select(idx, idx + term.length)
        }
    }

    function findPrev(term) {
        if (!term || term.length === 0) return
        var start = Math.max(0, codeTextEdit.cursorPosition - 1)
        var idx = codeTextEdit.text.lastIndexOf(term, start)
        if (idx === -1) idx = codeTextEdit.text.lastIndexOf(term)
        if (idx !== -1) {
            codeTextEdit.cursorPosition = idx
            codeTextEdit.select(idx, idx + term.length)
        }
    }

    function forceFocus() {
        codeTextEdit.forceActiveFocus()
    }

    function pasteFromClipboard() {
        codeTextEdit.forceActiveFocus()
        codeTextEdit.paste()
    }

    function insertSymbol(sym) {
        codeTextEdit.forceActiveFocus()
        var pos = codeTextEdit.cursorPosition
        var t = codeTextEdit.text
        codeTextEdit.text = t.substring(0, pos) + sym + t.substring(pos)
        codeTextEdit.cursorPosition = pos + sym.length
        root.textModified()
    }

    function updateCursorPosition() {
        var pos = codeTextEdit.cursorPosition
        var sub = codeTextEdit.text.substring(0, pos)
        var lines = sub.split("\n")
        root.currentLine = lines.length
        root.currentCol = lines[lines.length - 1].length + 1

        var detectedSection = "desc"
        for (var i = lines.length - 1; i >= 0; --i) {
            var trimmed = lines[i].trim()
            if (trimmed.indexOf("@") === 0) {
                detectedSection = trimmed.split(/\s+/)[0]
                break
            }
        }
        root.currentSection = detectedSection
        root.cursorMoved()
    }

    // CONTENEDOR FLUIDO FLICKABLE (OPTIMIZADO PARA TÁCTIL EN ANDROID Y RATÓN EN ESCRITORIO)
    Flickable {
        id: editorFlickable
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: (Qt.platform.os === "android") ? accessoryBar.top : parent.bottom
        clip: true
        contentWidth: Math.max(width, codeRow.width)
        contentHeight: Math.max(height, codeRow.height)
        boundsBehavior: Flickable.StopAtBounds
        // 🛡️ pressDelay en Android: si el usuario arrastra el dedo para hacer scroll,
        // el Flickable captura el gesto de inmediato antes de que TextEdit abra el teclado
        pressDelay: (Qt.platform.os === "android" ? 120 : 0)
        flickDeceleration: 1500
        maximumFlickVelocity: 3000
        flickableDirection: Flickable.AutoFlickDirection

        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AsNeeded
            width: (Qt.platform.os === "android") ? 6 : 8
        }
        ScrollBar.horizontal: ScrollBar {
            policy: ScrollBar.AsNeeded
            height: (Qt.platform.os === "android") ? 6 : 8
        }

        Row {
            id: codeRow
            spacing: 0
            width: Math.max(editorFlickable.width - 20, codeEditorContainer.width + gutter.width + 30)
            height: Math.max(editorFlickable.height, codeTextEdit.contentHeight + 40)

            // GUTTER: NÚMEROS DE LÍNEA ULTRALIGEROS
            Rectangle {
                id: gutter
                width: (Qt.platform.os === "android") ? 38 : 46
                height: parent.height
                color: "#181A20"

                // Indicador de línea actual activa
                Rectangle {
                    x: 0
                    y: codeTextEdit.y + codeTextEdit.cursorRectangle.y
                    width: parent.width
                    height: codeTextEdit.cursorRectangle.height > 0 ? codeTextEdit.cursorRectangle.height : 18
                    color: "#252832"
                    visible: codeTextEdit.activeFocus
                }

                // Números de línea sincronizados con la misma fuente en un solo elemento Text
                Text {
                    id: gutterNumbersText
                    anchors.top: parent.top
                    anchors.topMargin: 8
                    anchors.right: parent.right
                    anchors.rightMargin: 8
                    color: "#555C6E"
                    font.family: codeTextEdit.font.family
                    font.pixelSize: codeTextEdit.font.pixelSize
                    horizontalAlignment: Text.AlignRight
                    text: root.cachedLineNumbers
                }

                // Línea divisoria vertical del gutter
                Rectangle {
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: 1
                    color: "#22252D"
                }
            }

            // ZONA EDITABLE DEL CÓDIGO
            Item {
                id: codeEditorContainer
                width: Math.max(editorFlickable.width - gutter.width - 20, codeTextEdit.contentWidth + 40)
                height: parent.height

                // Resaltado de línea activa estilo VS Code
                Rectangle {
                    x: 0
                    y: codeTextEdit.y + codeTextEdit.cursorRectangle.y
                    width: parent.width
                    height: codeTextEdit.cursorRectangle.height > 0 ? codeTextEdit.cursorRectangle.height : 18
                    color: "#20232B"
                    border.color: "#282C37"
                    border.width: 1
                    visible: codeTextEdit.activeFocus
                }

                // TextEdit con renderizado nativo nítido
                TextEdit {
                    id: codeTextEdit
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.topMargin: 8
                    anchors.leftMargin: 6
                    width: parent.width - 12
                    height: Math.max(contentHeight + 40, editorFlickable.height)
                    color: "#D4D4D4"
                    selectedTextColor: "#FFFFFF"
                    selectionColor: "#264F78"
                    font.family: (Qt.platform.os === "android") ? "monospace" : "JetBrains Mono, Consolas, 'Courier New', Monospace"
                    font.pixelSize: (Qt.platform.os === "android") ? 13 : 13
                    inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                    wrapMode: TextEdit.NoWrap
                    tabStopDistance: 32
                    cursorVisible: activeFocus
                    // 🛡️ En Android selectByMouse debe ser false para que el arrastre con el dedo
                    // realice scroll en vez de seleccionar todo el texto de la pantalla
                    selectByMouse: (Qt.platform.os !== "android")
                    mouseSelectionMode: TextEdit.SelectCharacters
                    persistentSelection: true

                    // Cursor personalizado de 2px estilo VS Code
                    cursorDelegate: Rectangle {
                        width: 2
                        color: "#AEAFAD"
                        visible: codeTextEdit.activeFocus

                        SequentialAnimation on opacity {
                            loops: Animation.Infinite
                            running: codeTextEdit.activeFocus
                            NumberAnimation { to: 1.0; duration: 100 }
                            PauseAnimation { duration: 400 }
                            NumberAnimation { to: 0.0; duration: 150 }
                            PauseAnimation { duration: 300 }
                        }
                    }

                    onCursorPositionChanged: {
                        root.updateCursorPosition()
                    }

                    onLineCountChanged: {
                        root.updateLineNumbers()
                    }

                    onTextChanged: {
                        if (!root.isInternalLoading) {
                            root.textModified()
                        }
                        root.updateCursorPosition()
                        root.updateLineNumbers()
                    }

                    Component.onCompleted: {
                        root.updateCursorPosition()
                        root.updateLineNumbers()
                    }
                }

                // Resaltador de sintaxis EEL2 / JSFX nativo en tiempo real
                JsfxHighlighter {
                    textDocument: codeTextEdit.textDocument
                }
            }
        }
    }

    // 📱 BARRA DE ACCESORIOS DE PROGRAMACIÓN PARA ANDROID
    // Facilita insertar símbolos clave sin tener que alternar teclados en el móvil,
    // e incluye botón directo para cerrar/ocultar el teclado virtual.
    Rectangle {
        id: accessoryBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: (Qt.platform.os === "android") ? 36 : 0
        visible: (Qt.platform.os === "android")
        color: "#1E2028"
        border.color: "#282C37"
        border.width: 1
        z: 50

        Flickable {
            anchors.fill: parent
            anchors.leftMargin: 4
            anchors.rightMargin: 4
            contentWidth: symbolRow.implicitWidth + 20
            contentHeight: parent.height
            flickableDirection: Flickable.HorizontalFlick
            boundsBehavior: Flickable.StopAtBounds
            clip: true

            Row {
                id: symbolRow
                anchors.verticalCenter: parent.verticalCenter
                spacing: 4

                // Botón para cerrar / ocultar teclado virtual en Android
                Rectangle {
                    width: 48
                    height: 28
                    radius: 4
                    color: hideKeyMouse.pressed ? "#EF4444" : "#2D3748"
                    border.color: "#4A5568"
                    border.width: 1

                    Row {
                        anchors.centerIn: parent
                        spacing: 2
                        Text { text: "⌨▾"; color: "#60A5FA"; font.pixelSize: 12; font.bold: true }
                    }

                    MouseArea {
                        id: hideKeyMouse
                        anchors.fill: parent
                        onClicked: {
                            Qt.inputMethod.hide()
                            codeTextEdit.focus = false
                        }
                    }
                }

                Repeater {
                    model: [
                        { label: "Tab", insert: "    " },
                        { label: "( )", insert: "()" },
                        { label: "[ ]", insert: "[]" },
                        { label: "{ }", insert: "{}" },
                        { label: ";", insert: ";" },
                        { label: "=", insert: " = " },
                        { label: "+", insert: " + " },
                        { label: "-", insert: " - " },
                        { label: "*", insert: " * " },
                        { label: "/", insert: " / " },
                        { label: "@", insert: "@" },
                        { label: "_", insert: "_" },
                        { label: "$", insert: "$" },
                        { label: "\"", insert: "\"" },
                        { label: ",", insert: ", " },
                        { label: "@init", insert: "\n@init\n" },
                        { label: "@sample", insert: "\n@sample\n" },
                        { label: "@block", insert: "\n@block\n" },
                        { label: "@slider", insert: "\n@slider\n" }
                    ]
                    delegate: Rectangle {
                        width: Math.max(30, symText.implicitWidth + 12)
                        height: 28
                        radius: 4
                        color: symMouse.pressed ? "#3B82F6" : "#282C37"
                        border.color: "#383E4D"
                        border.width: 1

                        Text {
                            id: symText
                            anchors.centerIn: parent
                            text: modelData.label
                            color: "#E2E8F0"
                            font.family: "Monospace"
                            font.pixelSize: 12
                            font.bold: true
                        }

                        MouseArea {
                            id: symMouse
                            anchors.fill: parent
                            onClicked: {
                                root.insertSymbol(modelData.insert)
                            }
                        }
                    }
                }
            }
        }
    }
}
